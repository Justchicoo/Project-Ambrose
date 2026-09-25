/*
 * Project Ambrose by Imjustchico
 * Validates name tables and disallowed names into a snapshot, reporting every problem by table, locale and position up to a cap: names and locales of plain characters, one table per name and locale, 1-256 parts of UTF-8 text without control characters within the column sizes, no text without a key, every human locale holding all four tables with keyed first names and a keyless first slot meaning no middle or last name, and known genders with unique ids; then checks indices by range and the disallowed list, where a rule index of 256 or more matches any index, and formats first, space, middle and last.
 */

#include "CharacterNames.h"
#include "Utf.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <set>
#include <tuple>
#include <utility>

namespace
{
    constexpr std::size_t MaxReportedErrors = 100;

    class ErrorList
    {
    public:
        explicit ErrorList(std::vector<std::string>& errors) : _errors(errors)
        {
        }

        template<typename... Args>
        void Add(fmt::format_string<Args...> format, Args&&... args)
        {
            ++_count;
            if (_count <= MaxReportedErrors)
                _errors.push_back(fmt::format(format, std::forward<Args>(args)...));
        }

        bool Any() const noexcept { return _count != 0; }

        void Finish()
        {
            if (_count > MaxReportedErrors)
                _errors.push_back(fmt::format("and {} more problems", _count - MaxReportedErrors));
        }

    private:
        std::vector<std::string>& _errors;
        std::size_t _count = 0;
    };

    bool IsPlainName(std::string_view text, bool allowDash) noexcept
    {
        return !text.empty() && std::all_of(text.begin(), text.end(), [allowDash](char c)
        {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || (allowDash && c == '-');
        });
    }

    std::string_view TextProblem(std::string_view text, std::size_t maxBytes) noexcept
    {
        if (text.size() > maxBytes)
            return "is too long";
        if (!Utf::IsValidUtf8(text))
            return "is not UTF-8";
        if (std::any_of(text.begin(), text.end(), [](char c) { return static_cast<uint8>(c) < 0x20 || c == 0x7F; }))
            return "holds a control character";
        return {};
    }

    bool IndexMatches(uint32 rule, uint8 index) noexcept
    {
        return rule >= DisallowedName::AnyIndex || rule == index;
    }
}

bool DisallowedName::Matches(NameIndices indices, uint32 gender, std::optional<uint32> localeId) const noexcept
{
    return Gender == gender && (!localeId || *localeId == LocaleId) && IndexMatches(First, indices.First) && IndexMatches(Middle, indices.Middle) && IndexMatches(Last, indices.Last);
}

