/*
 * Project Ambrose by Imjustchico
 * Swaps the Patch settings and the served manifest atomically and logs what each load found, builds a new manifest off to the side from the current settings and publishes it only when it read and validated whole, registers that load as the manifest reload target after the config target, rereads the Patch options when a configuration reload changed one and reloads the manifest when its folder or revision moved, and fills the file list reply: version 1, LatestFileList.bin of type 1 with the manifest's time, size and CRC, the list's URL under the public address and the revision folder, that folder as the URL prefix, no suffix, and the client's locale echoed.
 */

#include "PatchMgr.h"
#include "ConfigMgr.h"
#include "Log.h"
#include "ReloadMgr.h"

#include <fmt/format.h>

#include <algorithm>
#include <utility>

PatchMgr& PatchMgr::Instance()
{
    static PatchMgr instance;
    return instance;
}

PatchMgr::PatchMgr() : _settings(std::make_shared<PatchSettings const>())
{
}

void PatchMgr::LoadSettings(ConfigMgr const& config)
{
    std::vector<std::string> problems;
    PatchSettings settings = PatchSettings::Load(config, &problems);
    for (std::string const& problem : problems)
        LOG_WARN("server.patchserver", "{}", problem);
    SetSettings(std::move(settings));
}

void PatchMgr::SetSettings(PatchSettings settings)
{
    _settings.store(std::make_shared<PatchSettings const>(std::move(settings)));
}

std::shared_ptr<PatchSettings const> PatchMgr::GetSettings() const
{
    return _settings.load();
}

bool PatchMgr::LoadManifest(std::vector<std::string>& errors)
{
    std::shared_ptr<PatchSettings const> const settings = GetSettings();
    std::vector<std::string> found;
    std::optional<PatchManifest> loaded = PatchManifest::Load(settings->OutputDir, settings->Revision, found);
    if (!loaded)
    {
        std::shared_ptr<PatchManifest const> const serving = GetManifest();
        for (std::string const& problem : found)
            LOG_ERROR("server.patchserver", "The manifest was not loaded: {}", problem);
        if (serving)
            LOG_ERROR("server.patchserver", "The manifest of {} loaded before goes on serving", serving->Revision);
        else
            LOG_ERROR("server.patchserver", "No manifest is serving, so file list requests go unanswered until one loads");
        errors.insert(errors.end(), found.begin(), found.end());
        return false;
    }
    auto manifest = std::make_shared<PatchManifest const>(std::move(*loaded));
    LOG_INFO("server.patchserver", "Serving the manifest of {}: {} packages listing {} files in {}, {} bytes with CRC {:08X}", manifest->Revision, manifest->Packages, manifest->Files,
        ConfigMgr::PathToUtf8(manifest->ListFile), manifest->ListFileSize, manifest->ListFileCrc);
    _manifest.store(std::move(manifest));
    return true;
}

std::shared_ptr<PatchManifest const> PatchMgr::GetManifest() const
{
    return _manifest.load();
}

bool PatchMgr::RegisterReloadTarget()
{
    return sReloadMgr.Register(std::string(ReloadTarget), [this](std::vector<std::string>& errors) { return LoadManifest(errors); }, { "config" });
}

void PatchMgr::UnregisterReloadTarget()
{
    sReloadMgr.Unregister(ReloadTarget);
}

bool PatchMgr::IsPatchOption(std::string_view key) noexcept
{
    return key.starts_with("Patch.") || key == "Http.PublicBaseUrl";
}

bool PatchMgr::ApplyConfig(ConfigMgr const& config, std::vector<std::string> const& changed)
{
    if (std::none_of(changed.begin(), changed.end(), [](std::string const& key) { return IsPatchOption(key); }))
        return true;
    std::shared_ptr<PatchSettings const> const before = GetSettings();
    LoadSettings(config);
    std::shared_ptr<PatchSettings const> const after = GetSettings();
    if (after->OutputDir == before->OutputDir && after->Revision == before->Revision)
        return true;
    std::vector<std::string> errors;
    return LoadManifest(errors);
}

PatchMessages::LatestFileListV2 PatchMgr::BuildReply(PatchManifest const& manifest, PatchSettings const& settings, std::string locale)
{
    PatchMessages::LatestFileListV2 reply;
    reply.LatestVersion = LatestVersion;
    reply.ListFileName = std::string(PatchManifest::ListFileName);
    reply.ListFileType = ListFileType;
    reply.ListFileTime = manifest.ListFileTime;
    reply.ListFileSize = manifest.ListFileSize;
    reply.ListFileCrc = manifest.ListFileCrc;
    reply.UrlPrefix = fmt::format("{}/{}", settings.PublicBaseUrl, manifest.GetDirectoryName());
    reply.ListFileUrl = fmt::format("{}/{}/{}", reply.UrlPrefix, ListFilePlatform, PatchManifest::ListFileName);
    reply.Locale = std::move(locale);
    return reply;
}

PatchMessages::LatestFileList PatchMgr::BuildReplyV1(PatchManifest const& manifest, PatchSettings const& settings)
{
    PatchMessages::LatestFileListV2 const full = BuildReply(manifest, settings, {});
    PatchMessages::LatestFileList reply;
    reply.LatestVersion = full.LatestVersion;
    reply.ListFileName = full.ListFileName;
    reply.ListFileType = full.ListFileType;
    reply.ListFileTime = full.ListFileTime;
    reply.ListFileSize = full.ListFileSize;
    reply.ListFileCrc = full.ListFileCrc;
    reply.ListFileUrl = full.ListFileUrl;
    reply.UrlPrefix = full.UrlPrefix;
    reply.UrlSuffix = full.UrlSuffix;
    return reply;
}

void PatchMgr::Reset()
{
    SetSettings(PatchSettings{});
    _manifest.store(nullptr);
}
