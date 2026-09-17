/*
 * Project Ambrose by Imjustchico
 * Extracts character creation data from the user's own Root.wad: every name table of CharacterNames.xml in each locale its section's .lang file covers, with each position's locale key and text, the disallowed names of the CharacterNamesDisallowedList.xml BINd file, and the schools and creation options of CharacterCreation/CharacterCreationConfig.xml, reporting every problem up to a cap and validating the names as the server will, either from an open archive and catalog or straight from an install folder and type dump.
 */

#ifndef AMBROSE_CHARACTERNAMEEXTRACTOR_H
#define AMBROSE_CHARACTERNAMEEXTRACTOR_H

#include "CharacterNames.h"
#include "TypeRegistry.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class KiwadArchive;

struct CreationSchool
{
    uint32 Order = 0;
    std::string Name;
    uint32 Id = 0;

    bool operator==(CreationSchool const&) const = default;
};

struct CreationOption
{
    uint32 Order = 0;
    uint32 TemplateId = 0;

    bool operator==(CreationOption const&) const = default;
};

struct NameExtraction
{
    static constexpr std::size_t MaxReportedErrors = 100;

    std::vector<CharacterNameTable> Tables;
    std::vector<DisallowedName> Disallowed;
    std::vector<CreationSchool> Schools;
    std::vector<CreationOption> Options;
    std::vector<std::string> Errors;
    std::size_t ErrorCount = 0;

    bool Ok() const noexcept { return ErrorCount == 0; }
    void AddError(std::string error);
    void FinishErrors();
    std::size_t GetPartCount() const noexcept;
};

class CharacterNameExtractor
{
public:
    static constexpr std::string_view NamesEntry = "CharacterNames.xml";
    static constexpr std::string_view DisallowedEntry = "CharacterNamesDisallowedList.xml";
    static constexpr std::string_view CreationConfigEntry = "CharacterCreation/CharacterCreationConfig.xml";
    static constexpr std::size_t MaxEntryBytes = 16 * 1024 * 1024;
    static constexpr std::size_t MaxSchoolNameBytes = 32;
    static constexpr uint32 MaxOrder = 65535;

    CharacterNameExtractor() = delete;

    static NameExtraction Extract(KiwadArchive const& archive, TypeCatalogPtr const& catalog);
    static std::optional<NameExtraction> ExtractFromInstall(std::filesystem::path const& clientDir, std::filesystem::path const& typeDump, std::string& error);
    static void ReadNameTables(KiwadArchive const& archive, std::span<uint8 const> xml, NameExtraction& extraction);
    static void ReadDisallowed(TypeCatalogPtr const& catalog, std::span<uint8 const> bind, NameExtraction& extraction);
    static void ReadCreationConfig(std::span<uint8 const> xml, NameExtraction& extraction);
    static void Validate(NameExtraction& extraction);
};

#endif
