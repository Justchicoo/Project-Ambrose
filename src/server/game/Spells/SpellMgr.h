/*
 * Project Ambrose by Imjustchico
 * Every spell the user's install holds (sSpellMgr), read from the templates TemplateManifest.xml lists under Spells/ through the template store's manifest, decoded once on every hardware thread into typed spell records and swapped in as one checked set through the reload target spells, which follows templates. A set that fails to load keeps the one serving and reports every spell that failed, grouped by what went wrong, and a caller holds the set it was given for as long as it needs it.
 */

#ifndef AMBROSE_SPELLMGR_H
#define AMBROSE_SPELLMGR_H

#include "NameKeyedTemplates.h"
#include "ReloadableStore.h"
#include "SpellInfo.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

using SpellStore = NameKeyedTemplates<SpellInfo>;

class SpellMgr
{
public:
    static constexpr std::string_view Target = "spells";
    static constexpr std::string_view Folder = "Spells/";

    static SpellMgr& Instance();

    SpellMgr() = default;
    SpellMgr(SpellMgr const&) = delete;
    SpellMgr& operator=(SpellMgr const&) = delete;

    void SetInstall(std::filesystem::path root);
    void RegisterReloadTargets();
    bool Load(std::vector<std::string>& errors);
    std::shared_ptr<SpellStore const> GetSpells() const { return _spells.Get(); }
    void Clear();

private:
    std::filesystem::path GameData() const;

    mutable std::mutex _installMutex;
    std::filesystem::path _install;
    ReloadableStore<SpellStore> _spells;
};

#define sSpellMgr SpellMgr::Instance()

#endif
