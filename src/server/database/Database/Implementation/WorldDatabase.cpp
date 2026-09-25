/*
 * Project Ambrose by Imjustchico
 * Registers every world database statement with its name, SQL, and the connections that prepare it.
 */

#include "WorldDatabase.h"

void WorldDatabaseConnection::DoPrepareStatements()
{
    PrepareStatement(WORLD_SEL_SERVER_TIME, "WORLD_SEL_SERVER_TIME", "SELECT UNIX_TIMESTAMP()", ConnectionFlags::Both);
    PrepareStatement(WORLD_SEL_CHARACTER_NAME_PARTS, "WORLD_SEL_CHARACTER_NAME_PARTS", "SELECT `table_name`, `locale`, `idx`, `locale_key`, `text` FROM `character_name_part` ORDER BY `table_name`, `locale`, `idx`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_CHARACTER_NAME_DISALLOWED, "WORLD_SEL_CHARACTER_NAME_DISALLOWED", "SELECT `id`, `locale_id`, `gender`, `first_idx`, `middle_idx`, `last_idx` FROM `character_name_disallowed` ORDER BY `id`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_CHARACTER_CREATE_SCHOOLS, "WORLD_SEL_CHARACTER_CREATE_SCHOOLS", "SELECT `school_id`, `school_name`, `sort_order` FROM `character_create_school` ORDER BY `sort_order`, `school_id`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_PLAYER_CREATE_INFO, "WORLD_SEL_PLAYER_CREATE_INFO", "SELECT `school_id`, `world`, `zone`, `zone_display`, `position_x`, `position_y`, `position_z`, `orientation`, `level`, `experience` FROM `playercreateinfo` ORDER BY `school_id`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_ZONE_TEMPLATES, "WORLD_SEL_ZONE_TEMPLATES", "SELECT `zone_path`, `display_name_key`, `far_clip`, `healing_per_minute`, `soft_limit`, `hard_limit`, `no_mounts` FROM `zone_template` ORDER BY `zone_path`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_ZONE_LOCATIONS, "WORLD_SEL_ZONE_LOCATIONS", "SELECT `id`, `zone_path`, `name`, `location`, `direction` FROM `zone_location` ORDER BY `zone_path`, `id`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_ZONE_OBJECTS, "WORLD_SEL_ZONE_OBJECTS", "SELECT `id`, `zone_path`, `object_id`, `template_id`, `location`, `orientation`, `scale`, `zone_tag`, `start_state`, `loading_type` FROM `zone_object` ORDER BY `zone_path`, `id`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_SERVER_CLASSES, "WORLD_SEL_SERVER_CLASSES", "SELECT `hash`, `name` FROM `server_class` ORDER BY `hash`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_SERVER_CLASS_BASES, "WORLD_SEL_SERVER_CLASS_BASES", "SELECT `class_hash`, `position`, `base_name` FROM `server_class_base` ORDER BY `class_hash`, `position`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_SERVER_CLASS_PROPERTIES, "WORLD_SEL_SERVER_CLASS_PROPERTIES", "SELECT `class_hash`, `property_id`, `name`, `type`, `hash`, `container`, `offset`, `flags`, `dynamic`, `singleton`, `pointer` FROM `server_class_property` ORDER BY `class_hash`, `property_id`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_CORE_OBJECT_TYPES, "WORLD_SEL_CORE_OBJECT_TYPES", "SELECT `block`, `type`, `class_name` FROM `core_object_type` ORDER BY `block`, `type`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_BEHAVIOR_CLIENT_CLASSES, "WORLD_SEL_BEHAVIOR_CLIENT_CLASSES", "SELECT `behavior_name`, `class_name` FROM `behavior_client_class` ORDER BY `behavior_name`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_PLAYER_LEVEL_STATS, "WORLD_SEL_PLAYER_LEVEL_STATS", "SELECT `school_id`, `level`, `xp_to_level`, `hitpoints`, `mana`, `gold`, `pip_chance`, `training_points`, `crafting_slots`, `pet_energy`, `pip_conversion_all`, `pip_conversion_fire`, `pip_conversion_ice`, `pip_conversion_storm`, `pip_conversion_life`, `pip_conversion_myth`, `pip_conversion_death`, `pip_conversion_balance`, `shadow_pip_rating`, `archmastery`, `level_name` FROM `player_level_stats` ORDER BY `school_id`, `level`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_MAGIC_SCHOOL_TEMPLATES, "WORLD_SEL_MAGIC_SCHOOL_TEMPLATES", "SELECT `school_id`, `school_name`, `min_level`, `school_index` FROM `magic_school_template` ORDER BY `school_id`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_MAGIC_SCHOOL_BADGES, "WORLD_SEL_MAGIC_SCHOOL_BADGES", "SELECT `school_id`, `position`, `badge_name` FROM `magic_school_badge` ORDER BY `school_id`, `position`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_MAGIC_XP_CONFIG, "WORLD_SEL_MAGIC_XP_CONFIG", "SELECT `name`, `value` FROM `magic_xp_config` ORDER BY `name`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_MAGIC_XP_ENCOUNTER_FACTORS, "WORLD_SEL_MAGIC_XP_ENCOUNTER_FACTORS", "SELECT `position`, `factor` FROM `magic_xp_encounter_factor` ORDER BY `position`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_MOB_RANK_LEVELS, "WORLD_SEL_MOB_RANK_LEVELS", "SELECT `rank`, `level` FROM `mob_rank_level` ORDER BY `rank`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_STAT_EFFECT_CONFIG, "WORLD_SEL_STAT_EFFECT_CONFIG", "SELECT `name`, `value` FROM `stat_effect_config` ORDER BY `name`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_STAT_CRIT_BLOCK_BANDS, "WORLD_SEL_STAT_CRIT_BLOCK_BANDS", "SELECT `min_level`, `position`, `cap_value`, `critical_hit_scalar_base`, `critical_hit_scaling_factor`, `block_scalar_base`, `block_scaling_factor` FROM `stat_crit_block_band` ORDER BY `min_level`, `position`", ConnectionFlags::Sync);
    PrepareStatement(WORLD_SEL_STAT_PIP_CONVERSION_BANDS, "WORLD_SEL_STAT_PIP_CONVERSION_BANDS", "SELECT `min_level`, `position`, `cap_value`, `scalar_base`, `scaling_factor` FROM `stat_pip_conversion_band` ORDER BY `min_level`, `position`", ConnectionFlags::Sync);
}
