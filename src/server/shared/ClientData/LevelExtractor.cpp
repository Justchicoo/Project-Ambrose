/*
 * Project Ambrose by Imjustchico
 * Reads the level, school and stat files through their typed views. A school's row for a level takes each value from that school's own table where the table sets it, a value other than zero, or for experience other than -1, and from the shared table otherwise: the client's own tables set only each wizard school's hitpoints and its own pip conversion rating, the client reads everything else from the shared table by level alone, and the other magic schools list no levels at all, so they are named but give no rows. Every level a school lists must be in the shared table, the shared table must run from level 0 to the level cap with each level once, a school template that carries behaviors is refused because nothing reads them, and a class's settings are every scalar the class declares, by the dump's own names.
 */

#include "LevelExtractor.h"
#include "BindFile.h"
#include "ConfigMgr.h"
#include "KiwadArchive.h"
#include "LevelViews.h"
#include "ObjectSerializer.h"
#include "StringHash.h"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <utility>

namespace
{
    PropertyObjectPtr DecodeRoot(TypeCatalogPtr const& catalog, std::string_view entry, std::span<uint8 const> bind, LevelExtraction& extraction)
    {
        BindReadResult read = BindFile::Read(catalog, bind);
        if (!read.Ok())
        {
            extraction.AddError(fmt::format("{}: {}: {}", entry, BindFile::GetStatusName(read.Status), read.Detail));
            return nullptr;
        }
        for (DecodeIssue const& issue : read.Decoded.Issues)
            extraction.AddError(fmt::format("{}: {} at {}: {}", entry, ObjectSerializer::GetIssueName(issue.Kind), issue.Path, issue.Detail));
        if (!read.Decoded.Issues.empty())
            return nullptr;
        return std::move(read.Decoded.Object);
    }

    std::string ClassOf(PropertyObject const* object)
    {
        return object ? object->GetClass().Name : std::string("no object");
    }

    std::optional<double> Scalar(PropertyValue const& value)
    {
        if (bool const* const flag = value.GetIf<bool>())
            return *flag ? 1.0 : 0.0;
        if (int32 const* const number = value.GetIf<int32>())
            return *number;
        if (uint32 const* const number = value.GetIf<uint32>())
            return *number;
        if (int16 const* const number = value.GetIf<int16>())
            return *number;
        if (uint16 const* const number = value.GetIf<uint16>())
            return *number;
        if (int8 const* const number = value.GetIf<int8>())
            return *number;
        if (uint8 const* const number = value.GetIf<uint8>())
            return *number;
        if (float const* const number = value.GetIf<float>())
            return *number;
        if (double const* const number = value.GetIf<double>())
            return *number;
        return std::nullopt;
    }

    void ReadSettings(PropertyObject const& root, std::string_view entry, std::vector<ConfigValue>& settings, LevelExtraction& extraction)
    {
        for (PropertyInfo const& property : root.GetClass().Properties)
        {
            if (property.Container != ContainerKind::Static || property.Pointer)
                continue;
            PropertyValue const* const value = root.Get(property.Name);
            std::optional<double> const number = value ? Scalar(*value) : std::nullopt;
            if (!number)
                continue;
            if (!std::isfinite(*number))
                extraction.AddError(fmt::format("{}: {} is not a finite number", entry, property.Name));
            else
                settings.push_back({ property.Name, *number });
        }
    }

    PlayerLevelInfo LevelValues(MagicLevelInfoView const& level)
    {
        PlayerLevelInfo row;
        row.Level = static_cast<uint32>(std::max<int32>(level.GetLevel(), 0));
        row.XpToLevel = level.GetXpToLevel();
        row.Hitpoints = level.GetHitpoints();
        row.Mana = level.GetMana();
        row.Gold = level.GetGold();
        row.PipChance = level.GetPipChance();
        row.TrainingPoints = level.GetTrainingPoints();
        row.CraftingSlots = level.GetCraftingSlots();
        row.PetEnergy = level.GetPetEnergy();
        row.PipConversionAll = level.GetPipConversionAll();
        row.PipConversion = { level.GetPipConversionFire(), level.GetPipConversionIce(), level.GetPipConversionStorm(), level.GetPipConversionLife(), level.GetPipConversionMyth(),
            level.GetPipConversionDeath(), level.GetPipConversionBalance() };
        row.ShadowPipRating = level.GetShadowPipRating();
        row.Archmastery = level.GetArchmastery();
        row.LevelName = level.GetLevelName();
        return row;
    }

    int32 Pick(int32 own, int32 shared)
    {
        return own != 0 ? own : shared;
    }

    float Pick(float own, float shared)
    {
        return own != 0.0f ? own : shared;
    }

