/*
 * Project Ambrose by Imjustchico
 * Every sigil the user's install holds (sSigilMgr), combat, PvP, battleground, dynamic and minigame alike, read from the templates TemplateManifest.xml lists under Sigils/ into typed sigil records and swapped in as one checked set through the reload target sigils, which follows templates. A set that fails to load keeps the one serving and reports every sigil that failed, and a duel holds the set it began with for as long as it runs.
 */

#ifndef AMBROSE_SIGILMGR_H
#define AMBROSE_SIGILMGR_H

#include "NameKeyedTemplates.h"
#include "ReloadableStore.h"
#include "SigilInfo.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

using SigilStore = NameKeyedTemplates<SigilInfo>;

class SigilMgr
{
public:
    static constexpr std::string_view Target = "sigils";
    static constexpr std::string_view Folder = "Sigils/";

    static SigilMgr& Instance();

    SigilMgr() = default;
    SigilMgr(SigilMgr const&) = delete;
    SigilMgr& operator=(SigilMgr const&) = delete;

    void SetInstall(std::filesystem::path root);
    void RegisterReloadTargets();
    bool Load(std::vector<std::string>& errors);
    std::shared_ptr<SigilStore const> GetSigils() const { return _sigils.Get(); }
    void Clear();

private:
    std::filesystem::path GameData() const;

    mutable std::mutex _installMutex;
    std::filesystem::path _install;
    ReloadableStore<SigilStore> _sigils;
};

#define sSigilMgr SigilMgr::Instance()

#endif
