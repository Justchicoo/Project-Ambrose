/*
 * Project Ambrose by Imjustchico
 * Where the supervisor's own live settings persist: a SettingStore over the panel store's settings and setting_audit tables, holding the same lock the panel takes for every other write to its store, so a change and its audit row land in one transaction beside everything else the panel records, and refusing every read and write plainly while the store is closed.
 */

#ifndef AMBROSE_PANELSETTINGSTORE_H
#define AMBROSE_PANELSETTINGSTORE_H

#include "Settings.h"

#include <cstddef>
#include <map>
#include <mutex>
#include <span>
#include <string>
#include <vector>

class PanelStore;

class PanelSettingStore final : public SettingStore
{
public:
    PanelSettingStore(PanelStore& store, std::mutex& mutex);

    bool Load(std::map<std::string, std::string, std::less<>>& values, std::string& error) override;
    bool Write(SettingWrite const& write, std::string& error) override;
    bool WriteMany(std::span<SettingWrite const> writes, std::string& error) override;
    bool History(std::string const& key, std::size_t limit, std::vector<SettingAuditEntry>& entries, std::string& error) override;

private:
    PanelStore& _store;
    std::mutex& _mutex;
};

#endif