    PlayerLevelInfo Merge(PlayerLevelInfo const& own, PlayerLevelInfo const& shared)
    {
        PlayerLevelInfo row = own;
        row.XpToLevel = own.XpToLevel != 0 && own.XpToLevel != -1 ? own.XpToLevel : shared.XpToLevel;
        row.Hitpoints = Pick(own.Hitpoints, shared.Hitpoints);
        row.Mana = Pick(own.Mana, shared.Mana);
        row.Gold = Pick(own.Gold, shared.Gold);
        row.PipChance = Pick(own.PipChance, shared.PipChance);
        row.TrainingPoints = Pick(own.TrainingPoints, shared.TrainingPoints);
        row.CraftingSlots = Pick(own.CraftingSlots, shared.CraftingSlots);
        row.PetEnergy = Pick(own.PetEnergy, shared.PetEnergy);
        row.PipConversionAll = Pick(own.PipConversionAll, shared.PipConversionAll);
        for (std::size_t school = 0; school < row.PipConversion.size(); ++school)
            row.PipConversion[school] = Pick(own.PipConversion[school], shared.PipConversion[school]);
        row.ShadowPipRating = Pick(own.ShadowPipRating, shared.ShadowPipRating);
        row.Archmastery = Pick(own.Archmastery, shared.Archmastery);
        row.LevelName = own.LevelName.empty() ? shared.LevelName : own.LevelName;
        return row;
    }

    std::string Describe(std::optional<MagicLevelInfoView> const& level)
    {
        return level ? fmt::format("for level {}", level->GetLevel()) : std::string("not a MagicLevelInfo");
    }
}

void LevelExtraction::AddError(std::string error)
{
    ++ErrorCount;
    if (ErrorCount <= MaxReportedErrors)
        Errors.push_back(std::move(error));
}

void LevelExtraction::FinishErrors()
{
    if (ErrorCount > MaxReportedErrors && Errors.size() == MaxReportedErrors)
        Errors.push_back(fmt::format("and {} more problems", ErrorCount - MaxReportedErrors));
}

LevelExtraction LevelExtractor::Extract(KiwadArchive const& archive, TypeCatalogPtr const& catalog)
{
    LevelExtraction extraction;
    auto const read = [&archive, &extraction](std::string_view entry) -> std::optional<std::vector<uint8>>
    {
        KiwadReadResult result = archive.Read(entry, MaxEntryBytes);
        if (!result.Succeeded())
        {
            extraction.AddError(fmt::format("{}: {}", entry, result.Error));
            return std::nullopt;
        }
        return std::move(result.Data);
    };
    if (std::optional<std::vector<uint8>> const config = read(MagicXPEntry))
        ReadMagicXP(catalog, *config, extraction);
    std::vector<std::string> schools;
    for (KiwadEntry const& entry : archive.GetEntries())
        if (entry.Name.starts_with(SchoolFolder) && entry.Name.ends_with(".xml") && entry.Name.find('/', SchoolFolder.size()) == std::string::npos)
            schools.push_back(entry.Name);
    std::sort(schools.begin(), schools.end());
    if (schools.empty())
        extraction.AddError(fmt::format("the archive holds no {}*.xml school templates", SchoolFolder));
    for (std::string const& school : schools)
        if (std::optional<std::vector<uint8>> const bind = read(school))
            ReadSchool(catalog, school, *bind, extraction);
    if (std::optional<std::vector<uint8>> const config = read(StatConfigEntry))
        ReadStatConfig(catalog, *config, extraction);
    if (extraction.Ok())
        Validate(extraction);
    extraction.FinishErrors();
    return extraction;
}

std::optional<LevelExtraction> LevelExtractor::ExtractFromInstall(std::filesystem::path const& clientDir, std::filesystem::path const& typeDump, std::string& error)
{
    std::filesystem::path const rootWad = clientDir / "Data" / "GameData" / "Root.wad";
    std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(rootWad, error);
    if (!archive)
    {
        error = fmt::format("cannot open {}: {}", ConfigMgr::PathToUtf8(rootWad), error);
        return std::nullopt;
    }
    TypedViewRegistry views;
    LevelViews::RegisterAll(views);
    TypeRegistry registry(&views);
    if (!registry.LoadFromFile(typeDump))
    {
        std::vector<std::string> const problems = registry.GetErrors();
        error = fmt::format("cannot load the type dump {}{}{}", ConfigMgr::PathToUtf8(typeDump), problems.empty() ? "" : ": ", problems.empty() ? std::string() : problems.front());
        return std::nullopt;
    }
    return Extract(*archive, registry.GetCatalog());
}

