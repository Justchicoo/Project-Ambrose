/*
 * Project Ambrose by Imjustchico
 * The object templates the world builds its objects from (sObjectTemplateMgr), read from the user's own install the way the client reads them: TemplateManifest.xml names the archive and entry that hold each template id, and that entry's object, of any class derived from CoreTemplate, is the template, whose behaviors every object made from it carries in the order the client expects, with an empty name where the list holds no behavior, as many NPC and prop templates do, and nothing for an entry whose name is empty, which the client leaves out of the object altogether. The manifest is read once and swapped in whole when it reloads; a template is decoded the first time it is asked for and kept, the least recently used dropped once the kept templates pass the memory budget, and a reload of the manifest retires every template decoded under the one before. A caller reads a template's fields through the typed view of its class, and holds the template it was given for as long as it needs it, so an object keeps the template it was made from through any reload. The player's template, id 1, which every wizard is made from, is held apart and read again when it reloads.
 */

#ifndef AMBROSE_OBJECTTEMPLATEMGR_H
#define AMBROSE_OBJECTTEMPLATEMGR_H

#include "ReloadableStore.h"
#include "TemplateManifest.h"
#include "TypeRegistry.h"
#include "Types.h"

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class KiwadArchive;
class PropertyObject;

struct ObjectTemplate
{
    uint32 TemplateId = 0;
    std::string Archive;
    std::string File;
    std::string ObjectName;
    std::vector<std::string> Behaviors;
    std::shared_ptr<PropertyObject const> Object;
    std::size_t Bytes = 0;

    bool IsLoaded() const noexcept { return TemplateId != 0; }

    template <class View>
    std::optional<View> As() const noexcept
    {
        return View::From(Object.get());
    }
};

struct TemplateLookup
{
    std::shared_ptr<ObjectTemplate const> Template;
    std::string Error;
};

struct TemplateCacheStats
{
    uint64 Hits = 0;
    uint64 Misses = 0;
    uint64 Evictions = 0;
    std::size_t Entries = 0;
    std::size_t Bytes = 0;
    std::size_t Budget = 0;
    uint64 Generation = 0;
};

class ObjectTemplateMgr
{
public:
    static constexpr std::string_view ManifestEntry = TemplateManifest::Entry;
    static constexpr std::string_view ManifestTarget = "templates";
    static constexpr std::string_view PlayerTarget = "player_template";
    static constexpr uint32 PlayerTemplateId = 1;
    static constexpr std::size_t DefaultBudget = std::size_t{ 256 } << 20;

    static ObjectTemplateMgr& Instance();

    ObjectTemplateMgr() = default;
    ObjectTemplateMgr(ObjectTemplateMgr const&) = delete;
    ObjectTemplateMgr& operator=(ObjectTemplateMgr const&) = delete;

    void SetInstall(std::filesystem::path root);
    void SetBudget(std::size_t bytes);
    void RegisterReloadTargets();
    bool LoadManifest(std::vector<std::string>& errors);
    bool LoadPlayer(std::vector<std::string>& errors);

    TemplateLookup Lookup(uint32 templateId);
    std::shared_ptr<ObjectTemplate const> GetTemplate(uint32 templateId) { return Lookup(templateId).Template; }
    std::shared_ptr<ObjectTemplate const> GetPlayer() const { return _player.Get(); }
    std::shared_ptr<TemplateManifest const> GetManifest() const { return _manifest.Get(); }
    TemplateCacheStats GetCacheStats() const;
    void Clear();

    static std::optional<ObjectTemplate> Decode(TypeCatalogPtr const& catalog, uint32 templateId, TemplateLocation const& location, std::span<uint8 const> bytes, std::string& error);
    static std::size_t EstimateBytes(PropertyObject const& object) noexcept;

private:
    struct Cached
    {
        std::shared_ptr<ObjectTemplate const> Template;
        uint64 Generation = 0;
        std::list<uint32>::iterator Use;
    };

    std::filesystem::path GameData() const;
    std::shared_ptr<KiwadArchive const> OpenArchive(std::string const& name, std::string& error);
    void Keep(uint32 templateId, std::shared_ptr<ObjectTemplate const> const& found, uint64 generation);
    void Trim();

    mutable std::mutex _installMutex;
    std::filesystem::path _install;
    ReloadableStore<TemplateManifest> _manifest;
    ReloadableStore<ObjectTemplate> _player;
    std::mutex _archiveMutex;
    std::map<std::string, std::shared_ptr<KiwadArchive const>, std::less<>> _archives;
    mutable std::mutex _cacheMutex;
    std::unordered_map<uint32, Cached> _cache;
    std::list<uint32> _uses;
    std::size_t _cachedBytes = 0;
    std::atomic<std::size_t> _budget{ DefaultBudget };
    std::atomic<uint64> _hits{ 0 };
    std::atomic<uint64> _misses{ 0 };
    std::atomic<uint64> _evictions{ 0 };
};

#define sObjectTemplateMgr ObjectTemplateMgr::Instance()

#endif
