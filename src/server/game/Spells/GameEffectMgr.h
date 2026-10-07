/*
 * Project Ambrose by Imjustchico
 * Every game effect template of the user's install (sGameEffectMgr), read from the GameEffectTemplateList files Root.wad holds under GameEffectData/, each template keyed as the client keys it, by the string hash of its m_effectName, a name listed twice kept as first read, as the client keeps it, and swapped in as one checked set through the reload target effects; and the effect a template makes, an instance of the effect class its template class names, carrying the template's id as m_effectNameID and every value the template holds under a name and type the effect class shares.
 */

#ifndef AMBROSE_GAMEEFFECTMGR_H
#define AMBROSE_GAMEEFFECTMGR_H

#include "NameKeyedTemplates.h"
#include "PropertyObject.h"
#include "ReloadableStore.h"
#include "Types.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class KiwadArchive;

struct GameEffectInfo
{
    uint32 TemplateId = 0;
    std::string Name;
    std::string File;
    std::string TemplateClass;
    std::string EffectClass;
    std::string Category;
    double Duration = 0.0;
    bool IsPublic = false;
    bool IsOnPet = false;
    std::shared_ptr<PropertyObject const> Template;

    static std::optional<std::string> FindEffectClass(TypeCatalog const& catalog, std::string_view templateClass);

    PropertyObjectPtr MakeEffect(std::string& problem) const;
    std::vector<std::string> Describe() const;
};

using GameEffectStore = NameKeyedTemplates<GameEffectInfo>;

class GameEffectMgr
{
public:
    static constexpr std::string_view Target = "effects";
    static constexpr std::string_view Folder = "GameEffectData/";
    static constexpr std::string_view Extension = ".xml";
    static constexpr std::string_view RootArchive = "Root.wad";
    static constexpr std::string_view ListClass = "class GameEffectTemplateList";
    static constexpr std::string_view TemplateClass = "class GameEffectTemplate";

    static GameEffectMgr& Instance();

    GameEffectMgr() = default;
    GameEffectMgr(GameEffectMgr const&) = delete;
    GameEffectMgr& operator=(GameEffectMgr const&) = delete;

    static std::shared_ptr<GameEffectStore const> Read(KiwadArchive const& root, TypeCatalogPtr const& catalog, std::vector<std::string>& errors,
        std::vector<std::string>& warnings);

    void SetInstall(std::filesystem::path root);
    void RegisterReloadTargets();
    bool Load(std::vector<std::string>& errors);
    std::shared_ptr<GameEffectStore const> GetEffects() const { return _effects.Get(); }
    void Clear();

private:
    mutable std::mutex _installMutex;
    std::filesystem::path _install;
    ReloadableStore<GameEffectStore> _effects;
};

#define sGameEffectMgr GameEffectMgr::Instance()

#endif