void LevelExtractor::ReadMagicXP(TypeCatalogPtr const& catalog, std::span<uint8 const> bind, LevelExtraction& extraction)
{
    PropertyObjectPtr const root = DecodeRoot(catalog, MagicXPEntry, bind, extraction);
    if (!root)
        return;
    std::optional<MagicXPConfigView> const config = MagicXPConfigView::From(root.get());
    if (!config)
    {
        extraction.AddError(fmt::format("{} holds {}, which the level view does not read; the type dump must list class MagicXPConfig", MagicXPEntry, ClassOf(root.get())));
        return;
    }
    if (config->GetMaxSchoolLevel() <= 0 || static_cast<uint32>(config->GetMaxSchoolLevel()) > PlayerLevelSet::MaxLevel)
    {
        extraction.AddError(fmt::format("{} gives the level cap as {}, where 1 to {} belongs", MagicXPEntry, config->GetMaxSchoolLevel(), PlayerLevelSet::MaxLevel));
        return;
    }
    uint32 const cap = static_cast<uint32>(config->GetMaxSchoolLevel());
    ReadSettings(*root, MagicXPEntry, extraction.Levels.XpConfig, extraction);
    for (PropertyValue const& factor : config->GetEncounterXPFactors())
    {
        if (float const* const number = factor.GetIf<float>())
            extraction.Levels.EncounterXpFactors.push_back(*number);
        else
            extraction.AddError(fmt::format("{}: encounter experience factor {} is not a float", MagicXPEntry, extraction.Levels.EncounterXpFactors.size()));
    }
    std::size_t position = 0;
    for (PropertyValue const& value : config->GetMobRanks())
    {
        if (std::optional<MobRankLevelView> const rank = MobRankLevelView::From(value.AsObject()))
            extraction.Levels.MobRanks.push_back({ rank->GetRank(), rank->GetLevel() });
        else
            extraction.AddError(fmt::format("{}: mob rank entry {} is not a MobRankLevel", MagicXPEntry, position));
        ++position;
    }

    std::map<uint32, PlayerLevelInfo> shared;
    position = 0;
    for (PropertyValue const& value : config->GetLevelTable())
    {
        std::optional<MagicLevelInfoView> const level = MagicLevelInfoView::From(value.AsObject());
        if (!level || level->GetLevel() < 0)
            extraction.AddError(fmt::format("{}: shared level entry {} is {}", MagicXPEntry, position, Describe(level)));
        else if (!shared.emplace(static_cast<uint32>(level->GetLevel()), LevelValues(*level)).second)
            extraction.AddError(fmt::format("{}: the shared table lists level {} twice", MagicXPEntry, level->GetLevel()));
        ++position;
    }
    for (uint32 level = 0; level <= cap; ++level)
        if (!shared.contains(level))
        {
            extraction.AddError(fmt::format("{}: the shared table has no level {}, and it must run from 0 to the cap of {}", MagicXPEntry, level, cap));
            break;
        }

    position = 0;
    for (PropertyValue const& value : config->GetSchoolTables())
    {
        std::optional<SchoolLevelTableView> const school = SchoolLevelTableView::From(value.AsObject());
        std::size_t const index = position++;
        if (!school || school->GetSchoolName().empty())
        {
            extraction.AddError(fmt::format("{}: school table {} is {}", MagicXPEntry, index, school ? std::string("unnamed") : std::string("not a ClassInfo")));
            continue;
        }
        std::string const& name = school->GetSchoolName();
        if (school->GetLevelTable().empty())
        {
            extraction.SchoolsWithoutTables.push_back(name);
            continue;
        }
        extraction.SchoolsWithTables.push_back(name);
        uint32 const schoolId = StringHash::KiStringHash(name);
        for (PropertyValue const& entry : school->GetLevelTable())
        {
            std::optional<MagicLevelInfoView> const level = MagicLevelInfoView::From(entry.AsObject());
            if (!level || level->GetLevel() < 0)
            {
                extraction.AddError(fmt::format("{}: school {} has an entry that is {}", MagicXPEntry, name, Describe(level)));
                continue;
            }
            auto const base = shared.find(static_cast<uint32>(level->GetLevel()));
            if (base == shared.end())
            {
                extraction.AddError(fmt::format("{}: school {} lists level {}, which the shared table does not", MagicXPEntry, name, level->GetLevel()));
                continue;
            }
            PlayerLevelInfo row = Merge(LevelValues(*level), base->second);
            row.SchoolId = schoolId;
            extraction.Levels.Levels.push_back(std::move(row));
        }
    }
}

