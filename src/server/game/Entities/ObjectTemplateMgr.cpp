/*
 * Project Ambrose by Imjustchico
 * Reads a template from Root.wad: TemplateManifest.xml through its typed view for the file the id lives in, then that file's BINd GameObjectTemplate for its behaviors, each named by its m_behaviorName, and says which step failed and why when one does, so a template the install cannot give is a named refusal rather than an object built without its behaviors.
 */

#include "ObjectTemplateMgr.h"
#include "BindFile.h"
#include "ConfigMgr.h"
#include "KiwadArchive.h"
#include "Log.h"
#include "ObjectViews.h"
#include "ReloadMgr.h"

#include <fmt/format.h>

#include <chrono>
#include <utility>

namespace
{
    constexpr char const* TemplateLog = "server.loading";
}

ObjectTemplateMgr& ObjectTemplateMgr::Instance()
{
    static ObjectTemplateMgr instance;
    return instance;
}

void ObjectTemplateMgr::SetInstall(std::filesystem::path root)
{
    std::lock_guard const lock(_installMutex);
    _install = std::move(root);
}

void ObjectTemplateMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(PlayerTarget), [this](std::vector<std::string>& errors) { return LoadPlayer(errors); });
}

bool ObjectTemplateMgr::LoadPlayer(std::vector<std::string>& errors)
{
    std::filesystem::path root;
    {
        std::lock_guard const lock(_installMutex);
        root = _install;
    }
    if (root.empty())
    {
        errors.emplace_back("no Wizard101 install is in use, so the player's template cannot be read");
        return false;
    }
    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    if (!catalog)
    {
        errors.emplace_back("no type dump is loaded, so the player's template cannot be read");
        return false;
    }
    auto const started = std::chrono::steady_clock::now();
    std::string error;
    std::filesystem::path const rootWad = root / "Data" / "GameData" / "Root.wad";
    std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(rootWad, error);
    if (!archive)
    {
        errors.push_back(fmt::format("{} cannot be opened: {}", ConfigMgr::PathToUtf8(rootWad), error));
        return false;
    }
    std::optional<ObjectTemplate> player = Read(*archive, catalog, PlayerTemplateId, error);
    if (!player)
    {
        errors.push_back(std::move(error));
        return false;
    }
    auto const took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    LOG_INFO(TemplateLog, "Read the player's template {} from {} in {} ms: {} behaviors", player->TemplateId, player->File, took.count(), player->Behaviors.size());
    _player.Replace(std::move(*player));
    return true;
}

void ObjectTemplateMgr::Clear()
{
    _player.Replace(ObjectTemplate());
}

std::optional<ObjectTemplate> ObjectTemplateMgr::Read(KiwadArchive const& root, TypeCatalogPtr const& catalog, uint32 templateId, std::string& error)
{
    KiwadReadResult const manifestBytes = root.Read(ManifestEntry);
    if (!manifestBytes.Succeeded())
    {
        error = fmt::format("{} cannot be read: {}", ManifestEntry, manifestBytes.Error);
        return std::nullopt;
    }
    BindReadResult const manifest = BindFile::Read(catalog, manifestBytes.Data);
    std::optional<TemplateManifestView> const manifestView = manifest.Ok() && manifest.Decoded.Object ? TemplateManifestView::From(*manifest.Decoded.Object) : std::nullopt;
    if (!manifestView)
    {
        error = fmt::format("{} does not read as a TemplateManifest: {}", ManifestEntry, manifest.Ok() ? std::string("its root is another class") : manifest.Detail);
        return std::nullopt;
    }
    ObjectTemplate found;
    for (PropertyValue const& entry : manifestView->GetSerializedTemplates())
    {
        std::optional<TemplateLocationView> const location = entry.AsObject() ? TemplateLocationView::From(*entry.AsObject()) : std::nullopt;
        if (location && location->GetId() == templateId)
        {
            found.File = location->GetFilename();
            break;
        }
    }
    if (found.File.empty())
    {
        error = fmt::format("{} lists no template {}", ManifestEntry, templateId);
        return std::nullopt;
    }

    KiwadReadResult const templateBytes = root.Read(found.File);
    if (!templateBytes.Succeeded())
    {
        error = fmt::format("template {} is in {}, which cannot be read: {}", templateId, found.File, templateBytes.Error);
        return std::nullopt;
    }
    BindReadResult const decoded = BindFile::Read(catalog, templateBytes.Data);
    std::optional<GameObjectTemplateView> const view = decoded.Ok() && decoded.Decoded.Object ? GameObjectTemplateView::From(*decoded.Decoded.Object) : std::nullopt;
    if (!view)
    {
        error = fmt::format("{} does not read as a GameObjectTemplate: {}", found.File, decoded.Ok() ? std::string("its root is another class") : decoded.Detail);
        return std::nullopt;
    }
    found.TemplateId = view->GetTemplateId() != 0 ? view->GetTemplateId() : templateId;
    found.ObjectName = view->GetObjectName();
    for (PropertyValue const& entry : view->GetBehaviors())
    {
        PropertyObject const* const behavior = entry.AsObject();
        PropertyValue const* const name = behavior ? behavior->Get("m_behaviorName") : nullptr;
        std::string const* const text = name ? name->GetIf<std::string>() : nullptr;
        if (!text || text->empty())
        {
            error = fmt::format("behavior {} of {} carries no m_behaviorName", found.Behaviors.size(), found.File);
            return std::nullopt;
        }
        found.Behaviors.push_back(*text);
    }
    return found;
}
