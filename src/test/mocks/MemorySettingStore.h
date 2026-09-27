/*
 * Project Ambrose by Imjustchico
 * A settings store held in memory for tests: it keeps the values and every write in order, answers a key's history newest first from those writes, and can be told to refuse every write, so a test sees exactly what the registry persisted and audited, and that a refused batch left nothing behind.
 */

#ifndef AMBROSE_MEMORYSETTINGSTORE_H
#define AMBROSE_MEMORYSETTINGSTORE_H

#include "Settings.h"

#include <map>
#include <mutex>
#include <span>
#include <string>
#include <vector>

class MemorySettingStore : public SettingStore
{
public:
    bool Load(std::map<std::string, std::string, std::less<>>& values, std::string&) override
    {
        std::lock_guard const lock(_mutex);
        values = Values;
        return true;
    }

    bool Write(SettingWrite const& write, std::string& error) override
    {
        return WriteMany(std::span<SettingWrite const>(&write, 1), error);
    }

    bool WriteMany(std::span<SettingWrite const> writes, std::string& error) override
    {
        std::lock_guard const lock(_mutex);
        if (FailWrites)
        {
            error = "the test refuses every write";
            return false;
        }
        ++Commits;
        for (SettingWrite const& write : writes)
        {
            Writes.push_back(write);
            if (write.Persisted)
                Values[write.Key] = *write.Persisted;
            else
                Values.erase(write.Key);
        }
        return true;
    }

    bool History(std::string const& key, std::size_t limit, std::vector<SettingAuditEntry>& entries, std::string&) override
    {
        std::lock_guard const lock(_mutex);
        for (auto write = Writes.rbegin(); write != Writes.rend() && entries.size() < limit; ++write)
            if (write->Key == key)
                entries.push_back({ static_cast<uint64>(Writes.rend() - write), write->Key, write->OldValue, write->NewValue, write->Author.Who, write->Author.AccountId,
                    write->Author.Source, write->Reason, write->EpochSeconds });
        return true;
    }

    std::map<std::string, std::string, std::less<>> Values;
    std::vector<SettingWrite> Writes;
    std::size_t Commits = 0;
    bool FailWrites = false;

private:
    std::mutex _mutex;
};

#endif
