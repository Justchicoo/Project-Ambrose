/*
 * Project Ambrose by Imjustchico
 * Loads and checks the five UTF-16 chat-filter files from Root.wad, rejecting malformed entries and preserving the serving snapshot whenever any required file cannot be read.
 */

#include "ChatFilter.h"

#include "ConfigMgr.h"
#include "KiwadArchive.h"
#include "Log.h"
#include "ReloadMgr.h"
#include "Utf.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <optional>
#include <utility>

namespace
{
    constexpr std::string_view ChatFilterLog = "server.loading";
    constexpr std::array<std::string_view, 5> Entries{
        "ChatFilter/WhiteListBase.txt",
        "ChatFilter/WhiteListPhrasesBase.txt",
        "ChatFilter/BlackListBase.txt",
        "ChatFilter/ExceptionListBase.txt",
        "ChatFilter/CharacterReplacementMap.txt"
    };

    bool IsSpace(char16_t character) noexcept
    {
        return character == u' ' || character == u'\t' || character == u'\r' || character == u'\n';
    }

    std::u16string_view Trim(std::u16string_view text) noexcept
    {
        while (!text.empty() && IsSpace(text.front()))
            text.remove_prefix(1);
        while (!text.empty() && IsSpace(text.back()))
            text.remove_suffix(1);
        return text;
    }

    char16_t Fold(char16_t character) noexcept
    {
        if (character >= u'A' && character <= u'Z')
            return static_cast<char16_t>(character + (u'a' - u'A'));
        return character;
    }

    std::u16string Fold(std::u16string_view text)
    {
        std::u16string result;
        result.reserve(text.size());
        for (char16_t character : text)
            result.push_back(Fold(character));
        return result;
    }

    bool IsWordCharacter(char16_t character) noexcept
    {
        return (character >= u'a' && character <= u'z') || (character >= u'A' && character <= u'Z') ||
            (character >= u'0' && character <= u'9') || character >= 0x80;
    }

    std::vector<std::u16string> Lines(std::u16string_view contents)
    {
        std::vector<std::u16string> lines;
        std::size_t start = 0;
        while (start < contents.size())
        {
            std::size_t const end = contents.find(u'\n', start);
            std::u16string_view const line = Trim(contents.substr(start, end == std::u16string_view::npos ? contents.size() - start : end - start));
            if (!line.empty())
                lines.emplace_back(line);
            if (end == std::u16string_view::npos)
                break;
            start = end + 1;
        }
        return lines;
    }

    bool DecodeEntry(KiwadArchive const& root, std::string_view name, std::u16string& contents, std::vector<std::string>& errors)
    {
        KiwadReadResult const read = root.Read(name);
        if (!read.Succeeded())
        {
            errors.push_back(fmt::format("{} cannot be read from {}: {}", name, ConfigMgr::PathToUtf8(root.GetPath()), read.Error));
            return false;
        }
        std::optional<std::u16string> decoded = Utf::Utf16LEBytesToString(read.Data, Utf::InvalidPolicy::Reject);
        if (!decoded)
        {
            errors.push_back(fmt::format("{} in {} is not valid UTF-16LE", name, ConfigMgr::PathToUtf8(root.GetPath())));
            return false;
        }
        if (!decoded->empty() && decoded->front() == u'\uFEFF')
            decoded->erase(decoded->begin());
        if (std::find(decoded->begin(), decoded->end(), u'\0') != decoded->end())
        {
            errors.push_back(fmt::format("{} in {} contains a null character", name, ConfigMgr::PathToUtf8(root.GetPath())));
            return false;
        }
        contents = std::move(*decoded);
        return true;
    }

    bool MatchesAt(std::u16string_view text, std::u16string_view pattern, std::unordered_map<char16_t, std::u16string> const& replacements, std::size_t start, bool word)
    {
        if (pattern.empty() || start + pattern.size() > text.size())
            return false;

        if (word && start != 0 && IsWordCharacter(text[start - 1]))
            return false;
        for (std::size_t offset = 0; offset < pattern.size(); ++offset)
        {
            char16_t const actual = Fold(text[start + offset]);
            char16_t const expected = Fold(pattern[offset]);
            if (actual == expected)
                continue;
            auto const found = replacements.find(actual);
            if (found == replacements.end() || std::none_of(found->second.begin(), found->second.end(), [expected](char16_t candidate)
            {
                return Fold(candidate) == expected;
            }))
                return false;
        }
        return !word || start + pattern.size() == text.size() || !IsWordCharacter(text[start + pattern.size()]);
    }

    bool Contains(std::u16string_view text, std::u16string_view pattern, std::unordered_map<char16_t, std::u16string> const& replacements, bool word)
    {
        if (pattern.empty() || text.size() < pattern.size())
            return false;
        for (std::size_t start = 0; start + pattern.size() <= text.size(); ++start)
            if (MatchesAt(text, pattern, replacements, start, word))
                return true;
        return false;
    }

    std::vector<std::u16string> AddedEntries(std::unordered_set<std::u16string> const& current, std::unordered_set<std::u16string> const& previous)
    {
        std::vector<std::u16string> added;
        for (std::u16string const& entry : current)
            if (!previous.contains(entry))
                added.push_back(entry);
        std::sort(added.begin(), added.end());
        return added;
    }
}