void LevelExtractor::ReadSchool(TypeCatalogPtr const& catalog, std::string_view entry, std::span<uint8 const> bind, LevelExtraction& extraction)
{
    PropertyObjectPtr const root = DecodeRoot(catalog, entry, bind, extraction);
    if (!root)
        return;
    std::optional<MagicSchoolTemplateView> const school = MagicSchoolTemplateView::From(root.get());
    if (!school)
    {
        extraction.AddError(fmt::format("{} holds {}, which the school view does not read; the type dump must list class MagicSchoolTemplate", entry, ClassOf(root.get())));
        return;
    }
    if (school->GetSchoolName().empty())
    {
        extraction.AddError(fmt::format("{} names no school", entry));
        return;
    }
    if (!school->GetBehaviors().empty())
    {
        extraction.AddError(fmt::format("{} carries {} behavior(s), which nothing reads yet, so the school is not taken", entry, school->GetBehaviors().size()));
        return;
    }
    MagicSchool row;
    row.Id = StringHash::KiStringHash(school->GetSchoolName());
    row.Name = school->GetSchoolName();
    row.MinLevel = school->GetMinLevel();
    row.Index = school->GetSchoolIndex();
    for (PropertyValue const& badge : school->GetSecondarySchoolBadges())
    {
        std::string const* const text = badge.GetIf<std::string>();
        if (!text)
        {
            extraction.AddError(fmt::format("{}: secondary school badge {} is not a name", entry, row.Badges.size()));
            return;
        }
        row.Badges.push_back(*text);
    }
    extraction.Levels.Schools.push_back(std::move(row));
}

void LevelExtractor::ReadStatConfig(TypeCatalogPtr const& catalog, std::span<uint8 const> bind, LevelExtraction& extraction)
{
    PropertyObjectPtr const root = DecodeRoot(catalog, StatConfigEntry, bind, extraction);
    if (!root)
        return;
    std::optional<StatEffectConfigView> const config = StatEffectConfigView::From(root.get());
    if (!config)
    {
        extraction.AddError(fmt::format("{} holds {}, which the stat view does not read; the type dump must list class WizStatisticEffectConfig", StatConfigEntry, ClassOf(root.get())));
        return;
    }
    ReadSettings(*root, StatConfigEntry, extraction.Stats.Settings, extraction);
    for (PropertyValue const& value : config->GetCritAndBlockBands())
    {
        std::optional<CritAndBlockBandView> const band = CritAndBlockBandView::From(value.AsObject());
        if (!band)
        {
            extraction.AddError(fmt::format("{}: a crit and block band is not a CritAndBlockLevelData", StatConfigEntry));
            continue;
        }
        uint32 position = 0;
        for (PropertyValue const& entry : band->GetValues())
        {
            if (std::optional<CritAndBlockValuesView> const values = CritAndBlockValuesView::From(entry.AsObject()))
                extraction.Stats.CritAndBlock.push_back({ band->GetMinLevel(), position, values->GetCapValue(), values->GetCriticalHitScalarBase(), values->GetCriticalHitScalingFactor(),
                    values->GetBlockScalarBase(), values->GetBlockScalingFactor() });
            else
                extraction.AddError(fmt::format("{}: crit and block band {} entry {} is not a CritAndBlockValues", StatConfigEntry, band->GetMinLevel(), position));
            ++position;
        }
    }
    for (PropertyValue const& value : config->GetPipConversionBands())
    {
        std::optional<PipConversionBandView> const band = PipConversionBandView::From(value.AsObject());
        if (!band)
        {
            extraction.AddError(fmt::format("{}: a pip conversion band is not a PipConversionLevelData", StatConfigEntry));
            continue;
        }
        uint32 position = 0;
        for (PropertyValue const& entry : band->GetValues())
        {
            if (std::optional<PipConversionValuesView> const values = PipConversionValuesView::From(entry.AsObject()))
                extraction.Stats.PipConversion.push_back({ band->GetMinLevel(), position, values->GetCapValue(), values->GetScalarBase(), values->GetScalingFactor() });
            else
                extraction.AddError(fmt::format("{}: pip conversion band {} entry {} is not a PipConversionValues", StatConfigEntry, band->GetMinLevel(), position));
            ++position;
        }
    }
}

void LevelExtractor::Validate(LevelExtraction& extraction)
{
    std::set<std::string, std::less<>> templates;
    for (MagicSchool const& school : extraction.Levels.Schools)
        templates.insert(school.Name);
    bool named = true;
    for (std::string const& school : extraction.SchoolsWithTables)
        if (!templates.contains(school))
        {
            extraction.AddError(fmt::format("{} has a level table for {}, which no {}*.xml template names", MagicXPEntry, school, SchoolFolder));
            named = false;
        }
    std::vector<std::string> errors;
    if (named)
        PlayerLevelSet::Build(extraction.Levels, errors);
    if (extraction.Stats.Settings.empty())
        errors.push_back(fmt::format("{} gives no settings", StatConfigEntry));
    StatEffectSet::Build(extraction.Stats, errors);
    for (std::string& error : errors)
        extraction.AddError(std::move(error));
}
