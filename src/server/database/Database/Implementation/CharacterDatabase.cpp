/*
 * Project Ambrose by Imjustchico
 * Registers every characters database statement with its name, SQL, and the connections that prepare it: an account's live characters in creation order, at most MaxCharactersListed of them, and one character by guid, each with its appearance, inserting a character and its appearance, soft deletion of an offline character that remembers the owner and restoring it, counting an account's live characters the way the list finds them, the online flag, and the highest guid ever used, kept in id_sequences so deleted rows cannot hand a guid out again.
 */

#include "CharacterDatabase.h"

#include <fmt/format.h>

#include <string>

void CharacterDatabaseConnection::DoPrepareStatements()
{
    PrepareStatement(CHAR_SEL_SERVER_TIME, "CHAR_SEL_SERVER_TIME", "SELECT UNIX_TIMESTAMP()", ConnectionFlags::Both);

    std::string const characterColumns = "SELECT c.`guid`, c.`account`, c.`name_indices`, c.`custom_name`, c.`should_rename`, c.`school_id`, c.`level`, c.`xp`, c.`world`, c.`zone`, c.`zone_display`, "
        "c.`pos_x`, c.`pos_y`, c.`pos_z`, c.`orientation`, c.`created`, c.`last_logout`, c.`online`, c.`deleted_at`, c.`deleted_account`, "
        "a.`behavior_template_name_id`, a.`gender`, a.`race`, a.`head_hands_model`, a.`hair_model`, a.`hat_model`, a.`torso_model`, a.`feet_model`, a.`wand_model`, "
        "a.`skin_color`, a.`skin_decal`, a.`hair_color`, a.`hat_color`, a.`hat_decal`, a.`torso_color`, a.`torso_decal`, a.`torso_decal2`, a.`feet_color`, a.`feet_decal`, "
        "a.`skin_decal2`, a.`extended_hair_color`, a.`extended_skin_decal`, a.`after_combat_dance`, a.`after_combat_victory_dance`, a.`new_player_options`, a.`new_player_options2` "
        "FROM `characters` c INNER JOIN `character_appearance` a ON a.`guid` = c.`guid`";
    PrepareStatement(CHAR_SEL_CHARACTERS_BY_ACCOUNT, "CHAR_SEL_CHARACTERS_BY_ACCOUNT", characterColumns + fmt::format(" WHERE c.`account` = ? AND c.`deleted_at` IS NULL ORDER BY c.`created`, c.`guid` LIMIT {}", MaxCharactersListed), ConnectionFlags::Both);
    PrepareStatement(CHAR_SEL_CHARACTER, "CHAR_SEL_CHARACTER", characterColumns + " WHERE c.`guid` = ?", ConnectionFlags::Both);

    PrepareStatement(CHAR_INS_CHARACTER, "CHAR_INS_CHARACTER", "INSERT INTO `characters` (`guid`, `account`, `name_indices`, `custom_name`, `should_rename`, `school_id`, `level`, `xp`, `world`, `zone`, `zone_display`, "
        "`pos_x`, `pos_y`, `pos_z`, `orientation`, `created`, `last_logout`, `online`) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", ConnectionFlags::Both);
    PrepareStatement(CHAR_INS_APPEARANCE, "CHAR_INS_APPEARANCE", "INSERT INTO `character_appearance` (`guid`, `behavior_template_name_id`, `gender`, `race`, `head_hands_model`, `hair_model`, `hat_model`, "
        "`torso_model`, `feet_model`, `wand_model`, `skin_color`, `skin_decal`, `hair_color`, `hat_color`, `hat_decal`, `torso_color`, `torso_decal`, `torso_decal2`, `feet_color`, `feet_decal`, "
        "`skin_decal2`, `extended_hair_color`, `extended_skin_decal`, `after_combat_dance`, `after_combat_victory_dance`, `new_player_options`, `new_player_options2`) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", ConnectionFlags::Both);

    PrepareStatement(CHAR_UPD_SOFT_DELETE, "CHAR_UPD_SOFT_DELETE", "UPDATE `characters` SET `deleted_at` = ?, `deleted_account` = `account`, `account` = 0 "
        "WHERE `guid` = ? AND `account` = ? AND `deleted_at` IS NULL AND `online` = 0", ConnectionFlags::Both);
    PrepareStatement(CHAR_UPD_RESTORE, "CHAR_UPD_RESTORE", "UPDATE `characters` SET `account` = `deleted_account`, `deleted_at` = NULL, `deleted_account` = NULL "
        "WHERE `guid` = ? AND `deleted_at` IS NOT NULL AND `deleted_account` IS NOT NULL", ConnectionFlags::Both);
    PrepareStatement(CHAR_SEL_COUNT_BY_ACCOUNT, "CHAR_SEL_COUNT_BY_ACCOUNT", "SELECT COUNT(*) FROM `characters` c INNER JOIN `character_appearance` a ON a.`guid` = c.`guid` "
        "WHERE c.`account` = ? AND c.`deleted_at` IS NULL", ConnectionFlags::Both);
    PrepareStatement(CHAR_UPD_ONLINE, "CHAR_UPD_ONLINE", "UPDATE `characters` SET `online` = ? WHERE `guid` = ?", ConnectionFlags::Both);
    PrepareStatement(CHAR_SEL_MAX_GUID, "CHAR_SEL_MAX_GUID", "SELECT GREATEST(COALESCE((SELECT MAX(`guid`) FROM `characters`), 0), "
        "COALESCE((SELECT `highest` FROM `id_sequences` WHERE `name` = 'character'), 0))", ConnectionFlags::Both);
    PrepareStatement(CHAR_INS_ID_SEQUENCE, "CHAR_INS_ID_SEQUENCE", "INSERT INTO `id_sequences` (`name`, `highest`) VALUES (?, ?) ON DUPLICATE KEY UPDATE `highest` = GREATEST(`highest`, ?)",
        ConnectionFlags::Both);
}
