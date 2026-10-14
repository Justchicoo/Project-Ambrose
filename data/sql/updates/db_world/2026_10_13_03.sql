-- Project Ambrose by Imjustchico
-- Adds quest_result, the results a quest runs when it completes, under an empty goal_name, and each of its goals runs when that goal completes, in position order, each a result class and the versionable bytes the data holds it in, as zone_trigger_result keeps a trigger's, and its rows go with their quest when the quest's row is replaced or removed.
CREATE TABLE IF NOT EXISTS `quest_result` (
    `quest` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `goal_name` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `position` INT UNSIGNED NOT NULL,
    `class_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `data` BLOB NOT NULL,
    PRIMARY KEY (`quest`, `goal_name`, `position`),
    CONSTRAINT `fk_quest_result_quest` FOREIGN KEY (`quest`) REFERENCES `quest_template` (`name`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
