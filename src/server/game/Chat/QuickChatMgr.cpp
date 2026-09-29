/*
 * Project Ambrose by Imjustchico
 * Reads QuickChat.xml as a BINd file through the quick chat entry view and walks its folders without recursion, taking every entry with an id as a phrase; the set is built only when no id repeats, then swapped in with how many phrases it holds and how long it took logged. Every problem reading the file names it.
 */

#include "QuickChatMgr.h"
#include "BindFile.h"
#include "ConfigMgr.h"
#include "KiwadArchive.h"
#include "Log.h"
#include "ObjectViews.h"
#include "PropertyObject.h"
#include "ReloadMgr.h"

#include <fmt/format.h>

#include <chrono>
#include <optional>
#include <utility>

namespace
{
    constexpr char const* QuickChatLog = "server.loading";
}

std::shared_ptr<QuickChatPhrases const> QuickChatPhrases::Build(std::vector<QuickChatPhrase> phrases, std::vector<std::string>& errors)
{
    auto set = std::make_shared<QuickChatPhrases>();
    std::size_t const before = errors.size();
    for (QuickChatPhrase& phrase : phrases)
    {
        uint32 const id = phrase.ChatId;
        if (!set->_phrases.emplace(id, std::move(phrase)).second)
            errors.push_back(fmt::format("{} holds the chat id {} more than once", QuickChatMgr::Entry, id));
    }
    if (errors.size() != before)
        return nullptr;
    return set;
}

QuickChatPhrase const* QuickChatPhrases::Find(uint32 chatId) const noexcept
{
    auto const found = _phrases.find(chatId);
    return found == _phrases.end() ? nullptr : &found->second;
}

QuickChatMgr& QuickChatMgr::Instance()
{
    static QuickChatMgr instance;
    return instance;
}

std::shared_ptr<QuickChatPhrases const> QuickChatMgr::Read(KiwadArchive const& root, TypeCatalogPtr const& catalog, std::vector<std::string>& errors)
{
    KiwadReadResult const read = root.Read(Entry);
    if (!read.Succeeded())
    {
        errors.push_back(fmt::format("{} cannot be read from {}: {}", Entry, ConfigMgr::PathToUtf8(root.GetPath()), read.Error));
        return nullptr;
    }
    BindReadResult const bind = BindFile::Read(catalog, read.Data);
    if (!bind.Ok() || !bind.Decoded.Object)
    {
        errors.push_back(fmt::format("{} does not decode: {}", Entry, bind.Detail.empty() ? std::string(BindFile::GetStatusName(bind.Status)) : bind.Detail));
        return nullptr;
    }
    std::vector<QuickChatPhrase> phrases;
    std::vector<PropertyObject const*> pending{ bind.Decoded.Object.get() };
    while (!pending.empty())
    {
        PropertyObject const* const object = pending.back();
        pending.pop_back();
        std::optional<QuickChatEntryView> const entry = QuickChatEntryView::From(object);
        if (!entry)
        {
            errors.push_back(fmt::format("{} holds a {} where a QuickChatEntry belongs", Entry, object ? object->GetClass().Name : std::string("null entry")));
            return nullptr;
        }
        if (entry->GetChatId() != 0)
            phrases.push_back({ entry->GetChatId(), entry->GetLabel(), entry->GetCharAnim(), entry->GetFaceAnim(), entry->GetSound(), entry->GetCategoryMask(), entry->IsMembersOnly(),
                entry->GetUnlockKey() });
        for (PropertyValue const& child : entry->GetChildEntries())
            pending.push_back(child.AsObject());
    }
    return QuickChatPhrases::Build(std::move(phrases), errors);
}

void QuickChatMgr::SetInstall(std::filesystem::path root)
{
    std::lock_guard const lock(_installMutex);
    _install = std::move(root);
}

void QuickChatMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(Target), [this](std::vector<std::string>& errors) { return Load(errors); });
}

bool QuickChatMgr::Load(std::vector<std::string>& errors)
{
    std::filesystem::path install;
    {
        std::lock_guard const lock(_installMutex);
        install = _install;
    }
    if (install.empty())
    {
        errors.emplace_back("no Wizard101 install is in use, so no quick chat phrase can be read");
        return false;
    }
    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    if (!catalog)
    {
        errors.emplace_back("no type dump is loaded, so no quick chat phrase can be read");
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
    std::shared_ptr<QuickChatPhrases const> phrases = Read(*root, catalog, errors);
    if (!phrases)
        return false;
    std::size_t const count = phrases->Size();
    _phrases.Replace(std::move(phrases));
    auto const took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    LOG_INFO(QuickChatLog, "Read {} quick chat phrases from {} in {} ms", count, Entry, took.count());
    return true;
}

void QuickChatMgr::Clear()
{
    _phrases.Replace(std::make_shared<QuickChatPhrases const>());
}
