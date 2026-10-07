/*
 * Project Ambrose by Imjustchico
 * Reads every template under ObjectData/ through the template folder reader, keeps each item template as a record at its own position and passes over every other template, then builds the set and swaps it in, logging how many items of each class it holds, how many of their behaviors are of classes nothing describes, the memory the set takes and how long it took.
 */

#include "ItemMgr.h"
#include "Log.h"
#include "ObjectTemplateMgr.h"
#include "ReloadMgr.h"
#include "TemplateFolder.h"

#include <fmt/format.h>

#include <chrono>
#include <optional>
#include <utility>

namespace
{
    constexpr char const* ItemLog = "server.loading";
}

ItemMgr& ItemMgr::Instance()
{
    static ItemMgr instance;
    return instance;
}

void ItemMgr::SetInstall(std::filesystem::path root)
{
    std::lock_guard const lock(_installMutex);
    _install = std::move(root);
}

void ItemMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(Target), [this](std::vector<std::string>& errors) { return Load(errors); }, { std::string(ObjectTemplateMgr::ManifestTarget) });
}

std::filesystem::path ItemMgr::GameData() const
{
    std::lock_guard const lock(_installMutex);
    return _install.empty() ? std::filesystem::path() : _install / "Data" / "GameData";
}

bool ItemMgr::Load(std::vector<std::string>& errors)
{
    std::filesystem::path const gameData = GameData();
    if (gameData.empty())
    {
        errors.emplace_back("no Wizard101 install is in use, so no item can be read");
        return false;
    }
    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    if (!catalog)
    {
        errors.emplace_back("no type dump is loaded, so no item can be read");
        return false;
    }
    std::shared_ptr<TemplateManifest const> const manifest = sObjectTemplateMgr.GetManifest();
    if (manifest->Size() == 0)
    {
        errors.emplace_back("the template manifest is not loaded, so no item can be found");
        return false;
    }
    auto const started = std::chrono::steady_clock::now();
    std::vector<TemplateFolderEntry> const entries = TemplateFolder::List(*manifest, Folder);
    if (entries.empty())
    {
        errors.push_back(fmt::format("{} lists no template under {}", TemplateManifest::Entry, Folder));
        return false;
    }
    std::vector<std::optional<ItemTemplateRecord>> decoded(entries.size());
    std::size_t threads = 0;
    bool const read = TemplateFolder::ReadAll(gameData, catalog, entries, "templates", Folder,
        [&decoded](std::size_t index, TemplateFolderEntry const& entry, PropertyObject const& object, std::vector<DecodeIssue> const& issues, std::string& error)
        {
            if (!ItemTemplateRecord::IsItem(object))
                return true;
            decoded[index] = ItemTemplateRecord::Read(object, entry.Id, entry.Location.Path, issues, error);
            return decoded[index].has_value();
        },
        errors, threads);
    if (!read)
        return false;

    std::vector<ItemTemplateRecord> records;
    for (std::optional<ItemTemplateRecord>& record : decoded)
        if (record)
            records.push_back(std::move(*record));
    if (records.empty())
    {
        errors.push_back(fmt::format("none of the {} templates under {} is a {}", entries.size(), Folder, ItemTemplateRecord::ItemClass));
        return false;
    }
    std::shared_ptr<ItemTemplateStore const> store = ItemTemplateStore::Build(std::move(records));
    std::string classes;
    for (auto const& [name, count] : store->CountByClass())
        classes += fmt::format("{}{} {}", classes.empty() ? "" : ", ", count, name);
    std::size_t const count = store->Size();
    std::size_t const unknown = store->CountUnknownBehaviors();
    std::size_t const memory = store->GetMemoryUsage();
    _items.Replace(std::move(store));
    auto const took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    LOG_INFO(ItemLog, "Read {} item templates ({}) from the {} templates under {}, {} of their behaviors of classes nothing describes, holding {:.1f} MiB, in {} ms on {} thread(s)",
        count, classes, entries.size(), Folder, unknown, static_cast<double>(memory) / (1024.0 * 1024.0), took.count(), threads);
    return true;
}

void ItemMgr::Clear()
{
    _items.Replace(std::make_shared<ItemTemplateStore const>());
}
