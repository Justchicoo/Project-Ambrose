/*
 * Project Ambrose by Imjustchico
 * Splits each name into its camel-case words, a run of capitals before a capitalized word standing on its own and digits apart, and builds names from them while continuing one djb2 hash word by word, so millions of names cost one pass; each type turns a wanted hash into the djb2 value its name must have, which only half of all values can be, so a random hash is named by a search of n names over t types with probability 1 - e^(-nt/2^32).
 */

#include "PropertyWordSearch.h"
#include "StringHash.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <unordered_map>
#include <utility>

namespace
{
    constexpr std::string_view Prefix = "m_";

    uint32 Continue(uint32 hash, std::string_view text)
    {
        for (char const character : text)
            hash = (hash << 5) + hash + static_cast<uint8>(character);
        return hash;
    }

    std::string Capitalized(std::string word)
    {
        if (!word.empty())
            word.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(word.front())));
        return word;
    }

    std::string Lowered(std::string word)
    {
        if (!word.empty())
            word.front() = static_cast<char>(std::tolower(static_cast<unsigned char>(word.front())));
        return word;
    }

    bool IsUpper(char c) { return std::isupper(static_cast<unsigned char>(c)) != 0; }
    bool IsLower(char c) { return std::islower(static_cast<unsigned char>(c)) != 0; }
    bool IsDigit(char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }
}

std::vector<std::string_view> PropertyWordSearch::DefaultTypes()
{
    return { "bool", "int", "unsigned int", "float", "double", "std::string", "std::wstring", "unsigned char", "char", "short", "unsigned short", "unsigned __int64", "__int64",
        "gid" };
}

std::vector<std::string> PropertyWordSearch::Split(std::string_view name)
{
    if (name.starts_with(Prefix))
        name.remove_prefix(Prefix.size());
    std::vector<std::string> words;
    std::size_t at = 0;
    while (at < name.size())
    {
        std::size_t end = at;
        if (IsDigit(name[at]))
            while (end < name.size() && IsDigit(name[end]))
                ++end;
        else if (IsUpper(name[at]))
        {
            while (end < name.size() && IsUpper(name[end]))
                ++end;
            if (end < name.size() && IsLower(name[end]))
            {
                if (end - at > 1)
                    --end;
                else
                    while (end < name.size() && IsLower(name[end]))
                        ++end;
            }
        }
        else if (IsLower(name[at]))
            while (end < name.size() && IsLower(name[end]))
                ++end;
        else
        {
            ++at;
            continue;
        }
        words.emplace_back(name.substr(at, end - at));
        at = end;
    }
    return words;
}

std::vector<std::string> PropertyWordSearch::Words(std::vector<std::string> const& names)
{
    std::set<std::string> words;
    for (std::string const& name : names)
        for (std::string& word : Split(name))
            words.insert(Capitalized(std::move(word)));
    return { words.begin(), words.end() };
}

PropertyWordSearchResult PropertyWordSearch::Run(std::vector<uint32> const& hashes, std::vector<std::string> const& types, std::vector<std::string> const& words,
    std::vector<std::string> const& anchors)
{
    std::unordered_map<uint32, std::vector<std::pair<uint32, std::string const*>>> wanted;
    for (uint32 const hash : hashes)
        for (std::string const& type : types)
        {
            uint32 const needed = hash - StringHash::KiStringHash(type);
            if (needed <= 0x7FFFFFFFu)
                wanted[needed].emplace_back(hash, &type);
        }

    PropertyWordSearchResult result;
    std::set<std::pair<uint32, std::pair<std::string, std::string>>> found;
    auto const check = [&](uint32 djb2, auto&& spell)
    {
        ++result.Names;
        auto const hit = wanted.find(djb2 & 0x7FFFFFFFu);
        if (hit == wanted.end())
            return;
        std::string const name = spell();
        for (auto const& [hash, type] : hit->second)
            if (StringHash::PropertyHash(*type, name) == hash)
                found.insert({ hash, { *type, name } });
    };

    std::vector<std::string> firsts;
    firsts.reserve(words.size());
    for (std::string const& word : words)
        firsts.push_back(Lowered(word));
    uint32 const start = Continue(5381, Prefix);
    for (std::string const& first : firsts)
    {
        uint32 const one = Continue(start, first);
        check(one, [&] { return std::string(Prefix).append(first); });
        for (std::string const& second : words)
        {
            uint32 const two = Continue(one, second);
            check(two, [&] { return std::string(Prefix).append(first).append(second); });
        }
    }
    for (std::string const& anchor : anchors)
    {
        std::string const leading = Lowered(anchor);
        std::string const inner = Capitalized(anchor);
        for (std::string const& first : firsts)
        {
            uint32 const lead = Continue(Continue(start, leading), Capitalized(first));
            uint32 const one = Continue(start, first);
            uint32 const middle = Continue(one, inner);
            for (std::string const& second : words)
            {
                check(Continue(lead, second), [&] { return std::string(Prefix).append(leading).append(Capitalized(first)).append(second); });
                check(Continue(middle, second), [&] { return std::string(Prefix).append(first).append(inner).append(second); });
                check(Continue(Continue(one, second), inner), [&] { return std::string(Prefix).append(first).append(second).append(inner); });
            }
        }
    }
    for (auto const& [hash, named] : found)
        result.Matches.push_back({ hash, named.first, named.second });
    double const expected = static_cast<double>(result.Names) * static_cast<double>(types.size()) / 4294967296.0;
    result.Chance = 1.0 - std::exp(-expected);
    return result;
}
