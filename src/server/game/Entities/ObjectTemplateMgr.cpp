/*
 * Project Ambrose by Imjustchico
 * Reads the manifest from a Root.wad opened afresh, so a reload sees the file as it is now, and when it swaps a manifest in closes every archive held open for templates but that Root.wad, which it keeps for the templates Root.wad holds; decodes a template from the archive its manifest entry names as any class derived from CoreTemplate, since recipes, spells and quests are templates as much as game objects are, its behaviors read through the CoreTemplate view and each named by m_behaviorName, a null entry kept as an empty name and an entry whose name is empty left out, as CoreObjectFactory::AddBehavior gives the first an empty slot and the second none, its name the GameObjectTemplate's m_objectName or else whichever property its class flags ObjectName, and says which step failed and why when one does; the player's template must be a GameObjectTemplate, since a wizard is built from its behaviors. The cache keeps a template only under the manifest it was decoded under, so a decode that finishes after a reload is handed to its caller but not kept, counts a template's memory as the values its object holds, keeps no template larger than the whole budget, and drops the least recently used first. The player's template is read again from its archive opened afresh whenever it reloads, rather than taken from the cache or an archive already open, so it reads the file as it is now.
 */

#include "ObjectTemplateMgr.h"
#include "BindFile.h"
#include "ConfigMgr.h"
#include "KiwadArchive.h"
#include "Log.h"
#include "ObjectViews.h"
#include "PropertyFlags.h"
#include "PropertyObject.h"
#include "ReloadMgr.h"

#include <fmt/format.h>

#include <chrono>
#include <utility>

namespace
{
    constexpr char const* TemplateLog = "server.loading";

    std::size_t EstimateValue(PropertyValue const& value) noexcept
    {
        std::size_t bytes = sizeof(PropertyValue);
        if (std::string const* const text = value.GetIf<std::string>())
            bytes += text->capacity();
        else if (std::u16string const* const wide = value.GetIf<std::u16string>())
            bytes += wide->capacity() * sizeof(char16_t);
        else if (PropertyValue::List const* const list = value.GetList())
        {
            for (PropertyValue const& item : *list)
                bytes += EstimateValue(item);
            bytes += (list->capacity() - list->size()) * sizeof(PropertyValue);
        }
        else if (PropertyObject const* const object = value.AsObject())
            bytes += ObjectTemplateMgr::EstimateBytes(*object);
        return bytes;
    }

    std::string NameOf(PropertyObject const& object)
    {
        if (std::optional<GameObjectTemplateView> const view = GameObjectTemplateView::From(object))
            return view->GetObjectName();
        std::vector<PropertyInfo> const& properties = object.GetClass().Properties;
        for (std::size_t ordinal = 0; ordinal < properties.size(); ++ordinal)
        {
            if (!properties[ordinal].HasFlag(PropertyFlag::ObjectName))
                continue;
            PropertyValue const* const value = object.GetAt(ordinal);
            if (std::string const* const text = value ? value->GetIf<std::string>() : nullptr)
                return *text;
        }
        return {};
    }
}

ObjectTemplateMgr& ObjectTemplateMgr::Instance()
{
    static ObjectTemplateMgr instance;
    return instance;
}

void ObjectTemplateMgr::SetInstall(std::filesystem::path root)
{
    {
        std::lock_guard const lock(_installMutex);
        _install = std::move(root);
    }
    std::lock_guard const lock(_archiveMutex);
    _archives.clear();
}

void ObjectTemplateMgr::SetBudget(std::size_t bytes)
{
    _budget.store(bytes, std::memory_order_relaxed);
    std::lock_guard const lock(_cacheMutex);
    Trim();
}

void ObjectTemplateMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(ManifestTarget), [this](std::vector<std::string>& errors) { return LoadManifest(errors); });
    sReloadMgr.Register(std::string(PlayerTarget), [this](std::vector<std::string>& errors) { return LoadPlayer(errors); }, { std::string(ManifestTarget) });
}

std::filesystem::path ObjectTemplateMgr::GameData() const
{
    std::lock_guard const lock(_installMutex);
    return _install.empty() ? std::filesystem::path() : _install / "Data" / "GameData";
}

bool ObjectTemplateMgr::LoadManifest(std::vector<std::string>& errors)
{
    std::filesystem::path const gameData = GameData();
    if (gameData.empty())
    {
        errors.emplace_back("no Wizard101 install is in use, so the template manifest cannot be read");
        return false;
    }
    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    if (!catalog)
    {
        errors.emplace_back("no type dump is loaded, so the template manifest cannot be read");
        return false;
    }
    auto const started = std::chrono::steady_clock::now();
    std::string error;
    std::filesystem::path const rootWad = gameData / TemplateManifest::RootArchive;
    std::shared_ptr<KiwadArchive const> root = KiwadArchive::Open(rootWad, error);
    if (!root)
    {
        errors.push_back(fmt::format("{} cannot be opened: {}", ConfigMgr::PathToUtf8(rootWad), error));
        return false;
    }
    std::shared_ptr<TemplateManifest const> manifest = TemplateManifest::Read(*root, catalog, errors);
    if (!manifest)
        return false;
    std::size_t const templates = manifest->Size();
    std::size_t const archives = manifest->GetArchives().size();
    {
        std::lock_guard const lock(_archiveMutex);
        _archives.clear();
        _archives.emplace(std::string(TemplateManifest::RootArchive), std::move(root));
    }
    {
        std::lock_guard const lock(_cacheMutex);
        _manifest.Replace(std::move(manifest));
        _cache.clear();
        _uses.clear();
        _cachedBytes = 0;
    }
    auto const took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    LOG_INFO(TemplateLog, "Read the template manifest in {} ms: {} templates in {} archives", took.count(), templates, archives);
    return true;
}

