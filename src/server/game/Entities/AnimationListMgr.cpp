/*
 * Project Ambrose by Imjustchico
 * Reads the list as the client's DML table XML, takes the key field of every record of its Animations table, refuses a record without one or with an empty name, takes a name keyed twice as the one type it names, as r806919's list keys RunBack_Girl_Relic twice with the same animation, and logs how many types the list swapped in holds and how long it took.
 */

#include "AnimationListMgr.h"
#include "ConfigMgr.h"
#include "DmlTableFile.h"
#include "KiwadArchive.h"
#include "Log.h"
#include "ReloadMgr.h"

#include <fmt/format.h>

#include <chrono>
#include <optional>
#include <utility>

namespace
{
    constexpr char const* AnimationLog = "server.loading";
}

std::shared_ptr<AnimationList const> AnimationList::Build(std::vector<std::string> types, std::vector<std::string>& errors)
{
    auto list = std::make_shared<AnimationList>();
    std::size_t const before = errors.size();
    for (std::string& type : types)
    {
        if (type.empty())
        {
            errors.push_back(fmt::format("{} has an animation type with no name", AnimationListMgr::Entry));
            continue;
        }
        list->_types.insert(std::move(type));
    }
    if (errors.size() != before)
        return nullptr;
    return list;
}

bool AnimationList::Contains(std::string_view type) const
{
    return _types.find(type) != _types.end();
}

AnimationListMgr& AnimationListMgr::Instance()
{
    static AnimationListMgr instance;
    return instance;
}

std::shared_ptr<AnimationList const> AnimationListMgr::Read(KiwadArchive const& root, std::vector<std::string>& errors)
{
    KiwadReadResult const read = root.Read(Entry);
    if (!read.Succeeded())
    {
        errors.push_back(fmt::format("{} cannot be read from {}: {}", Entry, ConfigMgr::PathToUtf8(root.GetPath()), read.Error));
        return nullptr;
    }
    std::string error;
    std::optional<std::vector<DmlTable>> const tables = DmlTableFile::Parse(std::string_view(reinterpret_cast<char const*>(read.Data.data()), read.Data.size()), error);
    if (!tables)
    {
        errors.push_back(fmt::format("{} {}", Entry, error));
        return nullptr;
    }
    DmlTable const* const animations = DmlTableFile::Find(*tables, Table);
    if (!animations)
    {
        errors.push_back(fmt::format("{} has no {} table", Entry, Table));
        return nullptr;
    }
    std::vector<std::string> types;
    types.reserve(animations->Records.size());
    for (std::size_t index = 0; index < animations->Records.size(); ++index)
    {
        DmlTableField const* const key = animations->Records[index].Find(KeyField);
        if (!key)
        {
            errors.push_back(fmt::format("record {} of {}'s {} table has no {}", index, Entry, Table, KeyField));
            return nullptr;
        }
        types.push_back(key->Value);
    }
    return AnimationList::Build(std::move(types), errors);
}

void AnimationListMgr::SetInstall(std::filesystem::path root)
{
    std::lock_guard const lock(_installMutex);
    _install = std::move(root);
}

void AnimationListMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(Target), [this](std::vector<std::string>& errors) { return Load(errors); });
}

bool AnimationListMgr::Load(std::vector<std::string>& errors)
{
    std::filesystem::path install;
    {
        std::lock_guard const lock(_installMutex);
        install = _install;
    }
    if (install.empty())
    {
        errors.emplace_back("no Wizard101 install is in use, so no animation type can be read");
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
    std::shared_ptr<AnimationList const> list = Read(*root, errors);
    if (!list)
        return false;
    std::size_t const count = list->Size();
    _list.Replace(std::move(list));
    auto const took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    LOG_INFO(AnimationLog, "Read {} animation types from {} in {} ms", count, Entry, took.count());
    return true;
}

void AnimationListMgr::Clear()
{
    _list.Replace(std::make_shared<AnimationList const>());
}