std::shared_ptr<CharacterNameSet const> CharacterNameSet::Build(std::vector<CharacterNameTable> tables, std::vector<DisallowedName> disallowed, std::vector<std::string>& errors)
{
    ErrorList problems(errors);
    std::sort(tables.begin(), tables.end(), [](CharacterNameTable const& left, CharacterNameTable const& right) { return std::tie(left.Name, left.Locale) < std::tie(right.Name, right.Locale); });

    std::shared_ptr<CharacterNameSet> set(new CharacterNameSet());
    std::set<std::pair<std::string_view, std::string_view>> seen;
    for (CharacterNameTable const& table : tables)
    {
        std::string const label = fmt::format("{} ({})", table.Name, table.Locale);
        if (table.Name.size() > MaxTableNameBytes || !IsPlainName(table.Name, false))
            problems.Add("the table name '{}' must be 1-{} letters, digits or underscores", table.Name, MaxTableNameBytes);
        if (table.Locale.size() > MaxLocaleBytes || !IsPlainName(table.Locale, true))
            problems.Add("{}: the locale must be 1-{} letters, digits, underscores or dashes", label, MaxLocaleBytes);
        if (!seen.emplace(table.Name, table.Locale).second)
            problems.Add("{} is listed more than once", label);
        if (table.Parts.empty() || table.Parts.size() > MaxParts)
            problems.Add("{} holds {} parts; a table holds 1-{}", label, table.Parts.size(), MaxParts);
        for (std::size_t index = 0; index < table.Parts.size(); ++index)
        {
            CharacterNamePart const& part = table.Parts[index];
            if (std::string_view const problem = TextProblem(part.LocaleKey, MaxLocaleKeyBytes); !problem.empty())
                problems.Add("{} position {}: the locale key {}", label, index, problem);
            if (std::string_view const problem = TextProblem(part.Text, MaxTextBytes); !problem.empty())
                problems.Add("{} position {}: the text {}", label, index, problem);
            if (part.LocaleKey.empty() && !part.Text.empty())
                problems.Add("{} position {}: has text but no locale key", label, index);
        }
        set->_partCount += table.Parts.size();
    }

    set->_tables = std::move(tables);
    for (CharacterNameTable const& table : set->_tables)
    {
        bool const firstMale = table.Name == FirstNameMale;
        bool const firstFemale = table.Name == FirstNameFemale;
        bool const middle = table.Name == MiddleName;
        bool const last = table.Name == LastName;
        if (!firstMale && !firstFemale && !middle && !last)
            continue;
        HumanTables& human = set->_human[table.Locale];
        (firstMale ? human.FirstMale : firstFemale ? human.FirstFemale : middle ? human.Middle : human.Last) = &table;
        for (std::size_t index = 0; index < table.Parts.size(); ++index)
        {
            CharacterNamePart const& part = table.Parts[index];
            bool const noneSlot = (middle || last) && index == 0;
            if (noneSlot && (!part.LocaleKey.empty() || !part.Text.empty()))
                problems.Add("{} ({}) position 0 must be empty, since index 0 means no {} name", table.Name, table.Locale, middle ? "middle" : "last");
            else if (!noneSlot && (part.LocaleKey.empty() || part.Text.empty()))
                problems.Add("{} ({}) position {}: a name needs a locale key and text", table.Name, table.Locale, index);
        }
    }
    for (auto const& [locale, human] : set->_human)
    {
        std::vector<std::string_view> missing;
        for (auto const& [table, name] : { std::pair{ human.FirstMale, FirstNameMale }, std::pair{ human.FirstFemale, FirstNameFemale }, std::pair{ human.Middle, MiddleName }, std::pair{ human.Last, LastName } })
            if (!table)
                missing.push_back(name);
        if (!missing.empty())
            problems.Add("the {} locale has some human name tables but not {}", locale, fmt::join(missing, ", "));
    }

    std::set<uint32> ids;
    for (DisallowedName const& rule : disallowed)
    {
        if (rule.Gender > static_cast<uint32>(NameGender::Male))
            problems.Add("disallowed name {} has gender {}; 0 is female and 1 is male", rule.Id, rule.Gender);
        if (!ids.insert(rule.Id).second)
            problems.Add("disallowed name {} is listed more than once", rule.Id);
    }
    std::sort(disallowed.begin(), disallowed.end(), [](DisallowedName const& left, DisallowedName const& right) { return left.Id < right.Id; });
    set->_disallowed = std::move(disallowed);

    problems.Finish();
    if (problems.Any())
        return nullptr;
    return set;
}

std::shared_ptr<CharacterNameSet const> CharacterNameSet::Empty()
{
    static std::shared_ptr<CharacterNameSet const> const empty(new CharacterNameSet());
    return empty;
}

std::vector<std::string> CharacterNameSet::GetHumanLocales() const
{
    std::vector<std::string> locales;
    locales.reserve(_human.size());
    for (auto const& [locale, human] : _human)
        locales.push_back(locale);
    return locales;
}

CharacterNameTable const* CharacterNameSet::FindTable(std::string_view name, std::string_view locale) const noexcept
{
    auto const found = std::lower_bound(_tables.begin(), _tables.end(), std::pair{ name, locale }, [](CharacterNameTable const& table, std::pair<std::string_view, std::string_view> const& key)
    {
        return std::pair<std::string_view, std::string_view>{ table.Name, table.Locale } < key;
    });
    return found != _tables.end() && found->Name == name && found->Locale == locale ? &*found : nullptr;
}

