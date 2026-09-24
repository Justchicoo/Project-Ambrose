/*
 * Project Ambrose by Imjustchico
 * Keeps one group per place in the code an error was raised at, counting repeats rather than keeping every record, and dropping the group seen longest ago when the store is full so an app that raises errors forever does not grow forever. Noting a record never throws and never logs, because it is called from inside the dispatch that is delivering one.
 */

#include "LogErrors.h"

#include "GitRevision.h"

#include <algorithm>
#include <exception>

std::string LogErrorStore::KeyOf(LogMessage const& message)
{
    std::string key;
    key.reserve(message.Category.size() + message.Source.File.size() + message.Template.size() + 24);
    key.append(message.Category);
    key.push_back('\x1f');
    key.append(message.Source.File);
    key.push_back('\x1f');
    key.append(std::to_string(message.Source.Line));
    key.push_back('\x1f');
    key.append(message.Template);
    return key;
}

void LogErrorStore::Note(LogMessage const& message) noexcept
{
    try
    {
        std::lock_guard const lock(_mutex);
        std::string key = KeyOf(message);
        auto found = _groups.find(key);
        if (found == _groups.end())
        {
            if (_capacity == 0)
                return;
            while (_groups.size() >= _capacity)
                EvictOldest();
            LogErrorGroup group;
            group.Category = message.Category;
            group.File = LogSourcePath::Portable(message.Source.File);
            group.Line = message.Source.Line;
            group.Function.assign(message.Source.Function);
            group.Template = message.Template;
            group.Revision = GitRevision::GetHash();
            group.FirstSeen = message.Time;
            found = _groups.emplace(std::move(key), Entry{ std::move(group), ++_order }).first;
        }
        Entry& entry = found->second;
        entry.Group.Level = message.Level;
        entry.Group.Count += 1;
        entry.Group.LastSeen = message.Time;
        entry.Group.LastMessage = message.Text;
        entry.Order = ++_order;
    }
    catch (std::exception const&)
    {
    }
    catch (...)
    {
    }
}

void LogErrorStore::EvictOldest()
{
    auto oldest = _groups.begin();
    for (auto it = _groups.begin(); it != _groups.end(); ++it)
        if (it->second.Order < oldest->second.Order)
            oldest = it;
    if (oldest != _groups.end())
    {
        _groups.erase(oldest);
        _dropped += 1;
    }
}

std::vector<LogErrorGroup> LogErrorStore::Groups() const
{
    std::lock_guard const lock(_mutex);
    std::vector<LogErrorGroup> out;
    out.reserve(_groups.size());
    for (auto const& [key, entry] : _groups)
        out.push_back(entry.Group);
    std::sort(out.begin(), out.end(), [](LogErrorGroup const& left, LogErrorGroup const& right)
    {
        if (left.LastSeen != right.LastSeen)
            return left.LastSeen > right.LastSeen;
        return left.Count > right.Count;
    });
    return out;
}

std::size_t LogErrorStore::Size() const
{
    std::lock_guard const lock(_mutex);
    return _groups.size();
}

void LogErrorStore::Clear()
{
    std::lock_guard const lock(_mutex);
    _groups.clear();
}

void LogErrorStore::SetCapacity(std::size_t capacity)
{
    std::lock_guard const lock(_mutex);
    _capacity = std::min(capacity, MaxCapacity);
    while (_groups.size() > _capacity)
        EvictOldest();
}

std::size_t LogErrorStore::GetCapacity() const
{
    std::lock_guard const lock(_mutex);
    return _capacity;
}

uint64 LogErrorStore::GetDropped() const
{
    std::lock_guard const lock(_mutex);
    return _dropped;
}