bool ObjectTemplateMgr::LoadPlayer(std::vector<std::string>& errors)
{
    uint64 const generation = _manifest.GetGeneration();
    std::shared_ptr<TemplateManifest const> const manifest = _manifest.Get();
    TemplateLocation const* const location = manifest->Find(PlayerTemplateId);
    if (!location)
    {
        errors.push_back(manifest->Size() == 0 ? std::string("the template manifest is not loaded, so the player's template cannot be read")
                                               : fmt::format("{} lists no template {}, the player's", ManifestEntry, PlayerTemplateId));
        return false;
    }
    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    if (!catalog)
    {
        errors.emplace_back("no type dump is loaded, so the player's template cannot be read");
        return false;
    }
    std::filesystem::path const gameData = GameData();
    if (gameData.empty())
    {
        errors.emplace_back("no Wizard101 install is in use, so the player's template cannot be read");
        return false;
    }
    auto const started = std::chrono::steady_clock::now();
    std::string error;
    std::filesystem::path const path = gameData / ConfigMgr::PathFromUtf8(location->Archive);
    std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(path, error);
    if (!archive)
    {
        errors.push_back(fmt::format("the player's template is in {}, which cannot be opened: {}", ConfigMgr::PathToUtf8(path), error));
        return false;
    }
    KiwadReadResult const bytes = archive->Read(location->Path);
    if (!bytes.Succeeded())
    {
        errors.push_back(fmt::format("the player's template is {} in {}, which cannot be read: {}", location->Path, location->Archive, bytes.Error));
        return false;
    }
    std::optional<ObjectTemplate> player = Decode(catalog, PlayerTemplateId, *location, bytes.Data, error);
    if (!player)
    {
        errors.push_back(std::move(error));
        return false;
    }
    if (!player->As<GameObjectTemplateView>())
    {
        errors.push_back(fmt::format("the player's template is {} in {}, a {}, which is not a GameObjectTemplate", location->Path, location->Archive, player->Object->GetClass().Name));
        return false;
    }
    auto const took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    LOG_INFO(TemplateLog, "Read the player's template {} from {} in {} ms: {} behaviors", player->TemplateId, player->File, took.count(), player->Behaviors.size());
    auto shared = std::make_shared<ObjectTemplate const>(std::move(*player));
    Keep(PlayerTemplateId, shared, generation);
    _player.Replace(std::move(shared));
    return true;
}

TemplateLookup ObjectTemplateMgr::Lookup(uint32 templateId)
{
    uint64 const generation = _manifest.GetGeneration();
    {
        std::lock_guard const lock(_cacheMutex);
        auto const found = _cache.find(templateId);
        if (found != _cache.end() && found->second.Generation == generation)
        {
            _uses.splice(_uses.begin(), _uses, found->second.Use);
            _hits.fetch_add(1, std::memory_order_relaxed);
            return { found->second.Template, {} };
        }
    }
    _misses.fetch_add(1, std::memory_order_relaxed);
    std::shared_ptr<TemplateManifest const> const manifest = _manifest.Get();
    TemplateLocation const* const location = manifest->Find(templateId);
    if (!location)
        return { nullptr, manifest->Size() == 0 ? std::string("the template manifest is not loaded") : fmt::format("{} lists no template {}", ManifestEntry, templateId) };
    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    if (!catalog)
        return { nullptr, "no type dump is loaded, so no template can be read" };
    std::string error;
    std::shared_ptr<KiwadArchive const> const archive = OpenArchive(location->Archive, error);
    if (!archive)
        return { nullptr, std::move(error) };
    KiwadReadResult const bytes = archive->Read(location->Path);
    if (!bytes.Succeeded())
        return { nullptr, fmt::format("template {} is {} in {}, which cannot be read: {}", templateId, location->Path, location->Archive, bytes.Error) };
    std::optional<ObjectTemplate> decoded = Decode(catalog, templateId, *location, bytes.Data, error);
    if (!decoded)
        return { nullptr, std::move(error) };
    auto shared = std::make_shared<ObjectTemplate const>(std::move(*decoded));
    Keep(templateId, shared, generation);
    return { std::move(shared), {} };
}

TemplateCacheStats ObjectTemplateMgr::GetCacheStats() const
{
    std::lock_guard const lock(_cacheMutex);
    return { _hits.load(std::memory_order_relaxed), _misses.load(std::memory_order_relaxed), _evictions.load(std::memory_order_relaxed), _cache.size(), _cachedBytes,
        _budget.load(std::memory_order_relaxed), _manifest.GetGeneration() };
}

