/*
 * Project Ambrose by Imjustchico
 * Registers commands by their normalized words, picks the longest whole-word match for a line, runs its handler outside the lock, lists a command group when only its prefix is typed, and hides sensitive arguments from logs.
 */

#include "ConsoleCommandTable.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>

namespace
{
    bool StartsWithWords(std::vector<std::string> const& words, std::vector<std::string> const& prefix)
    {
        if (prefix.size() > words.size())
            return false;
        for (std::size_t i = 0; i < prefix.size(); ++i)
            if (!Ambrose::EqualsIgnoreCase(words[i], prefix[i]))
                return false;
        return true;
    }

    std::string JoinWords(std::vector<std::string> const& words, std::size_t count)
    {
        std::string text;
        for (std::size_t i = 0; i < count && i < words.size(); ++i)
        {
            if (i)
                text += ' ';
            text += words[i];
        }
        return text;
    }
}

bool ConsoleCommandTable::Register(Command command)
{
    std::vector<std::string> words;
    for (std::string_view const word : Ambrose::Tokenize(command.Name, ' ', false))
        words.push_back(Ambrose::ToLower(word));
    if (words.empty() || !command.Run)
        return false;
    std::lock_guard const lock(_mutex);
    for (Entry const& entry : _entries)
        if (entry.Words == words)
            return false;
    command.Name = JoinWords(words, words.size());
    _entries.push_back({ std::move(command), std::move(words) });
    std::sort(_entries.begin(), _entries.end(), [](Entry const& left, Entry const& right) { return left.Definition.Name < right.Definition.Name; });
    return true;
}

bool ConsoleCommandTable::Unregister(std::string_view name)
{
    std::vector<std::string> words;
    for (std::string_view const word : Ambrose::Tokenize(name, ' ', false))
        words.push_back(Ambrose::ToLower(word));
    std::lock_guard const lock(_mutex);
    auto const removed = std::remove_if(_entries.begin(), _entries.end(), [&words](Entry const& entry) { return entry.Words == words; });
    bool const found = removed != _entries.end();
    _entries.erase(removed, _entries.end());
    return found;
}

ConsoleCommandTable::Entry const* ConsoleCommandTable::Find(std::vector<std::string> const& words) const
{
    Entry const* best = nullptr;
    for (Entry const& entry : _entries)
        if (StartsWithWords(words, entry.Words) && (!best || entry.Words.size() > best->Words.size()))
            best = &entry;
    return best;
}

std::vector<std::string> ConsoleCommandTable::DescribeMatching(std::vector<std::string> const& prefix) const
{
    std::vector<std::string> lines;
    for (Entry const& entry : _entries)
    {
        if (!StartsWithWords(entry.Words, prefix))
            continue;
        std::string line = entry.Definition.Name;
        if (!entry.Definition.Arguments.empty())
            line += ' ' + entry.Definition.Arguments;
        if (!entry.Definition.Help.empty())
            line += " - " + entry.Definition.Help;
        lines.push_back(std::move(line));
    }
    return lines;
}

ConsoleCommandTable::Result ConsoleCommandTable::Execute(std::string_view line, Reply const& reply) const
{
    std::vector<std::string> const words = Split(line);
    if (words.empty())
        return Result::Empty;

    Handler handler;
    std::string usage;
    std::vector<std::string> arguments;
    std::vector<std::string> group;
    std::size_t groupWords = 0;
    {
        std::lock_guard const lock(_mutex);
        if (Entry const* const entry = Find(words))
        {
            handler = entry->Definition.Run;
            usage = entry->Definition.Arguments.empty() ? entry->Definition.Name : entry->Definition.Name + ' ' + entry->Definition.Arguments;
            arguments.assign(words.begin() + static_cast<std::ptrdiff_t>(entry->Words.size()), words.end());
        }
        else
        {
            for (groupWords = words.size(); groupWords > 0 && group.empty(); --groupWords)
                group = DescribeMatching(std::vector<std::string>(words.begin(), words.begin() + static_cast<std::ptrdiff_t>(groupWords)));
            ++groupWords;
        }
    }

    if (!handler)
    {
        if (group.empty())
        {
            reply(fmt::format("Unknown command '{}'. Type 'help' to list commands.", words.front()));
            return Result::Unknown;
        }
        reply(fmt::format("Commands starting with '{}':", JoinWords(words, groupWords)));
        for (std::string const& entry : group)
            reply("  " + entry);
        return Result::Usage;
    }
    if (!handler(arguments, reply))
    {
        reply("Usage: " + usage);
        return Result::Usage;
    }
    return Result::Ran;
}

std::string ConsoleCommandTable::DescribeForLog(std::string_view line) const
{
    std::vector<std::string> const words = Split(line);
    if (words.empty())
        return {};
    std::lock_guard const lock(_mutex);
    Entry const* const entry = Find(words);
    if (!entry)
        return fmt::format("unknown command '{}'", words.front());
    if (entry->Definition.Sensitive)
        return words.size() > entry->Words.size() ? entry->Definition.Name + " (arguments hidden)" : entry->Definition.Name;
    return JoinWords(words, words.size());
}

std::vector<std::string> ConsoleCommandTable::DescribeCommands(std::string_view prefix) const
{
    std::vector<std::string> const words = Split(prefix);
    std::lock_guard const lock(_mutex);
    return DescribeMatching(words);
}

std::vector<std::string> ConsoleCommandTable::Split(std::string_view line)
{
    std::vector<std::string> words;
    std::string current;
    bool inWord = false;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i)
    {
        char const c = line[i];
        if (quoted)
        {
            if (c == '\\' && i + 1 < line.size() && (line[i + 1] == '"' || line[i + 1] == '\\'))
                current += line[++i];
            else if (c == '"')
                quoted = false;
            else
                current += c;
        }
        else if (c == '"')
        {
            quoted = true;
            inWord = true;
        }
        else if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            if (inWord)
                words.push_back(std::move(current));
            current.clear();
            inWord = false;
        }
        else
        {
            current += c;
            inWord = true;
        }
    }
    if (inWord)
        words.push_back(std::move(current));
    return words;
}