bool CharacterNameSet::HasLocale(std::string_view locale) const noexcept
{
    return FindHuman(locale) != nullptr;
}

CharacterNameSet::HumanTables const* CharacterNameSet::FindHuman(std::string_view locale) const noexcept
{
    auto const found = _human.find(locale);
    return found == _human.end() ? nullptr : &found->second;
}

NameCheck CharacterNameSet::CheckRanges(uint32 packed, uint32 gender, HumanTables const& human) const noexcept
{
    if (gender > static_cast<uint32>(NameGender::Male))
        return NameCheck::UnknownGender;
    NameIndices const indices = NameIndices::Unpack(packed);
    CharacterNameTable const& first = *(gender == static_cast<uint32>(NameGender::Male) ? human.FirstMale : human.FirstFemale);
    if (indices.First >= first.Parts.size())
        return NameCheck::FirstOutOfRange;
    if (indices.Middle >= human.Middle->Parts.size())
        return NameCheck::MiddleOutOfRange;
    if (indices.Last >= human.Last->Parts.size())
        return NameCheck::LastOutOfRange;
    return NameCheck::Ok;
}

NameCheck CharacterNameSet::Check(uint32 packed, uint32 gender, std::string_view locale, std::optional<uint32> localeId) const noexcept
{
    HumanTables const* const human = FindHuman(locale);
    if (!human)
        return NameCheck::UnknownLocale;
    if (NameCheck const ranges = CheckRanges(packed, gender, *human); ranges != NameCheck::Ok)
        return ranges;
    uint8 const marked = NameIndices::Unpack(packed).Locale;
    std::optional<uint32> const against = localeId ? localeId : marked == 0 ? std::nullopt : std::optional<uint32>(marked);
    return IsDisallowed(packed, gender, against) ? NameCheck::Disallowed : NameCheck::Ok;
}

bool CharacterNameSet::IsValidIndices(uint32 packed, uint32 gender, std::string_view locale) const noexcept
{
    HumanTables const* const human = FindHuman(locale);
    return human && CheckRanges(packed, gender, *human) == NameCheck::Ok;
}

bool CharacterNameSet::IsDisallowed(uint32 packed, uint32 gender, std::optional<uint32> localeId) const noexcept
{
    NameIndices const indices = NameIndices::Unpack(packed);
    return std::any_of(_disallowed.begin(), _disallowed.end(), [&](DisallowedName const& rule) { return rule.Matches(indices, gender, localeId); });
}

std::optional<std::string> CharacterNameSet::FormatName(uint32 packed, uint32 gender, std::string_view locale) const
{
    HumanTables const* const human = FindHuman(locale);
    if (!human || CheckRanges(packed, gender, *human) != NameCheck::Ok)
        return std::nullopt;
    NameIndices const indices = NameIndices::Unpack(packed);
    CharacterNameTable const& first = *(gender == static_cast<uint32>(NameGender::Male) ? human->FirstMale : human->FirstFemale);
    std::string name = first.Parts[indices.First].Text;
    if (indices.Middle != 0 || indices.Last != 0)
    {
        name.push_back(' ');
        name.append(human->Middle->Parts[indices.Middle].Text);
        name.append(human->Last->Parts[indices.Last].Text);
    }
    return name;
}

std::string_view CharacterNameSet::GetCheckName(NameCheck check) noexcept
{
    switch (check)
    {
        case NameCheck::Ok: return "valid";
        case NameCheck::UnknownLocale: return "no human name tables for the locale";
        case NameCheck::UnknownGender: return "unknown gender";
        case NameCheck::FirstOutOfRange: return "first name index out of range";
        case NameCheck::MiddleOutOfRange: return "middle name index out of range";
        case NameCheck::LastOutOfRange: return "last name index out of range";
        case NameCheck::Disallowed: return "disallowed name";
    }
    return "unknown";
}
