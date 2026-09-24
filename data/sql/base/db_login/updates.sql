-- Project Ambrose by Imjustchico
-- Creates the updates table that records every update file applied to the login database with its SHA-256 hash, state, time and duration.
CREATE TABLE IF NOT EXISTS `updates` (
    `name` VARCHAR(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `hash` CHAR(64) NOT NULL DEFAULT '',
    `state` ENUM('RELEASED', 'CUSTOM', 'MODULE', 'ARCHIVED', 'PENDING') NOT NULL DEFAULT 'RELEASED',
    `timestamp` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `speed` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
