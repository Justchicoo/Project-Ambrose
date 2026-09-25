/*
 * Project Ambrose by Imjustchico
 * Lays extracted rows out for the world tables: level rows in extraction order, schools with their badges by position, settings by name, encounter factors by position, mob ranks in list order, and band values by level and position, with every float carried as the double it widens to so it reads back unchanged.
 */

#include "LevelScript.h"

WorldSqlScript LevelScript::Build(LevelExtraction const& extraction)
{
    PlayerLevelData const& levels = extraction.Levels;
    StatEffectData const& stats = extraction.Stats;
    auto const number = [](float value) { return WorldSqlScript::Value{ static_cast<double>(value) }; };
    auto const whole = [](int32 value) { return WorldSqlScript::Value{ int64{ value } }; };

    std::vector<WorldSqlScript::Row> levelRows;
    levelRows.reserve(levels.Levels.size());
    for (PlayerLevelInfo const& row : levels.Levels)
        levelRows.push_back({ uint64{ row.SchoolId }, uint64{ row.Level }, whole(row.XpToLevel), whole(row.Hitpoints), whole(row.Mana), whole(row.Gold), number(row.PipChance),
            whole(row.TrainingPoints), whole(row.CraftingSlots), whole(row.PetEnergy), whole(row.PipConversionAll), whole(row.PipConversion[0]), whole(row.PipConversion[1]),
            whole(row.PipConversion[2]), whole(row.PipConversion[3]), whole(row.PipConversion[4]), whole(row.PipConversion[5]), whole(row.PipConversion[6]), number(row.ShadowPipRating),
            number(row.Archmastery), row.LevelName });
    std::vector<WorldSqlScript::Row> schools;
    std::vector<WorldSqlScript::Row> badges;
    for (MagicSchool const& school : levels.Schools)
    {
        schools.push_back({ uint64{ school.Id }, school.Name, uint64{ school.MinLevel }, whole(school.Index) });
        for (std::size_t position = 0; position < school.Badges.size(); ++position)
            badges.push_back({ uint64{ school.Id }, uint64{ position }, school.Badges[position] });
    }
    std::vector<WorldSqlScript::Row> xpSettings;
    for (ConfigValue const& setting : levels.XpConfig)
        xpSettings.push_back({ setting.Name, setting.Value });
    std::vector<WorldSqlScript::Row> factors;
    for (std::size_t position = 0; position < levels.EncounterXpFactors.size(); ++position)
        factors.push_back({ uint64{ position }, number(levels.EncounterXpFactors[position]) });
    std::vector<WorldSqlScript::Row> ranks;
    for (MobRankLevel const& rank : levels.MobRanks)
        ranks.push_back({ whole(rank.Rank), whole(rank.Level) });
    std::vector<WorldSqlScript::Row> statSettings;
    for (ConfigValue const& setting : stats.Settings)
        statSettings.push_back({ setting.Name, setting.Value });
    std::vector<WorldSqlScript::Row> critAndBlock;
    for (CritAndBlockValues const& values : stats.CritAndBlock)
        critAndBlock.push_back({ whole(values.MinLevel), uint64{ values.Position }, number(values.CapValue), number(values.CriticalHitScalarBase), number(values.CriticalHitScalingFactor),
            number(values.BlockScalarBase), number(values.BlockScalingFactor) });
    std::vector<WorldSqlScript::Row> pipConversion;
    for (PipConversionValues const& values : stats.PipConversion)
        pipConversion.push_back({ whole(values.MinLevel), uint64{ values.Position }, number(values.CapValue), number(values.ScalarBase), number(values.ScalingFactor) });

    std::vector<std::string_view> const tables = GetTables();
    WorldSqlScript script;
    script.ReplaceTable(tables[0], { "school_id", "level", "xp_to_level", "hitpoints", "mana", "gold", "pip_chance", "training_points", "crafting_slots", "pet_energy", "pip_conversion_all",
        "pip_conversion_fire", "pip_conversion_ice", "pip_conversion_storm", "pip_conversion_life", "pip_conversion_myth", "pip_conversion_death", "pip_conversion_balance",
        "shadow_pip_rating", "archmastery", "level_name" }, levelRows);
    script.ReplaceTable(tables[1], { "school_id", "school_name", "min_level", "school_index" }, schools);
    script.ReplaceTable(tables[2], { "school_id", "position", "badge_name" }, badges);
    script.ReplaceTable(tables[3], { "name", "value" }, xpSettings);
    script.ReplaceTable(tables[4], { "position", "factor" }, factors);
    script.ReplaceTable(tables[5], { "rank", "level" }, ranks);
    script.ReplaceTable(tables[6], { "name", "value" }, statSettings);
    script.ReplaceTable(tables[7], { "min_level", "position", "cap_value", "critical_hit_scalar_base", "critical_hit_scaling_factor", "block_scalar_base", "block_scaling_factor" }, critAndBlock);
    script.ReplaceTable(tables[8], { "min_level", "position", "cap_value", "scalar_base", "scaling_factor" }, pipConversion);
    return script;
}

std::vector<std::string_view> LevelScript::GetTables()
{
    return { "player_level_stats", "magic_school_template", "magic_school_badge", "magic_xp_config", "magic_xp_encounter_factor", "mob_rank_level", "stat_effect_config",
        "stat_crit_block_band", "stat_pip_conversion_band" };
}
