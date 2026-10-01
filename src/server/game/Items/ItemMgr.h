/*
 * Project Ambrose by Imjustchico
 * Every item template the user's install holds (sItemMgr), read from the templates TemplateManifest.xml lists under ObjectData/ through the template store's manifest, decoded once on every hardware thread, each whose class is or derives from WizItemTemplate kept as a typed item record, and swapped in as one checked set through the reload target item_template, which follows templates. A set in which an item fails to load, such as one holding an object of a class the type dump does not list, keeps the one serving and reports every item that failed, grouped by what went wrong, and a caller holds the set it was given for as long as it needs it.
 */

#ifndef AMBROSE_ITEMMGR_H
#define AMBROSE_ITEMMGR_H

#include "ItemTemplateStore.h"
#include "ReloadableStore.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

class ItemMgr
{
public:
    static constexpr std::string_view Target = "item_template";
    static constexpr std::string_view Folder = "ObjectData/";

    static ItemMgr& Instance();

    ItemMgr() = default;
    ItemMgr(ItemMgr const&) = delete;
    ItemMgr& operator=(ItemMgr const&) = delete;

    void SetInstall(std::filesystem::path root);
    void RegisterReloadTargets();
    bool Load(std::vector<std::string>& errors);
    std::shared_ptr<ItemTemplateStore const> GetItems() const { return _items.Get(); }
    uint64 GetGeneration() const noexcept { return _items.GetGeneration(); }
    void Clear();

private:
    std::filesystem::path GameData() const;

    mutable std::mutex _installMutex;
    std::filesystem::path _install;
    ReloadableStore<ItemTemplateStore> _items;
};

#define sItemMgr ItemMgr::Instance()

#endif
