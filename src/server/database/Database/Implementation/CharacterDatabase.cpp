/*
 * Project Ambrose by Imjustchico
 * Registers every characters database statement with its name, SQL, and the connections that prepare it: an account's live characters in creation order, at most MaxCharactersListed of them, and one character by guid, each with its appearance, inserting a character and its appearance, soft deletion of an offline character that remembers the owner and restoring it, the newest deleted characters, of one account or all, and asking a wizard to choose a new name at its next login, counting an account's live characters the way the list finds them, the online flag, and the highest guid ever used, kept in id_sequences so deleted rows cannot hand a guid out again. It also registers the live settings statements: every persisted value, setting and removing one, writing a change's audit row, and reading a key's newest audit rows; and a wizard's character_stats row, read by guid through its character so the read returns a row whenever the wizard exists and an empty result means it failed, and written whole, inserted or replacing the one there only when the write's revision is newer than the row's, the revision assigned last so every column is judged against the revision the row had; a wizard's position, written with the next revision of its character row only when the row's is older; and a wizard's spellbook rows, read through its character in the order it learned them, and each written under the spellbook's next revision, inserted or changing the one there only when the write's revision is newer. It also registers the social statements: a wizard's friends, the wizards it ignores and the friend requests sent to it, each with the other wizard's name and only while that wizard is not deleted, whether one request exists, adding and removing a request, a friend and an ignore, and setting a friend's best-friend symbol.
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
        "a.`skin_decal2`, a.`extended_hair_color`, a.`extended_skin_decal`, a.`after_combat_dance`, a.`after_combat_victory_dance`, a.`new_player_options`, a.`new_player_options2`, c.`state_revision` "
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
    PrepareStatement(CHAR_SEL_DELETED, "CHAR_SEL_DELETED", "SELECT `guid`, `deleted_account`, `deleted_at`, `level`, `school_id` FROM `characters` "
        "WHERE `deleted_at` IS NOT NULL AND `deleted_account` IS NOT NULL AND (? = 0 OR `deleted_account` = ?) ORDER BY `deleted_at` DESC LIMIT ?", ConnectionFlags::Both);
    PrepareStatement(CHAR_UPD_SHOULD_RENAME, "CHAR_UPD_SHOULD_RENAME", "UPDATE `characters` SET `should_rename` = 1 WHERE `guid` = ? AND `deleted_at` IS NULL", ConnectionFlags::Both);
    PrepareStatement(CHAR_DEL_CHARACTER, "CHAR_DEL_CHARACTER", "DELETE FROM `characters` WHERE `guid` = ? AND `account` = ? AND `deleted_at` IS NULL AND `online` = 0",
        ConnectionFlags::Both);
    PrepareStatement(CHAR_DEL_DELETED_BEFORE, "CHAR_DEL_DELETED_BEFORE", "DELETE FROM `characters` WHERE `deleted_at` IS NOT NULL AND `deleted_at` < ?", ConnectionFlags::Both);
    PrepareStatement(CHAR_SEL_COUNT_BY_ACCOUNT, "CHAR_SEL_COUNT_BY_ACCOUNT", "SELECT COUNT(*) FROM `characters` c INNER JOIN `character_appearance` a ON a.`guid` = c.`guid` "
        "WHERE c.`account` = ? AND c.`deleted_at` IS NULL", ConnectionFlags::Both);
    PrepareStatement(CHAR_UPD_ONLINE, "CHAR_UPD_ONLINE", "UPDATE `characters` SET `online` = ? WHERE `guid` = ?", ConnectionFlags::Both);
    PrepareStatement(CHAR_SEL_MAX_GUID, "CHAR_SEL_MAX_GUID", "SELECT GREATEST(COALESCE((SELECT MAX(`guid`) FROM `characters`), 0), "
        "COALESCE((SELECT `highest` FROM `id_sequences` WHERE `name` = 'character'), 0))", ConnectionFlags::Both);
    PrepareStatement(CHAR_INS_ID_SEQUENCE, "CHAR_INS_ID_SEQUENCE", "INSERT INTO `id_sequences` (`name`, `highest`) VALUES (?, ?) ON DUPLICATE KEY UPDATE `highest` = GREATEST(`highest`, ?)",
        ConnectionFlags::Both);
    PrepareStatement(CHAR_SEL_SETTINGS, "CHAR_SEL_SETTINGS", "SELECT `key`, `value` FROM `settings`", ConnectionFlags::Both);
    PrepareStatement(CHAR_REP_SETTING, "CHAR_REP_SETTING", "INSERT INTO `settings` (`key`, `value`, `updated_by`, `updated_at`) VALUES (?, ?, ?, ?) ON DUPLICATE KEY UPDATE `value` = VALUES(`value`), `updated_by` = VALUES(`updated_by`), `updated_at` = VALUES(`updated_at`)", ConnectionFlags::Both);
    PrepareStatement(CHAR_DEL_SETTING, "CHAR_DEL_SETTING", "DELETE FROM `settings` WHERE `key` = ?", ConnectionFlags::Both);
    PrepareStatement(CHAR_INS_SETTING_AUDIT, "CHAR_INS_SETTING_AUDIT", "INSERT INTO `setting_audit` (`key`, `old_value`, `new_value`, `who`, `account_id`, `source`, `reason`, `created`) VALUES (?, ?, ?, ?, ?, ?, ?, ?)", ConnectionFlags::Both);
    PrepareStatement(CHAR_SEL_SETTING_AUDIT, "CHAR_SEL_SETTING_AUDIT", "SELECT `id`, `key`, `old_value`, `new_value`, `who`, `account_id`, `source`, `reason`, `created` FROM `setting_audit` WHERE `key` = ? ORDER BY `id` DESC LIMIT ?", ConnectionFlags::Both);
    PrepareStatement(CHAR_SEL_CHARACTER_STATS, "CHAR_SEL_CHARACTER_STATS", "SELECT s.`guid` IS NOT NULL, s.`overflow_xp`, s.`secondary_school_id`, s.`training_points`, s.`gold`, s.`health`, s.`mana`, "
        "s.`potion_charge`, s.`potion_max`, s.`arena_points`, s.`level_locked`, s.`purchased_custom_emotes_1`, s.`purchased_custom_emotes_2`, s.`purchased_custom_emotes_3`, "
        "s.`purchased_custom_teleport_effects_1`, s.`purchased_custom_teleport_effects_2`, s.`purchased_custom_teleport_effects_3`, s.`revision` "
        "FROM `characters` c LEFT JOIN `character_stats` s ON s.`guid` = c.`guid` WHERE c.`guid` = ?", ConnectionFlags::Both);
    std::string newer;
    for (char const* const column : { "overflow_xp", "secondary_school_id", "training_points", "gold", "health", "mana", "potion_charge", "potion_max", "arena_points", "level_locked",
             "purchased_custom_emotes_1", "purchased_custom_emotes_2", "purchased_custom_emotes_3", "purchased_custom_teleport_effects_1", "purchased_custom_teleport_effects_2",
             "purchased_custom_teleport_effects_3" })
        newer += fmt::format("`{0}` = IF(VALUES(`revision`) > `revision`, VALUES(`{0}`), `{0}`), ", column);
    PrepareStatement(CHAR_REP_CHARACTER_STATS, "CHAR_REP_CHARACTER_STATS", "INSERT INTO `character_stats` (`guid`, `overflow_xp`, `secondary_school_id`, `training_points`, `gold`, `health`, `mana`, "
        "`potion_charge`, `potion_max`, `arena_points`, `level_locked`, `purchased_custom_emotes_1`, `purchased_custom_emotes_2`, `purchased_custom_emotes_3`, "
        "`purchased_custom_teleport_effects_1`, `purchased_custom_teleport_effects_2`, `purchased_custom_teleport_effects_3`, `revision`) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) ON DUPLICATE KEY UPDATE " + newer
        + "`revision` = GREATEST(`revision`, VALUES(`revision`))", ConnectionFlags::Both);
    PrepareStatement(CHAR_UPD_POSITION, "CHAR_UPD_POSITION", "UPDATE `characters` SET `pos_x` = ?, `pos_y` = ?, `pos_z` = ?, `orientation` = ?, `state_revision` = ? WHERE `guid` = ? AND `state_revision` < ?",
        ConnectionFlags::Both);
    PrepareStatement(CHAR_SEL_CHARACTER_SPELLS, "CHAR_SEL_CHARACTER_SPELLS", "SELECT s.`guid` IS NOT NULL, s.`spell_id`, s.`known`, s.`learned`, s.`revision` FROM `characters` c "
        "LEFT JOIN `character_spell` s ON s.`guid` = c.`guid` WHERE c.`guid` = ? ORDER BY s.`learned`, s.`spell_id`", ConnectionFlags::Both);
    PrepareStatement(CHAR_REP_CHARACTER_SPELL, "CHAR_REP_CHARACTER_SPELL", "INSERT INTO `character_spell` (`guid`, `spell_id`, `known`, `learned`, `revision`) VALUES (?, ?, ?, ?, ?) "
        "ON DUPLICATE KEY UPDATE `known` = IF(VALUES(`revision`) > `revision`, VALUES(`known`), `known`), `learned` = IF(VALUES(`revision`) > `revision`, VALUES(`learned`), `learned`), "
        "`revision` = GREATEST(`revision`, VALUES(`revision`))", ConnectionFlags::Both);

    PrepareStatement(CHAR_SEL_SOCIAL_FRIENDS, "CHAR_SEL_SOCIAL_FRIENDS", "SELECT f.`friend_guid`, f.`best_friend_symbol`, f.`date`, c.`custom_name`, c.`name_indices`, a.`gender` "
        "FROM `character_friend` f INNER JOIN `characters` c ON c.`guid` = f.`friend_guid` "
        "INNER JOIN `character_appearance` a ON a.`guid` = c.`guid` WHERE f.`owner_guid` = ? AND c.`deleted_at` IS NULL ORDER BY f.`date`, f.`friend_guid`", ConnectionFlags::Both);
    PrepareStatement(CHAR_SEL_SOCIAL_IGNORES, "CHAR_SEL_SOCIAL_IGNORES", "SELECT i.`ignored_guid`, i.`platform_type`, c.`custom_name`, c.`name_indices`, a.`gender` "
        "FROM `character_ignore` i INNER JOIN `characters` c ON c.`guid` = i.`ignored_guid` INNER JOIN `character_appearance` a ON a.`guid` = c.`guid` "
        "WHERE i.`owner_guid` = ? AND c.`deleted_at` IS NULL ORDER BY i.`date`, i.`ignored_guid`", ConnectionFlags::Both);
    PrepareStatement(CHAR_SEL_SOCIAL_REQUESTS, "CHAR_SEL_SOCIAL_REQUESTS", "SELECT r.`requester_guid`, c.`custom_name`, c.`name_indices`, a.`gender`, c.`level` "
        "FROM `character_friend_request` r INNER JOIN `characters` c ON c.`guid` = r.`requester_guid` INNER JOIN `character_appearance` a ON a.`guid` = c.`guid` "
        "WHERE r.`target_guid` = ? AND c.`deleted_at` IS NULL ORDER BY r.`date`, r.`requester_guid`", ConnectionFlags::Both);
    PrepareStatement(CHAR_SEL_SOCIAL_REQUEST_EXISTS, "CHAR_SEL_SOCIAL_REQUEST_EXISTS", "SELECT 1 FROM `character_friend_request` WHERE `requester_guid` = ? AND `target_guid` = ?", ConnectionFlags::Both);
    PrepareStatement(CHAR_INS_SOCIAL_REQUEST, "CHAR_INS_SOCIAL_REQUEST", "INSERT INTO `character_friend_request` (`requester_guid`, `target_guid`, `date`) VALUES (?, ?, ?) "
        "ON DUPLICATE KEY UPDATE `date` = VALUES(`date`)", ConnectionFlags::Both);
    PrepareStatement(CHAR_DEL_SOCIAL_REQUEST, "CHAR_DEL_SOCIAL_REQUEST", "DELETE FROM `character_friend_request` WHERE `requester_guid` = ? AND `target_guid` = ?", ConnectionFlags::Both);
    PrepareStatement(CHAR_INS_SOCIAL_FRIEND, "CHAR_INS_SOCIAL_FRIEND", "INSERT INTO `character_friend` (`owner_guid`, `friend_guid`, `best_friend_symbol`, `date`) VALUES (?, ?, ?, ?) "
        "ON DUPLICATE KEY UPDATE `best_friend_symbol` = VALUES(`best_friend_symbol`), `date` = VALUES(`date`)", ConnectionFlags::Both);
    PrepareStatement(CHAR_DEL_SOCIAL_FRIEND, "CHAR_DEL_SOCIAL_FRIEND", "DELETE FROM `character_friend` WHERE `owner_guid` = ? AND `friend_guid` = ?", ConnectionFlags::Both);
    PrepareStatement(CHAR_UPD_SOCIAL_BEST_FRIEND, "CHAR_UPD_SOCIAL_BEST_FRIEND", "UPDATE `character_friend` SET `best_friend_symbol` = ? WHERE `owner_guid` = ? AND `friend_guid` = ?", ConnectionFlags::Both);
    PrepareStatement(CHAR_INS_SOCIAL_IGNORE, "CHAR_INS_SOCIAL_IGNORE", "INSERT INTO `character_ignore` (`owner_guid`, `ignored_guid`, `platform_type`, `date`) VALUES (?, ?, ?, ?) "
        "ON DUPLICATE KEY UPDATE `platform_type` = VALUES(`platform_type`), `date` = VALUES(`date`)", ConnectionFlags::Both);
    PrepareStatement(CHAR_DEL_SOCIAL_IGNORE, "CHAR_DEL_SOCIAL_IGNORE", "DELETE FROM `character_ignore` WHERE `owner_guid` = ? AND `ignored_guid` = ?", ConnectionFlags::Both);
}
