/*
 * Project Ambrose by Imjustchico
 * Extracts what a wizard's level and school decide from the user's own Root.wad: MagicXPConfig.xml's shared level table and each school's own table merged into one row per school and level, with its settings, encounter experience factors and mob rank levels, the MagicSchools templates with their secondary-school badges, and WizStatisticEffectConfig.xml's settings with its crit, block and pip conversion bands, checked as the server's own level and stat sets check them and reporting every problem up to a cap, either from an open archive and catalog or straight from an install folder and type dump.
 */

#ifndef AMBROSE_LEVELEXTRACTOR_H
#define AMBROSE_LEVELEXTRACTOR_H

#include "PlayerLevels.h"
#include "StatEffects.h"
#include "TypeRegistry.h"
#include "Types.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class KiwadArchive;

struct LevelExtraction
{
    static constexpr std::size_t MaxReportedErrors = 100;

    PlayerLevelData Levels;
    StatEffectData Stats;
    std::vector<std::string> SchoolsWithTables;
    std::vector<std::string> SchoolsWithoutTables;
    std::vector<std::string> Errors;
    std::size_t ErrorCount = 0;

    bool Ok() const noexcept { return ErrorCount == 0; }
    void AddError(std::string error);
    void FinishErrors();
};

class LevelExtractor
{
public:
    static constexpr std::string_view MagicXPEntry = "MagicXPConfig.xml";
    static constexpr std::string_view SchoolFolder = "MagicSchools/";
    static constexpr std::string_view StatConfigEntry = "WizStatisticEffectConfig.xml";
    static constexpr std::size_t MaxEntryBytes = 16 * 1024 * 1024;

    static LevelExtraction Extract(KiwadArchive const& archive, TypeCatalogPtr const& catalog);
    static std::optional<LevelExtraction> ExtractFromInstall(std::filesystem::path const& clientDir, std::filesystem::path const& typeDump, std::string& error);
    static void ReadMagicXP(TypeCatalogPtr const& catalog, std::span<uint8 const> bind, LevelExtraction& extraction);
    static void ReadSchool(TypeCatalogPtr const& catalog, std::string_view entry, std::span<uint8 const> bind, LevelExtraction& extraction);
    static void ReadStatConfig(TypeCatalogPtr const& catalog, std::span<uint8 const> bind, LevelExtraction& extraction);
    static void Validate(LevelExtraction& extraction);
};

#endif
