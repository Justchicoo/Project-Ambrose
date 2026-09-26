/*
 * Project Ambrose by Imjustchico
 * Reads the sigils through the template folder reader, each decoded template read into a sigil record at its own position, then builds the set, which checks that each sigil's id is the hash of its name, before swapping it in and logging how many sigils it holds, how many of them are combat sigils and how long they took.
 */

#include "SigilMgr.h"
#include "Log.h"
#include "ObjectTemplateMgr.h"
#include "ReloadMgr.h"
#include "TemplateFolder.h"

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <optional>
#include <utility>

namespace
{
    constexpr char const* SigilLog = "server.loading";
}

SigilMgr& SigilMgr::Instance()
{
    static SigilMgr instance;
    return instance;
}

void SigilMgr::SetInstall(std::filesystem::path root)
{
    std::lock_guard const lock(_installMutex);
    _install = std::move(root);
}

void SigilMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(Target), [this](std::vector<std::string>& errors) { return Load(errors); }, { std::string(ObjectTemplateMgr::ManifestTarget) });
}

std::filesystem::path SigilMgr::GameData() const
{
    std::lock_guard const lock(_installMutex);
    return _install.empty() ? std::filesystem::path() : _install / "Data" / "GameData";
}

bool SigilMgr::Load(std::vector<std::string>& errors)
{
    std::filesystem::path const gameData = GameData();
    if (gameData.empty())
    {
        errors.emplace_back("no Wizard101 install is in use, so no sigil can be read");
        return false;
    }
    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    if (!catalog)
    {
        errors.emplace_back("no type dump is loaded, so no sigil can be read");
        return false;
    }
    std::shared_ptr<TemplateManifest const> const manifest = sObjectTemplateMgr.GetManifest();
    if (manifest->Size() == 0)
    {
        errors.emplace_back("the template manifest is not loaded, so no sigil can be found");
        return false;
    }
    auto const started = std::chrono::steady_clock::now();
    std::vector<TemplateFolderEntry> const entries = TemplateFolder::List(*manifest, Folder);
    if (entries.empty())
    {
        errors.push_back(fmt::format("{} lists no template under {}", TemplateManifest::Entry, Folder));
        return false;
    }
    std::vector<std::optional<SigilInfo>> decoded(entries.size());
    std::size_t threads = 0;
    bool const read = TemplateFolder::ReadAll(gameData, catalog, entries, "sigils", Folder,
        [&decoded](std::size_t index, TemplateFolderEntry const& entry, PropertyObject const& object, std::string& error)
        {
            decoded[index] = SigilInfo::Read(object, entry.Id, entry.Location.Path, error);
            return decoded[index].has_value();
        },
        errors, threads);
    if (!read)
        return false;

    std::vector<SigilInfo> sigils;
    sigils.reserve(decoded.size());
    for (std::optional<SigilInfo>& sigil : decoded)
        sigils.push_back(std::move(*sigil));
    std::shared_ptr<SigilStore const> store = SigilStore::Build(std::move(sigils), errors);
    if (!store)
        return false;
    std::size_t const combat = static_cast<std::size_t>(std::count_if(store->GetAll().begin(), store->GetAll().end(), [](SigilInfo const& sigil) { return sigil.Combat; }));
    std::size_t const count = store->Size();
    _sigils.Replace(std::move(store));
    auto const took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    LOG_INFO(SigilLog, "Read {} sigils, {} of them combat sigils, from {} in {} ms", count, combat, Folder, took.count());
    return true;
}

void SigilMgr::Clear()
{
    _sigils.Replace(std::make_shared<SigilStore const>());
}
