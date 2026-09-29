/*
 * Project Ambrose by Imjustchico
 * The animation types of the user's install (sAnimationListMgr), read from Root.wad's AnimationData/MasterAnimationList.xml, whose Animations table keys each type the client can play, such as Wave, Chat or DabDance, by name; swapped in whole through the reload target animations, so an emote naming an animation the client has no type for is refused. A list that fails to load keeps the one serving and reports why.
 */

#ifndef AMBROSE_ANIMATIONLISTMGR_H
#define AMBROSE_ANIMATIONLISTMGR_H

#include "ReloadableStore.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class KiwadArchive;

class AnimationList
{
public:
    static std::shared_ptr<AnimationList const> Build(std::vector<std::string> types, std::vector<std::string>& errors);

    bool Contains(std::string_view type) const;
    std::size_t Size() const noexcept { return _types.size(); }

private:
    std::set<std::string, std::less<>> _types;
};

class AnimationListMgr
{
public:
    static constexpr std::string_view Target = "animations";
    static constexpr std::string_view Entry = "AnimationData/MasterAnimationList.xml";
    static constexpr std::string_view Table = "Animations";
    static constexpr std::string_view KeyField = "AnimType";
    static constexpr std::string_view RootArchive = "Root.wad";

    static AnimationListMgr& Instance();

    AnimationListMgr() = default;
    AnimationListMgr(AnimationListMgr const&) = delete;
    AnimationListMgr& operator=(AnimationListMgr const&) = delete;

    static std::shared_ptr<AnimationList const> Read(KiwadArchive const& root, std::vector<std::string>& errors);

    void SetInstall(std::filesystem::path root);
    void RegisterReloadTargets();
    bool Load(std::vector<std::string>& errors);
    std::shared_ptr<AnimationList const> GetList() const { return _list.Get(); }
    void Clear();

private:
    mutable std::mutex _installMutex;
    std::filesystem::path _install;
    ReloadableStore<AnimationList> _list;
};

#define sAnimationListMgr AnimationListMgr::Instance()

#endif
