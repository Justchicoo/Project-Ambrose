/*
 * Project Ambrose by Imjustchico
 * Reads each Root.wad entry under Scripts/ that ends in .lua, keys it by its lower-cased name with forward slashes, refuses the library when an entry cannot be read, and lets a reader outlive the library it came from by holding it.
 */

#include "LuaScriptLibrary.h"
#include "ConfigMgr.h"
#include "KiwadArchive.h"

#include <fmt/format.h>

#include <utility>

namespace
{
    bool IsScript(std::string_view name)
    {
        std::string const normalized = KiwadArchive::NormalizeName(name);
        std::string const folder = KiwadArchive::NormalizeName(LuaState::ScriptsFolder);
        return normalized.size() > folder.size() + LuaScriptLibrary::Extension.size() && normalized.starts_with(folder) && normalized.ends_with(LuaScriptLibrary::Extension);
    }
}

std::shared_ptr<LuaScriptLibrary const> LuaScriptLibrary::Build(std::map<std::string, std::string> scripts)
{
    auto library = std::make_shared<LuaScriptLibrary>();
    for (auto& [name, source] : scripts)
    {
        std::string const script = LuaState::NameOf(name);
        library->_scripts.insert_or_assign(KiwadArchive::NormalizeName(script), Script{ script, std::move(source) });
    }
    return library;
}

std::shared_ptr<LuaScriptLibrary const> LuaScriptLibrary::Read(KiwadArchive const& root, std::vector<std::string>& errors)
{
    std::map<std::string, std::string> scripts;
    std::size_t const before = errors.size();
    for (KiwadEntry const& entry : root.GetEntries())
    {
        if (!IsScript(entry.Name))
            continue;
        KiwadReadResult const read = root.Read(entry);
        if (!read.Succeeded())
        {
            errors.push_back(fmt::format("{} cannot be read from {}: {}", entry.Name, ConfigMgr::PathToUtf8(root.GetPath()), read.Error));
            continue;
        }
        scripts.insert_or_assign(entry.Name, std::string(reinterpret_cast<char const*>(read.Data.data()), read.Data.size()));
    }
    if (errors.size() != before)
        return nullptr;
    return Build(std::move(scripts));
}

std::optional<std::string> LuaScriptLibrary::Find(std::string_view name) const
{
    auto const found = _scripts.find(KiwadArchive::NormalizeName(LuaState::NameOf(name)));
    if (found == _scripts.end())
        return std::nullopt;
    return found->second.Source;
}

std::vector<std::string> LuaScriptLibrary::GetNames() const
{
    std::vector<std::string> names;
    names.reserve(_scripts.size());
    for (auto const& [key, script] : _scripts)
        names.push_back(script.Name);
    return names;
}

LuaState::SourceReader LuaScriptLibrary::ReaderOf(std::shared_ptr<LuaScriptLibrary const> library)
{
    return [library = std::move(library)](std::string_view name) { return library ? library->Find(name) : std::nullopt; };
}
