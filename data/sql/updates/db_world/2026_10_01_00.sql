-- Project Ambrose by Imjustchico
-- Adds client_extraction, which records for each set of world tables extracted from the user's own install the revision and program it came from, so a server extracts that set again when the install moves to another revision. No row is committed: every one is written when a set is extracted.
CREATE TABLE IF NOT EXISTS `client_extraction` (
    `kind` VARCHAR(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `revision` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `executable_sha256` CHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT '',
    `extracted_at` BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (`kind`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
