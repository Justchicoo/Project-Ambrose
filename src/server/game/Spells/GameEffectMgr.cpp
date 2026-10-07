/*
 * Project Ambrose by Imjustchico
 * Reads every GameEffectData/ entry of Root.wad ending in .xml in name order as a BINd GameEffectTemplateList, keeps each template's name, class, category, duration, whether it is public and whether it rides on the pet, warns of a template with no name or a name read before and leaves it out, refuses a file that does not read or is not a template list, finds a template's effect class by its name without Template, or with GameEffect in its place, and logs how many templates the set swapped in holds, how many of them make an effect and how long they took.
 */

#include "GameEffectMgr.h"
#include "BindFile.h"
#include "ConfigMgr.h"
#include "GameEffectHolder.h"
#include "KiwadArchive.h"
#include "Log.h"
#include "ReloadMgr.h"
#include "StringUtil.h"
#include "TypeRegistry.h"

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <utility>

namespace
{
    constexpr char const* EffectLog = "server.loading";
    constexpr std::string_view TemplateSuffix = "Template";
    constexpr std::string_view EffectSuffix = "GameEffect";

    bool IsEffectFile(std::string_view name)
    {
        std::size_t const folder = GameEffectMgr::Folder.size();
        std::size_t const extension = GameEffectMgr::Extension.size();
        return name.size() > folder + extension && Ambrose::EqualsIgnoreCase(name.substr(0, folder), GameEffectMgr::Folder)
            && Ambrose::EqualsIgnoreCase(name.substr(name.size() - extension), GameEffectMgr::Extension);
    }

    template<class Value>
    Value ValueOf(PropertyObject const& object, std::string_view name, Value fallback)
    {
        PropertyValue const* const value = object.Get(name);
        Value const* const held = value != nullptr ? value->GetIf<Value>() : nullptr;
        return held != nullptr ? *held : fallback;
    }
}

std::optional<std::string> GameEffectInfo::FindEffectClass(TypeCatalog const& catalog, std::string_view templateClass)
{
    ClassInfo const* const base = catalog.FindClass(GameEffectHolder::EffectClass);
    if (base == nullptr || templateClass.size() <= TemplateSuffix.size() || templateClass.substr(templateClass.size() - TemplateSuffix.size()) != TemplateSuffix)
        return std::nullopt;
    std::string const stem(templateClass.substr(0, templateClass.size() - TemplateSuffix.size()));
    for (std::string const& candidate : { stem, stem + std::string(EffectSuffix) })
    {
        ClassInfo const* const found = catalog.FindClass(candidate);
        if (found != nullptr && found->IsA(*base))
            return found->Name;
    }
    return std::nullopt;
}

PropertyObjectPtr GameEffectInfo::MakeEffect(std::string& problem) const
{
    if (!Template)
    {
        problem = fmt::format("{} holds no template", Name);
        return nullptr;
    }
    if (EffectClass.empty())
    {
        problem = fmt::format("{} is a {}, and the type dump has no effect class for it", Name, TemplateClass);
        return nullptr;
    }
    PropertyObjectPtr effect = PropertyObject::Create(Template->GetCatalog(), EffectClass);
    if (!effect)
    {
        problem = fmt::format("the type dump has no property class {}", EffectClass);
        return nullptr;
    }
    ClassInfo const& from = Template->GetClass();
    for (PropertyInfo const& property : effect->GetClass().Properties)
    {
        PropertyInfo const* const source = from.FindProperty(property.Name);
        PropertyValue const* const value = Template->Get(property.Name);
        if (source == nullptr || value == nullptr || source->TypeName != property.TypeName || source->Container != property.Container)
            continue;
        PropertySetResult const result = effect->Set(property.Name, PropertyValue(*value));
        if (result != PropertySetResult::Ok)
        {
            problem = fmt::format("{}'s {} cannot be given to a {}: {}", Name, property.Name, EffectClass, PropertyObject::GetResultName(result));
            return nullptr;
        }
    }
    PropertySetResult const named = effect->Set("m_effectNameID", TemplateId);
    if (named != PropertySetResult::Ok)
    {
        problem = fmt::format("a {} cannot carry the name id {}: {}", EffectClass, TemplateId, PropertyObject::GetResultName(named));
        return nullptr;
    }
    return effect;
}

std::vector<std::string> GameEffectInfo::Describe() const
{
    std::vector<std::string> lines;
    lines.push_back(fmt::format("{}, template {}, a {} in {}", Name, TemplateId, TemplateClass, File));
    lines.push_back(EffectClass.empty() ? std::string("  makes no effect the type dump has a class for") : fmt::format("  makes a {}", EffectClass));
    lines.push_back(fmt::format("  lasts {} s, {}, shown on the {}{}", Duration, IsPublic ? "seen by everyone" : "seen by its wizard alone", IsOnPet ? "pet" : "wizard",
        Category.empty() ? std::string() : fmt::format(", category {}", Category)));
    return lines;
}

GameEffectMgr& GameEffectMgr::Instance()
{
    static GameEffectMgr instance;
    return instance;
}

