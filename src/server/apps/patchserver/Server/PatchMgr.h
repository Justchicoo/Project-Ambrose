/*
 * Project Ambrose by Imjustchico
 * State the patch server's sessions share: the live Patch settings and the manifest being served, each swapped whole so a session reads one consistent snapshot without locking, a manifest reload, offered to the console and the admin API as the manifest reload target, that keeps the one serving when the new one fails, which options a configuration reload must hand it, and the reply MSG_LATEST_FILE_LIST_V2 and MSG_LATEST_FILE_LIST give for a manifest.
 */

#ifndef AMBROSE_PATCHMGR_H
#define AMBROSE_PATCHMGR_H

#include "PatchManifest.h"
#include "PatchMessages.h"
#include "PatchSettings.h"

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class ConfigMgr;

class PatchMgr
{
public:
    static constexpr uint32 LatestVersion = 1;
    static constexpr uint32 ListFileType = 1;
    static constexpr std::string_view ListFilePlatform = "Windows";
    static constexpr std::string_view ReloadTarget = "manifest";

    static PatchMgr& Instance();

    void LoadSettings(ConfigMgr const& config);
    void SetSettings(PatchSettings settings);
    std::shared_ptr<PatchSettings const> GetSettings() const;

    bool LoadManifest(std::vector<std::string>& errors);
    std::shared_ptr<PatchManifest const> GetManifest() const;
    bool RegisterReloadTarget();
    void UnregisterReloadTarget();
    bool ApplyConfig(ConfigMgr const& config, std::vector<std::string> const& changed);
    static bool IsPatchOption(std::string_view key) noexcept;

    static PatchMessages::LatestFileListV2 BuildReply(PatchManifest const& manifest, PatchSettings const& settings, std::string locale);
    static PatchMessages::LatestFileList BuildReplyV1(PatchManifest const& manifest, PatchSettings const& settings);

    void Reset();

private:
    PatchMgr();

    std::atomic<std::shared_ptr<PatchSettings const>> _settings;
    std::atomic<std::shared_ptr<PatchManifest const>> _manifest;
};

#define sPatchMgr PatchMgr::Instance()

#endif
