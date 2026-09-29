/*
 * Project Ambrose by Imjustchico
 * The quick chat phrases of the user's install (sQuickChatMgr), read from Root.wad's QuickChat.xml, a tree of QuickChatEntry folders and phrases, into a set keyed by chat id, each phrase with the animation it plays, and swapped in whole through the reload target quickchat, so a request naming a phrase the client does not hold is refused. Folders carry id 0 and are not phrases; a set with an id twice is refused, and one that fails to load keeps the set serving and reports why.
 */

#ifndef AMBROSE_QUICKCHATMGR_H
#define AMBROSE_QUICKCHATMGR_H

#include "ReloadableStore.h"
#include "TypeRegistry.h"
#include "Types.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class KiwadArchive;

struct QuickChatPhrase
{
    uint32 ChatId = 0;
    std::string Label;
    std::string CharAnim;
    std::string FaceAnim;
    std::string Sound;
    uint32 CategoryMask = 0;
    bool MembersOnly = false;
    std::string UnlockKey;
};

class QuickChatPhrases
{
public:
    static std::shared_ptr<QuickChatPhrases const> Build(std::vector<QuickChatPhrase> phrases, std::vector<std::string>& errors);

    QuickChatPhrase const* Find(uint32 chatId) const noexcept;
    std::size_t Size() const noexcept { return _phrases.size(); }

private:
    std::unordered_map<uint32, QuickChatPhrase> _phrases;
};

class QuickChatMgr
{
public:
    static constexpr std::string_view Target = "quickchat";
    static constexpr std::string_view Entry = "QuickChat.xml";
    static constexpr std::string_view RootArchive = "Root.wad";

    static QuickChatMgr& Instance();

    QuickChatMgr() = default;
    QuickChatMgr(QuickChatMgr const&) = delete;
    QuickChatMgr& operator=(QuickChatMgr const&) = delete;

    static std::shared_ptr<QuickChatPhrases const> Read(KiwadArchive const& root, TypeCatalogPtr const& catalog, std::vector<std::string>& errors);

    void SetInstall(std::filesystem::path root);
    void RegisterReloadTargets();
    bool Load(std::vector<std::string>& errors);
    std::shared_ptr<QuickChatPhrases const> GetPhrases() const { return _phrases.Get(); }
    void Clear();

private:
    mutable std::mutex _installMutex;
    std::filesystem::path _install;
    ReloadableStore<QuickChatPhrases> _phrases;
};

#define sQuickChatMgr QuickChatMgr::Instance()

#endif