std::shared_ptr<GameEffectStore const> GameEffectMgr::Read(KiwadArchive const& root, TypeCatalogPtr const& catalog, std::vector<std::string>& errors,
    std::vector<std::string>& warnings)
{
    std::vector<std::string> files;
    for (KiwadEntry const& entry : root.GetEntries())
        if (IsEffectFile(entry.Name))
            files.push_back(entry.Name);
    std::sort(files.begin(), files.end());
    if (files.empty())
    {
        errors.push_back(fmt::format("{} holds no {} file under {}", ConfigMgr::PathToUtf8(root.GetPath()), Extension, Folder));
        return nullptr;
    }
    std::size_t const before = errors.size();
    std::vector<GameEffectInfo> records;
    std::unordered_map<uint32, std::string> firstFile;
    for (std::string const& file : files)
    {
        KiwadReadResult const bytes = root.Read(file);
        if (!bytes.Succeeded())
        {
            errors.push_back(fmt::format("{} in {} cannot be read: {}", file, RootArchive, bytes.Error));
            continue;
        }
        BindReadResult const decoded = BindFile::Read(catalog, bytes.Data);
        if (!decoded.Ok() || !decoded.Decoded.Object)
        {
            errors.push_back(fmt::format("{} in {} does not read: {}", file, RootArchive, decoded.Ok() ? std::string("it holds no object") : decoded.Detail));
            continue;
        }
        PropertyObject const& list = *decoded.Decoded.Object;
        PropertyValue const* const listed = list.IsA(ListClass) ? list.Get("m_effectTemplates") : nullptr;
        if (listed == nullptr || listed->GetList() == nullptr)
        {
            errors.push_back(fmt::format("{} is a {}, which is not a {}", file, list.GetClass().Name, ListClass));
            continue;
        }
        PropertyValue::List const& templates = *listed->GetList();
        for (std::size_t index = 0; index < templates.size(); ++index)
        {
            PropertyObject const* const effect = templates[index].AsObject();
            if (effect == nullptr || !effect->IsA(TemplateClass))
            {
                errors.push_back(fmt::format("template {} of {} is {}, which is not a {}", index, file, effect == nullptr ? std::string("empty") : effect->GetClass().Name,
                    TemplateClass));
                continue;
            }
            std::string name = ValueOf<std::string>(*effect, "m_effectName", {});
            if (name.empty())
            {
                warnings.push_back(fmt::format("template {} of {} has no m_effectName, so the client cannot find it, and it is left out", index, file));
                continue;
            }
            uint32 const id = GameEffectStore::NameHash(name);
            auto const [first, added] = firstFile.emplace(id, file);
            if (!added)
            {
                warnings.push_back(fmt::format("{} in {} was read before in {}, and the first is kept, as the client keeps it", name, file, first->second));
                continue;
            }
            GameEffectInfo record;
            record.TemplateId = id;
            record.Name = std::move(name);
            record.File = file;
            record.TemplateClass = effect->GetClass().Name;
            record.EffectClass = GameEffectInfo::FindEffectClass(*catalog, record.TemplateClass).value_or(std::string());
            record.Category = ValueOf<std::string>(*effect, "m_effectCategory", {});
            record.Duration = ValueOf<double>(*effect, "m_duration", 0.0);
            record.IsPublic = ValueOf<bool>(*effect, "m_isPublic", false);
            record.IsOnPet = ValueOf<bool>(*effect, "m_bIsOnPet", false);
            record.Template = effect->Clone();
            records.push_back(std::move(record));
        }
    }
    if (errors.size() != before)
        return nullptr;
    return GameEffectStore::Build(std::move(records), errors);
}

void GameEffectMgr::SetInstall(std::filesystem::path root)
{
    std::lock_guard const lock(_installMutex);
    _install = std::move(root);
}

void GameEffectMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(Target), [this](std::vector<std::string>& errors) { return Load(errors); });
}

bool GameEffectMgr::Load(std::vector<std::string>& errors)
{
    std::filesystem::path install;
    {
        std::lock_guard const lock(_installMutex);
        install = _install;
    }
    if (install.empty())
    {
        errors.emplace_back("no Wizard101 install is in use, so no game effect can be read");
        return false;
    }
    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    if (!catalog)
    {
        errors.emplace_back("no type dump is loaded, so no game effect can be read");
        return false;
    }
    auto const started = std::chrono::steady_clock::now();
    std::filesystem::path const rootWad = install / "Data" / "GameData" / std::filesystem::path(RootArchive);
    std::string error;
    std::unique_ptr<KiwadArchive> const root = KiwadArchive::Open(rootWad, error);
    if (!root)
    {
        errors.push_back(fmt::format("{} cannot be opened: {}", ConfigMgr::PathToUtf8(rootWad), error));
        return false;
    }
    std::vector<std::string> warnings;
    std::shared_ptr<GameEffectStore const> store = Read(*root, catalog, errors, warnings);
    for (std::string const& warning : warnings)
        LOG_WARN(EffectLog, "Game effects: {}", warning);
    if (!store)
        return false;
    std::size_t const count = store->Size();
    std::size_t const making = static_cast<std::size_t>(std::count_if(store->GetAll().begin(), store->GetAll().end(), [](GameEffectInfo const& effect) { return !effect.EffectClass.empty(); }));
    _effects.Replace(std::move(store));
    auto const took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    LOG_INFO(EffectLog, "Read {} game effect templates, {} of them making an effect, from {} in {} ms", count, making, Folder, took.count());
    return true;
}

void GameEffectMgr::Clear()
{
    _effects.Replace(std::make_shared<GameEffectStore const>());
}
