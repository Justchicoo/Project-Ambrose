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
}