std::shared_ptr<ChatFilterLists const> ChatFilterLists::Read(KiwadArchive const& root, std::vector<std::string>& errors)
{
    std::array<std::u16string, Entries.size()> contents;
    std::size_t const initialErrors = errors.size();
    for (std::size_t index = 0; index < Entries.size(); ++index)
        DecodeEntry(root, Entries[index], contents[index], errors);
    if (errors.size() != initialErrors)
        return nullptr;

    auto lists = std::make_shared<ChatFilterLists>();
    for (std::u16string const& line : Lines(contents[0]))
        lists->_whitelist.insert(Fold(line));
    for (std::u16string const& line : Lines(contents[1]))
        lists->_whitelistPhrases.push_back(Fold(line));
    for (std::u16string const& line : Lines(contents[2]))
        lists->_blacklist.insert(Fold(line));
    for (std::u16string const& line : Lines(contents[3]))
        lists->_exceptions.insert(Fold(line));

    std::size_t lineNumber = 0;
    for (std::u16string const& line : Lines(contents[4]))
    {
        ++lineNumber;
        if (line.front() == u'#')
            continue;
        std::size_t const separator = line.find(u'=');
        if (separator == std::u16string::npos)
        {
            errors.push_back(fmt::format("{} line {} has no '=' separator", Entries[4], lineNumber));
            continue;
        }
        std::u16string_view const source = Trim(std::u16string_view(line).substr(0, separator));
        std::u16string_view const replacement = Trim(std::u16string_view(line).substr(separator + 1));
        if (source.empty() || replacement.empty())
        {
            errors.push_back(fmt::format("{} line {} has an empty source or replacement character list", Entries[4], lineNumber));
            continue;
        }
        for (char16_t character : source)
        {
            std::u16string& choices = lists->_replacements[Fold(character)];
            for (char16_t candidate : replacement)
                if (!IsSpace(candidate) && choices.find(Fold(candidate)) == std::u16string::npos)
                    choices.push_back(Fold(candidate));
            if (choices.empty())
                errors.push_back(fmt::format("{} line {} has no replacement characters", Entries[4], lineNumber));
        }
    }

    if (errors.size() != initialErrors)
        return nullptr;
    return lists;
}

ChatFilterResult ChatFilterLists::Inspect(std::u16string_view message) const
{
    for (std::u16string const& phrase : _whitelistPhrases)
        if (Contains(message, phrase, _replacements, false))
            return ChatFilterResult::Whitelisted;

    for (std::u16string const& word : _blacklist)
        for (std::size_t start = 0; start + word.size() <= message.size(); ++start)
        {
            if (!MatchesAt(message, word, _replacements, start, true))
                continue;

            std::size_t const end = start + word.size();
            bool excepted = false;
            for (std::u16string const& exception : _exceptions)
                for (std::size_t exceptionStart = 0; exceptionStart <= start; ++exceptionStart)
                    if (exceptionStart + exception.size() >= end && MatchesAt(message, exception, _replacements, exceptionStart, true))
                    {
                        excepted = true;
                        break;
                    }

            bool whitelisted = false;
            for (std::u16string const& allowed : _whitelist)
                if (allowed.size() == word.size() && MatchesAt(message, allowed, _replacements, start, true))
                {
                    whitelisted = true;
                    break;
                }

            if (!excepted && !whitelisted)
                return ChatFilterResult::Blacklisted;
        }
    return ChatFilterResult::Clear;
}

std::vector<std::u16string> ChatFilterLists::AddedBlacklistEntries(ChatFilterLists const& previous) const
{
    return AddedEntries(_blacklist, previous._blacklist);
}

std::vector<std::u16string> ChatFilterLists::AddedWhitelistEntries(ChatFilterLists const& previous) const
{
    return AddedEntries(_whitelist, previous._whitelist);
}

ChatFilterMgr& ChatFilterMgr::Instance()
{
    static ChatFilterMgr instance;
    return instance;
}

void ChatFilterMgr::SetInstall(std::filesystem::path root)
{
    std::lock_guard const lock(_installMutex);
    _install = std::move(root);
}

void ChatFilterMgr::RegisterReloadTarget(ChatFilterAdditionNotifier notifyAdditions)
{
    sReloadMgr.Register(std::string(Target), [this, notifyAdditions = std::move(notifyAdditions)](std::vector<std::string>& errors)
    {
        std::shared_ptr<ChatFilterLists const> const previous = _lists.Get();
        if (!Load(errors))
            return false;
        if (notifyAdditions)
        {
            std::shared_ptr<ChatFilterLists const> const current = _lists.Get();
            notifyAdditions(current->AddedBlacklistEntries(*previous), current->AddedWhitelistEntries(*previous));
        }
        return true;
    });
}

bool ChatFilterMgr::Load(std::vector<std::string>& errors)
{
    std::filesystem::path install;
    {
        std::lock_guard const lock(_installMutex);
        install = _install;
    }
    if (install.empty())
    {
        errors.emplace_back("no Wizard101 install is in use, so no chat-filter list can be read");
        return false;
    }

    auto const started = std::chrono::steady_clock::now();
    std::filesystem::path const rootWad = install / "Data" / "GameData" / "Root.wad";
    std::string error;
    std::unique_ptr<KiwadArchive> const root = KiwadArchive::Open(rootWad, error);
    if (!root)
    {
        errors.push_back(fmt::format("{} cannot be opened: {}", ConfigMgr::PathToUtf8(rootWad), error));
        return false;
    }
    std::shared_ptr<ChatFilterLists const> lists = ChatFilterLists::Read(*root, errors);
    if (!lists)
        return false;

    std::size_t const blacklist = lists->BlacklistSize();
    std::size_t const whitelist = lists->WhitelistSize();
    std::size_t const phrases = lists->WhitelistPhraseCount();
    _lists.Replace(std::move(lists));
    auto const took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    LOG_INFO(ChatFilterLog, "Read {} blacklisted words, {} whitelisted words and {} whitelisted phrases from Root.wad in {} ms", blacklist, whitelist, phrases, took.count());
    return true;
}

void ChatFilterMgr::Clear()
{
    _lists.Replace(ChatFilterLists{});
}
