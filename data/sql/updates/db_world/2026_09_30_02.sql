-- Project Ambrose by Imjustchico
-- Adds the tables the extractor's templates command fills from the user's own install, one object_template row for every template TemplateManifest.xml lists that reads as a CoreTemplate: its class and class hash, the archive and entry it lives in, its object name, display and description keys, icon, visual id, object type and loot table, with NULL where a template that is not a GameObjectTemplate has no such field; object_template_adjective, its adjectives in order; and object_template_behavior, its behavior slots in order, each by the class hash of the behavior and its m_behaviorName, empty for an empty slot; for a behavior of a class nothing describes yet, the name its bytes hold under the m_behaviorName hash every behavior template shares, or NULL when they hold none. No row is committed: every one comes from the install at run time.
CREATE TABLE IF NOT EXISTS `object_template` (
    `template_id` INT UNSIGNED NOT NULL,
    `class_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `class_hash` INT UNSIGNED NOT NULL,
    `archive` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `source_path` VARCHAR(512) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `object_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `display_key` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `description_key` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `icon` VARCHAR(512) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `visual_id` INT UNSIGNED NULL,
    `object_type` INT NULL,
    `loot_table` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    PRIMARY KEY (`template_id`),
    KEY `idx_object_template_name` (`object_name`),
    KEY `idx_object_template_class` (`class_name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `object_template_adjective` (
    `template_id` INT UNSIGNED NOT NULL,
    `position` INT UNSIGNED NOT NULL,
    `adjective` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    PRIMARY KEY (`template_id`, `position`),
    KEY `idx_object_template_adjective` (`adjective`),
    CONSTRAINT `fk_object_template_adjective_template` FOREIGN KEY (`template_id`) REFERENCES `object_template` (`template_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `object_template_behavior` (
    `template_id` INT UNSIGNED NOT NULL,
    `position` INT UNSIGNED NOT NULL,
    `class_hash` INT UNSIGNED NOT NULL DEFAULT 0,
    `behavior_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NULL,
    PRIMARY KEY (`template_id`, `position`),
    KEY `idx_object_template_behavior_name` (`behavior_name`),
    CONSTRAINT `fk_object_template_behavior_template` FOREIGN KEY (`template_id`) REFERENCES `object_template` (`template_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
