/*
 * Project Ambrose by Imjustchico
 * Reads custom-emote animations and template ids from the user's own ObjectData/Emotes templates, keeps the bitfield each requires, and answers what a wizard's saved ownership masks unlock.
 */

#ifndef AMBROSE_CUSTOMEMOTEMGR_H
#define AMBROSE_CUSTOMEMOTEMGR_H

#include "ReloadableStore.h"
#include "TemplateManifest.h"
#include "Types.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct CustomEmoteAnimation
{
    std::string Animation;
    int32 BitFieldNumber = 0;
    bool IsDefault = false;
    uint32 TemplateId = 0;
};

class CustomEmoteStore
{
public:
    static constexpr std::size_t RankCount = 3;
    static constexpr std::string_view Target = "custom_emotes";
    static constexpr std::string_view Folder = "ObjectData/Emotes/";

    static std::optional<CustomEmoteStore> Build(std::vector<CustomEmoteAnimation> animations, std::vector<std::string>& errors);
    static std::optional<CustomEmoteStore> Read(std::filesystem::path const& gameData, TypeCatalogPtr const& catalog, TemplateManifest const& manifest,
        std::vector<std::string>& errors, std::size_t& threads);

    bool OwnsAnimation(std::string_view animation, std::array<uint32, RankCount> const& ownership) const noexcept;
    std::vector<uint32> OwnedTemplateIds(std::array<uint32, RankCount> const& ownership) const;
    std::size_t Size() const noexcept { return _animations.size(); }

private:
    struct Ownership
    {
        int32 BitFieldNumber = 0;
        bool IsDefault = false;
    };

    std::map<std::string, std::vector<Ownership>, std::less<>> _animations;
    std::map<uint32, std::vector<Ownership>> _templates;
};

class CustomEmoteMgr
{
public:
    static constexpr std::string_view Target = CustomEmoteStore::Target;
    static constexpr std::string_view Folder = CustomEmoteStore::Folder;

    static CustomEmoteMgr& Instance();

    CustomEmoteMgr() = default;
    CustomEmoteMgr(CustomEmoteMgr const&) = delete;
    CustomEmoteMgr& operator=(CustomEmoteMgr const&) = delete;

    void SetInstall(std::filesystem::path root);
    void RegisterReloadTargets();
    bool Load(std::vector<std::string>& errors);
    std::shared_ptr<CustomEmoteStore const> GetEmotes() const { return _emotes.Get(); }
    void Clear();

private:
    std::filesystem::path GameData() const;

    mutable std::mutex _installMutex;
    std::filesystem::path _install;
    ReloadableStore<CustomEmoteStore> _emotes;
};

#define sCustomEmoteMgr CustomEmoteMgr::Instance()

#endif
