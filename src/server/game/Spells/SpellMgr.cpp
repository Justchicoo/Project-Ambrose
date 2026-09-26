/*
 * Project Ambrose by Imjustchico
 * Reads the spells through the template folder reader, each decoded template read into a spell record at its own position, and the tiered spell groups from Root.wad as the install holds them now, a file that does not read failing the set like a spell that does not, then gives each tiered spell its group and builds the set, which checks that each spell's id is the hash of its name, before swapping it in and logging how many spells and effects it holds and how long they took.
 */

#include "SpellMgr.h"
#include "ConfigMgr.h"
#include "KiwadArchive.h"
#include "Log.h"
#include "ObjectTemplateMgr.h"
#include "ReloadMgr.h"
#include "TemplateFolder.h"
#include "TieredSpellGroups.h"

#include <fmt/format.h>

#include <chrono>
#include <optional>
#include <utility>

namespace
{
    constexpr char const* SpellLog = "server.loading";
}

SpellMgr& SpellMgr::Instance()
{
    static SpellMgr instance;
    return instance;
}

void SpellMgr::SetInstall(std::filesystem::path root)
{
    std::lock_guard const lock(_installMutex);
    _install = std::move(root);
}

void SpellMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(Target), [this](std::vector<std::string>& errors) { return Load(errors); }, { std::string(ObjectTemplateMgr::ManifestTarget) });
}

std::filesystem::path SpellMgr::GameData() const
{
    std::lock_guard const lock(_installMutex);
    return _install.empty() ? std::filesystem::path() : _install / "Data" / "GameData";
}

bool SpellMgr::Load(std::vector<std::string>& errors)
{
    std::filesystem::path const gameData = GameData();
    if (gameData.empty())
    {
        errors.emplace_back("no Wizard101 install is in use, so no spell can be read");
        return false;
    }
    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    if (!catalog)
    {
        errors.emplace_back("no type dump is loaded, so no spell can be read");
        return false;
    }
    std::shared_ptr<TemplateManifest const> const manifest = sObjectTemplateMgr.GetManifest();
    if (manifest->Size() == 0)
    {
        errors.emplace_back("the template manifest is not loaded, so no spell can be found");
        return false;
    }
    auto const started = std::chrono::steady_clock::now();
    std::vector<TemplateFolderEntry> const entries = TemplateFolder::List(*manifest, Folder);
    if (entries.empty())
    {
        errors.push_back(fmt::format("{} lists no template under {}", TemplateManifest::Entry, Folder));
        return false;
    }
    std::vector<std::optional<SpellInfo>> decoded(entries.size());
    std::size_t threads = 0;
    bool const read = TemplateFolder::ReadAll(gameData, catalog, entries, "spells", Folder,
        [&decoded](std::size_t index, TemplateFolderEntry const& entry, PropertyObject const& object, std::string& error)
        {
            decoded[index] = SpellInfo::Read(object, entry.Id, entry.Location.Path, error);
            return decoded[index].has_value();
        },
        errors, threads);
    if (!read)
        return false;

    std::string error;
    std::filesystem::path const rootWad = gameData / TemplateManifest::RootArchive;
    std::shared_ptr<KiwadArchive const> const root = KiwadArchive::Open(rootWad, error);
    if (!root)
    {
        errors.push_back(fmt::format("{} cannot be opened for the tiered spell groups: {}", ConfigMgr::PathToUtf8(rootWad), error));
        return false;
    }
    std::optional<TieredSpellGroups> const groups = TieredSpellGroups::Read(*root, catalog, errors);
    if (!groups)
        return false;

    std::vector<SpellInfo> spells;
    spells.reserve(decoded.size());
    std::size_t tiered = 0;
    for (std::optional<SpellInfo>& spell : decoded)
    {
        if (spell->Tiered)
        {
            spell->TieredGroupIndex = groups->Find(spell->Name).value_or(SpellInfo::NoTieredGroup);
            ++tiered;
        }
        spells.push_back(std::move(*spell));
    }
    std::shared_ptr<SpellStore const> store = SpellStore::Build(std::move(spells), errors);
    if (!store)
        return false;
    std::size_t effects = 0;
    for (SpellInfo const& spell : store->GetAll())
        effects += spell.CountEffects();
    std::size_t const count = store->Size();
    _spells.Replace(std::move(store));
    auto const took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    LOG_INFO(SpellLog, "Read {} spells with {} effects from {}, {} of them tiered with {} named in {} tiered spell groups, in {} ms on {} thread(s)", count, effects, Folder, tiered,
        groups->Size(), groups->CountGroups(), took.count(), threads);
    return true;
}

void SpellMgr::Clear()
{
    _spells.Replace(std::make_shared<SpellStore const>());
}
