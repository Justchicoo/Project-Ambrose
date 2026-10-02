/*
 * Project Ambrose by Imjustchico
 * Reads the user's Root.wad chat-filter lists as one immutable snapshot, keeps filtered-word decisions consistent for each reader, and replaces a live snapshot only after all five UTF-16 files load.
 */

#ifndef AMBROSE_CHATFILTER_H
#define AMBROSE_CHATFILTER_H

#include "ReloadableStore.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class KiwadArchive;

enum class ChatFilterResult : uint8
{
    Clear,
    Whitelisted,
    Blacklisted
};

class ChatFilterLists
{
public:
    static std::shared_ptr<ChatFilterLists const> Read(KiwadArchive const& root, std::vector<std::string>& errors);

    ChatFilterResult Inspect(std::u16string_view message) const;
    std::size_t BlacklistSize() const noexcept { return _blacklist.size(); }
    std::size_t WhitelistSize() const noexcept { return _whitelist.size(); }
    std::size_t WhitelistPhraseCount() const noexcept { return _whitelistPhrases.size(); }
    std::vector<std::u16string> AddedBlacklistEntries(ChatFilterLists const& previous) const;
    std::vector<std::u16string> AddedWhitelistEntries(ChatFilterLists const& previous) const;

private:
    std::unordered_set<std::u16string> _whitelist;
    std::vector<std::u16string> _whitelistPhrases;
    std::unordered_set<std::u16string> _blacklist;
    std::unordered_set<std::u16string> _exceptions;
    std::unordered_map<char16_t, std::u16string> _replacements;
};

using ChatFilterAdditionNotifier = std::function<void(std::vector<std::u16string> const&, std::vector<std::u16string> const&)>;

class ChatFilterMgr
{
public:
    static constexpr std::string_view Target = "chatfilter";

    static ChatFilterMgr& Instance();

    ChatFilterMgr() = default;
    ChatFilterMgr(ChatFilterMgr const&) = delete;
    ChatFilterMgr& operator=(ChatFilterMgr const&) = delete;

    void SetInstall(std::filesystem::path root);
    void RegisterReloadTarget(ChatFilterAdditionNotifier notifyAdditions = {});
    bool Load(std::vector<std::string>& errors);
    std::shared_ptr<ChatFilterLists const> GetLists() const { return _lists.Get(); }
    uint64 GetGeneration() const noexcept { return _lists.GetGeneration(); }
    void Clear();

private:
    mutable std::mutex _installMutex;
    std::filesystem::path _install;
    ReloadableStore<ChatFilterLists> _lists;
};

#define sChatFilterMgr ChatFilterMgr::Instance()

#endif
