-- Project Ambrose by Imjustchico
-- Adds the authored quest tables sQuestMgr loads and reloads through quest_template: quest_template, one row per quest keyed by its name with its keys, level, repeat and flags; quest_start_goal, the goals a quest starts with; quest_goal, each goal with its type, keys, destination, persona, tally and tags; quest_goal_adjective and quest_goal_client_tag, a goal's adjectives and client tags in order; quest_goal_logic, what completing a goal does, and quest_goal_logic_member, the goals each logic entry needs all of (AND), any of (OR) or adds (ADD); quest_dialog, the Start, Prep, Underway, Completion and Complete dialogs of a quest or one of its goals; quest_dialog_entry, each line of a dialog in order; and quest_dialog_madlib, the madlib values a line fills in. Every table is plain columns, no JSON, so MySQL 8.0 and MariaDB 10.6 read it alike.
CREATE TABLE IF NOT EXISTS `quest_template` (
    `name` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `name_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `title_key` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `info_key` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `prep_key` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `underway_key` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `complete_key` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `level` INT UNSIGNED NOT NULL DEFAULT 0,
    `repeat` INT UNSIGNED NOT NULL DEFAULT 0,
    `mainline` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `no_quest_helper` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `skip_qh_autoselect` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `pet_only` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `activity_type` INT UNSIGNED NOT NULL DEFAULT 0,
    `prep_always` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `is_hidden` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `quest_start_goal` (
    `quest` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `goal_name` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    PRIMARY KEY (`quest`, `goal_name`),
    CONSTRAINT `fk_quest_start_goal_quest` FOREIGN KEY (`quest`) REFERENCES `quest_template` (`name`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `quest_goal` (
    `quest` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `goal_name` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `name_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `type` VARCHAR(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `title_key` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `underway_key` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `complete_key` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `location_key` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `destination_zone` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `image1` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `image2` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `persona_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `use_patron` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `tally_count` INT UNSIGNED NOT NULL DEFAULT 0,
    `tally_percent` INT UNSIGNED NOT NULL DEFAULT 0,
    `tally_descriptor_key` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `tally_descriptor2_key` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `zone_entry` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `zone_exit` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `zone_tag` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `proximity_tag` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `rank` INT UNSIGNED NOT NULL DEFAULT 0,
    `hide_floaty_text` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `hide_location` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`quest`, `goal_name`),
    KEY `idx_quest_goal_type` (`type`),
    KEY `idx_quest_goal_persona` (`persona_name`),
    KEY `idx_quest_goal_destination` (`destination_zone`),
    CONSTRAINT `chk_quest_goal_type` CHECK (`type` IN ('persona', 'waypoint', 'bounty', 'bounty_collect', 'scavenge', 'usage', 'achieve_rank')),
    CONSTRAINT `fk_quest_goal_quest` FOREIGN KEY (`quest`) REFERENCES `quest_template` (`name`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `quest_goal_adjective` (
    `quest` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `goal_name` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `position` INT UNSIGNED NOT NULL,
    `adjective` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    PRIMARY KEY (`quest`, `goal_name`, `position`),
    KEY `idx_quest_goal_adjective_adjective` (`adjective`),
    CONSTRAINT `fk_quest_goal_adjective_goal` FOREIGN KEY (`quest`, `goal_name`) REFERENCES `quest_goal` (`quest`, `goal_name`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `quest_goal_client_tag` (
    `quest` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `goal_name` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `position` INT UNSIGNED NOT NULL,
    `tag` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    PRIMARY KEY (`quest`, `goal_name`, `position`),
    KEY `idx_quest_goal_client_tag_tag` (`tag`),
    CONSTRAINT `fk_quest_goal_client_tag_goal` FOREIGN KEY (`quest`, `goal_name`) REFERENCES `quest_goal` (`quest`, `goal_name`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `quest_goal_logic` (
    `quest` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `goal_name` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `position` INT UNSIGNED NOT NULL,
    `complete_quest` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`quest`, `goal_name`, `position`),
    CONSTRAINT `fk_quest_goal_logic_quest` FOREIGN KEY (`quest`) REFERENCES `quest_template` (`name`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `quest_goal_logic_member` (
    `quest` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `goal_name` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `logic` INT UNSIGNED NOT NULL,
    `kind` VARCHAR(3) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `position` INT UNSIGNED NOT NULL,
    `member_goal` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    PRIMARY KEY (`quest`, `goal_name`, `logic`, `kind`, `position`),
    CONSTRAINT `chk_quest_goal_logic_member_kind` CHECK (`kind` IN ('AND', 'OR', 'ADD')),
    CONSTRAINT `fk_quest_goal_logic_member_logic` FOREIGN KEY (`quest`, `goal_name`, `logic`) REFERENCES `quest_goal_logic` (`quest`, `goal_name`, `position`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `quest_dialog` (
    `quest` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `owner` VARCHAR(5) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `goal_name` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `tag` VARCHAR(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    PRIMARY KEY (`quest`, `owner`, `goal_name`, `tag`),
    CONSTRAINT `chk_quest_dialog_owner` CHECK ((`owner` = 'quest' AND `goal_name` = '') OR (`owner` = 'goal' AND `goal_name` <> '')),
    CONSTRAINT `chk_quest_dialog_tag` CHECK (`tag` IN ('Start', 'Prep', 'Underway', 'Completion', 'Complete')),
    CONSTRAINT `fk_quest_dialog_quest` FOREIGN KEY (`quest`) REFERENCES `quest_template` (`name`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `quest_dialog_entry` (
    `quest` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `owner` VARCHAR(5) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `goal_name` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `tag` VARCHAR(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `position` INT UNSIGNED NOT NULL,
    `actor_template_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `dialog_key` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `picture` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `sound` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `action` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `dialog_event` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `animation` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    PRIMARY KEY (`quest`, `owner`, `goal_name`, `tag`, `position`),
    KEY `idx_quest_dialog_entry_actor` (`actor_template_id`),
    CONSTRAINT `fk_quest_dialog_entry_dialog` FOREIGN KEY (`quest`, `owner`, `goal_name`, `tag`) REFERENCES `quest_dialog` (`quest`, `owner`, `goal_name`, `tag`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `quest_dialog_madlib` (
    `quest` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `owner` VARCHAR(5) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `goal_name` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `tag` VARCHAR(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `entry` INT UNSIGNED NOT NULL,
    `identifier` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `value` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    PRIMARY KEY (`quest`, `owner`, `goal_name`, `tag`, `entry`, `identifier`),
    CONSTRAINT `fk_quest_dialog_madlib_entry` FOREIGN KEY (`quest`, `owner`, `goal_name`, `tag`, `entry`) REFERENCES `quest_dialog_entry` (`quest`, `owner`, `goal_name`, `tag`, `position`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
