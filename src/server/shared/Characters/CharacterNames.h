/*
 * Project Ambrose by Imjustchico
 * Wizard names: the first, middle and last indices a wizard stores packed in one number, a client name table per locale with each position's locale key and text, the disallowed name combinations, and CharacterNameSet, an immutable snapshot that validates all of them once and then checks a wizard's indices against its locale's human tables and the disallowed list and formats the name.
 */

#ifndef AMBROSE_CHARACTERNAMES_H
#define AMBROSE_CHARACTERNAMES_H

#include "Types.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct NameIndices
{
    static constexpr uint32 UnusedBits = 0xFF000000u;

    uint8 First = 0;
    uint8 Middle = 0;
    uint8 Last = 0;

    static constexpr NameIndices Unpack(uint32 packed) noexcept
    {
        return { static_cast<uint8>(packed >> 16), static_cast<uint8>(packed >> 8), static_cast<uint8>(packed) };
    }

    constexpr uint32 Pack() const noexcept
    {
        return (uint32{ First } << 16) | (uint32{ Middle } << 8) | uint32{ Last };
    }

    bool operator==(NameIndices const&) const = default;
};

enum class NameGender : uint32
{
    Female = 0,
    Male = 1
};

struct CharacterNamePart
{
    std::string LocaleKey;
    std::string Text;

    bool operator==(CharacterNamePart const&) const = default;
};

struct CharacterNameTable
{
    std::string Name;
    std::string Locale;
    std::vector<CharacterNamePart> Parts;

    bool operator==(CharacterNameTable const&) const = default;
};

struct DisallowedName
{
    static constexpr uint32 AnyIndex = 256;

    uint32 Id = 0;
    uint32 LocaleId = 0;
    uint32 Gender = 0;
    uint32 First = 0;
    uint32 Middle = 0;
    uint32 Last = 0;

    bool Matches(NameIndices indices, uint32 gender, std::optional<uint32> localeId) const noexcept;
    bool operator==(DisallowedName const&) const = default;
};

enum class NameCheck : uint8
{
    Ok,
    UnknownLocale,
    UnknownGender,
    UnusedBitsSet,
    FirstOutOfRange,
    MiddleOutOfRange,
    LastOutOfRange,
    Disallowed
};

class CharacterNameSet
{
public:
    static constexpr std::string_view FirstNameMale = "FirstName_HumanMale";
    static constexpr std::string_view FirstNameFemale = "FirstName_HumanFemale";
    static constexpr std::string_view MiddleName = "MiddleName_Human";
    static constexpr std::string_view LastName = "LastName_Human";
    static constexpr std::size_t MaxParts = 256;
    static constexpr std::size_t MaxTableNameBytes = 64;
    static constexpr std::size_t MaxLocaleBytes = 16;
    static constexpr std::size_t MaxLocaleKeyBytes = 128;
    static constexpr std::size_t MaxTextBytes = 255;

    CharacterNameSet(CharacterNameSet const&) = delete;
    CharacterNameSet& operator=(CharacterNameSet const&) = delete;

    static std::shared_ptr<CharacterNameSet const> Build(std::vector<CharacterNameTable> tables, std::vector<DisallowedName> disallowed, std::vector<std::string>& errors);
    static std::shared_ptr<CharacterNameSet const> Empty();

    std::vector<CharacterNameTable> const& GetTables() const noexcept { return _tables; }
    std::vector<DisallowedName> const& GetDisallowed() const noexcept { return _disallowed; }
    std::vector<std::string> GetHumanLocales() const;
    std::size_t GetPartCount() const noexcept { return _partCount; }
    CharacterNameTable const* FindTable(std::string_view name, std::string_view locale) const noexcept;
    bool HasLocale(std::string_view locale) const noexcept;

    NameCheck Check(uint32 packed, uint32 gender, std::string_view locale, std::optional<uint32> localeId = std::nullopt) const noexcept;
    bool IsValidIndices(uint32 packed, uint32 gender, std::string_view locale) const noexcept;
    bool IsDisallowed(uint32 packed, uint32 gender, std::optional<uint32> localeId = std::nullopt) const noexcept;
    std::optional<std::string> FormatName(uint32 packed, uint32 gender, std::string_view locale) const;

    static std::string_view GetCheckName(NameCheck check) noexcept;

private:
    struct HumanTables
    {
        CharacterNameTable const* FirstFemale = nullptr;
        CharacterNameTable const* FirstMale = nullptr;
        CharacterNameTable const* Middle = nullptr;
        CharacterNameTable const* Last = nullptr;
    };

    CharacterNameSet() = default;

    HumanTables const* FindHuman(std::string_view locale) const noexcept;
    NameCheck CheckRanges(uint32 packed, uint32 gender, HumanTables const& human) const noexcept;

    std::vector<CharacterNameTable> _tables;
    std::vector<DisallowedName> _disallowed;
    std::map<std::string, HumanTables, std::less<>> _human;
    std::size_t _partCount = 0;
};

#endif
