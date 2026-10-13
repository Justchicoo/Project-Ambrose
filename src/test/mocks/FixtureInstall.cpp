/*
 * Project Ambrose by Imjustchico
 * Writes the fixture install: the message archive from the project's login and base message fixtures, and a type dump that starts from the character select fixture classes and adds every class, field, enum and value type the built-in typed views name, appending a missing field to a view's class with the next property id and refusing a class other classes derive from, since their inherited ids would no longer line up.
 */

#include "FixtureInstall.h"
#include "BaseMessageFixtures.h"
#include "CharacterTypeFixtures.h"
#include "ConfigMgr.h"
#include "KiwadBuilder.h"
#include "LoginMessageFixtures.h"
#include "MessageRegistry.h"
#include "PropertyValue.h"
#include "StringHash.h"
#include "TypeDumpLoader.h"
#include "TypedView.h"
#include "ViewDefinition.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <fstream>
#include <set>
#include <system_error>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 WireFlags = 0x1F;

    Json* FindClass(Json& classes, std::string const& name)
    {
        auto const found = classes.find(std::to_string(StringHash::KiStringHash(name)));
        return found == classes.end() ? nullptr : &*found;
    }

    Json& EnsureClass(Json& classes, std::string const& name, bool propertyClass)
    {
        if (Json* const found = FindClass(classes, name))
            return *found;
        CharacterTypeFixtures::Detail::AddClass(classes, name, propertyClass ? Json::array({ "PropertyClass" }) : Json::array(), {});
        return *FindClass(classes, name);
    }

    bool IsDerivedFrom(Json const& classes, std::string const& name)
    {
        for (auto const& [key, type] : classes.items())
            for (Json const& base : type["bases"])
                if (base.get<std::string>() == name || "class " + base.get<std::string>() == name)
                    return true;
        return false;
    }

    bool WriteBytes(std::filesystem::path const& path, std::string_view bytes, std::string& error)
    {
        std::error_code code;
        std::filesystem::create_directories(path.parent_path(), code);
        if (code)
        {
            error = fmt::format("cannot create {}: {}", ConfigMgr::PathToUtf8(path.parent_path()), code.message());
            return false;
        }
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream || !stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size())))
        {
            error = fmt::format("cannot write {}", ConfigMgr::PathToUtf8(path));
            return false;
        }
        return true;
    }
}

std::vector<uint8> FixtureInstall::MessageArchive()
{
    return KiwadBuilder(1)
        .Add("FixtureLoginMessages.xml", LoginMessageFixtures::LoginXml, false)
        .Add("FixtureSystemMessages.xml", BaseMessageFixtures::SystemXml, false)
        .Add("FixtureExtendedBaseMessages.xml", BaseMessageFixtures::ExtendedBaseXml, false)
        .Build();
}

std::optional<std::string> FixtureInstall::TypeDump(std::string& error)
{
    Json dump = Json::parse(CharacterTypeFixtures::Dump());
    Json& classes = dump["classes"];
    std::vector<ViewDefinition const*> const views = sTypedViewRegistry.GetViews();
    std::set<std::string> viewClasses;
    for (ViewDefinition const* view : views)
        viewClasses.emplace(view->ClassName);

    for (ViewDefinition const* view : views)
    {
        std::string const className(view->ClassName);
        EnsureClass(classes, className, true);
        for (ViewField const& field : view->Fields)
        {
            std::string const typeName(field.TypeName);
            std::string const name(field.Name);
            bool const list = field.StorageIndex == PropertyValue::IndexOf<PropertyValue::List>();
            bool const object = field.StorageIndex == PropertyValue::IndexOf<PropertyObjectPtr>();
            std::string const target = TypeDumpLoader::Canonicalize(typeName);
            if (target.starts_with("enum "))
                EnsureClass(classes, target, false);
            else if (target.starts_with("class ") || target.starts_with("struct "))
                EnsureClass(classes, target, object || list || target != typeName || viewClasses.contains(target));

            Json& properties = (*FindClass(classes, className))["properties"];
            if (properties.contains(name))
            {
                if (properties[name]["hash"].get<uint32>() != field.Hash)
                {
                    error = fmt::format("view {} reads {} {} of {}, but the fixture classes give that property another type", view->Name, typeName, name, className);
                    return std::nullopt;
                }
                continue;
            }
            if (IsDerivedFrom(classes, className))
            {
                error = fmt::format("view {} needs {} added to {}, which other fixture classes derive from", view->Name, name, className);
                return std::nullopt;
            }
            properties[name] = CharacterTypeFixtures::Detail::Property(typeName, name, static_cast<uint32>(properties.size()), WireFlags, list ? "List" : "Static");
        }
    }
    return dump.dump();
}

std::filesystem::path FixtureInstall::TypeDumpPath(std::filesystem::path const& root)
{
    return root / ConfigMgr::PathFromUtf8(fmt::format("{}.json", Revision));
}

bool FixtureInstall::Write(std::filesystem::path const& root, std::string& error)
{
    std::optional<std::string> const dump = TypeDump(error);
    if (!dump)
        return false;
    std::vector<uint8> const archive = MessageArchive();
    return WriteBytes(root / "Bin" / "revision.dat", fmt::format("{}\n", Revision), error)
        && WriteBytes(MessageRegistry::GetClientArchivePath(root), std::string_view(reinterpret_cast<char const*>(archive.data()), archive.size()), error)
        && WriteBytes(TypeDumpPath(root), *dump, error);
}