void ObjectTemplateMgr::Clear()
{
    {
        std::lock_guard const lock(_cacheMutex);
        _manifest.Replace(std::make_shared<TemplateManifest const>());
        _cache.clear();
        _uses.clear();
        _cachedBytes = 0;
    }
    _player.Replace(ObjectTemplate());
    std::lock_guard const lock(_archiveMutex);
    _archives.clear();
}

std::shared_ptr<KiwadArchive const> ObjectTemplateMgr::OpenArchive(std::string const& name, std::string& error)
{
    std::lock_guard const lock(_archiveMutex);
    auto const found = _archives.find(name);
    if (found != _archives.end())
        return found->second;
    std::filesystem::path const gameData = GameData();
    if (gameData.empty())
    {
        error = "no Wizard101 install is in use, so no template can be read";
        return nullptr;
    }
    std::filesystem::path const path = gameData / ConfigMgr::PathFromUtf8(name);
    std::unique_ptr<KiwadArchive> opened = KiwadArchive::Open(path, error);
    if (!opened)
    {
        error = fmt::format("{} cannot be opened: {}", ConfigMgr::PathToUtf8(path), error);
        return nullptr;
    }
    std::shared_ptr<KiwadArchive const> shared(std::move(opened));
    _archives.emplace(name, shared);
    return shared;
}

void ObjectTemplateMgr::Keep(uint32 templateId, std::shared_ptr<ObjectTemplate const> const& found, uint64 generation)
{
    std::lock_guard const lock(_cacheMutex);
    if (generation != _manifest.GetGeneration() || found->Bytes > _budget.load(std::memory_order_relaxed))
        return;
    if (auto const existing = _cache.find(templateId); existing != _cache.end())
    {
        _cachedBytes -= existing->second.Template->Bytes;
        _uses.erase(existing->second.Use);
        _cache.erase(existing);
    }
    _uses.push_front(templateId);
    _cache.emplace(templateId, Cached{ found, generation, _uses.begin() });
    _cachedBytes += found->Bytes;
    Trim();
}

void ObjectTemplateMgr::Trim()
{
    std::size_t const budget = _budget.load(std::memory_order_relaxed);
    while (_cachedBytes > budget && !_uses.empty())
    {
        auto const oldest = _cache.find(_uses.back());
        _cachedBytes -= oldest->second.Template->Bytes;
        _cache.erase(oldest);
        _uses.pop_back();
        _evictions.fetch_add(1, std::memory_order_relaxed);
    }
}

std::optional<ObjectTemplate> ObjectTemplateMgr::Decode(TypeCatalogPtr const& catalog, uint32 templateId, TemplateLocation const& location, std::span<uint8 const> bytes, std::string& error)
{
    BindReadResult decoded = BindFile::Read(catalog, bytes);
    if (!decoded.Ok() || !decoded.Decoded.Object)
    {
        error = fmt::format("{} in {} does not read as a template: {}", location.Path, location.Archive, decoded.Ok() ? std::string("it holds no object") : decoded.Detail);
        return std::nullopt;
    }
    std::optional<CoreTemplateView> const core = CoreTemplateView::From(*decoded.Decoded.Object);
    if (!core)
    {
        error = fmt::format("{} in {} is a {}, which is not a CoreTemplate", location.Path, location.Archive, decoded.Decoded.Object->GetClass().Name);
        return std::nullopt;
    }
    ObjectTemplate found;
    found.Archive = location.Archive;
    found.File = location.Path;
    found.TemplateId = templateId;
    found.ObjectName = NameOf(*decoded.Decoded.Object);
    std::size_t position = 0;
    for (PropertyValue const& entry : core->GetBehaviors())
    {
        PropertyObject const* const behavior = entry.AsObject();
        if (!behavior)
        {
            found.Behaviors.emplace_back();
            ++position;
            continue;
        }
        PropertyValue const* const name = behavior->Get("m_behaviorName");
        std::string const* const text = name ? name->GetIf<std::string>() : nullptr;
        if (!text)
        {
            error = fmt::format("behavior {} of {} in {} is a {}, which carries no m_behaviorName", position, location.Path, location.Archive, behavior->GetClass().Name);
            return std::nullopt;
        }
        if (!text->empty())
            found.Behaviors.push_back(*text);
        ++position;
    }
    found.Bytes = sizeof(ObjectTemplate) + EstimateBytes(*decoded.Decoded.Object);
    found.Object = std::shared_ptr<PropertyObject const>(std::move(decoded.Decoded.Object));
    return found;
}

std::size_t ObjectTemplateMgr::EstimateBytes(PropertyObject const& object) noexcept
{
    std::size_t bytes = sizeof(PropertyObject);
    std::size_t const count = object.GetClass().Properties.size();
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal)
        if (PropertyValue const* const value = object.GetAt(ordinal))
            bytes += EstimateValue(*value);
    return bytes;
}
