/*
 * Project Ambrose by Imjustchico
 * The object templates the world builds its objects from (sObjectTemplateMgr), read from the user's own install the way the client reads them: TemplateManifest.xml names the file that holds a template id, and that file's GameObjectTemplate lists the behaviors every object made from it carries, in the order the client expects them. It holds the player's template, id 1, which every wizard in the world is made from; the rest of the store arrives with the template milestone. A template is rebuilt off to the side and swapped in whole, and a read that fails keeps the template serving.
 */

#ifndef AMBROSE_OBJECTTEMPLATEMGR_H
#define AMBROSE_OBJECTTEMPLATEMGR_H

#include "ReloadableStore.h"
#include "TypeRegistry.h"
#include "Types.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class KiwadArchive;

struct ObjectTemplate
{
    uint32 TemplateId = 0;
    std::string File;
    std::string ObjectName;
    std::vector<std::string> Behaviors;

    bool IsLoaded() const noexcept { return TemplateId != 0; }
};

class ObjectTemplateMgr
{
public:
    static constexpr std::string_view ManifestEntry = "TemplateManifest.xml";
    static constexpr std::string_view PlayerTarget = "player_template";
    static constexpr uint32 PlayerTemplateId = 1;

    static ObjectTemplateMgr& Instance();

    ObjectTemplateMgr(ObjectTemplateMgr const&) = delete;
    ObjectTemplateMgr& operator=(ObjectTemplateMgr const&) = delete;

    void SetInstall(std::filesystem::path root);
    void RegisterReloadTargets();
    bool LoadPlayer(std::vector<std::string>& errors);
    std::shared_ptr<ObjectTemplate const> GetPlayer() const { return _player.Get(); }
    void Clear();

    static std::optional<ObjectTemplate> Read(KiwadArchive const& root, TypeCatalogPtr const& catalog, uint32 templateId, std::string& error);

private:
    ObjectTemplateMgr() = default;

    mutable std::mutex _installMutex;
    std::filesystem::path _install;
    ReloadableStore<ObjectTemplate> _player;
};

#define sObjectTemplateMgr ObjectTemplateMgr::Instance()

#endif
