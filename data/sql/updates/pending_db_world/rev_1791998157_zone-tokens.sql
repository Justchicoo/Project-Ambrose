-- Project Ambrose by Imjustchico
-- Adds zone_object.token_container, the token container a ZoneTokenInfo placement's m_pContainer holds, which the extractor keeps encoded, and the server-only zone token classes zone triggers are written in that the r806919 type dump does not describe: ReqZoneToken, which asks whether a token is set, and the results ResZoneTokenEnable, ResZoneTokenDisable and ResZoneTokenModify, which change one.
ALTER TABLE `zone_object` ADD COLUMN `token_container` LONGBLOB NULL AFTER `sigil_template`;

INSERT INTO `server_class` (`hash`, `name`, `source`, `evidence`) VALUES
    (89860789, 'class ReqZoneToken', 'authored', 'A requirement of zone triggers, 25 of them in 9 r806919 zones, such as DM_Z03_OvergrownEstate''s clue triggers. The class name hashes to the class the files give, but the type dump does not describe it. m_applyNOT and m_operator are Requirement''s own, and m_tokenName, a std::string, is the name and type the hash the files hold gives; every value names a token a ZoneTokenInfo placement of the same zone holds.'),
    (1822381948, 'class ResZoneTokenEnable', 'authored', 'A result of zone triggers, 25 of them in 13 r806919 zones. The class name hashes to the class the files give, but the type dump does not describe it. m_tokenName, a std::string naming a token of the same zone, and m_zoneWide, a bool that is always true in the files, are the names and types the hashes the files hold give.'),
    (1378561953, 'class ResZoneTokenDisable', 'authored', 'A result of zone triggers, 9 of them in 3 r806919 zones. The class name hashes to the class the files give, but the type dump does not describe it. Its properties are ResZoneTokenEnable''s, m_tokenName and m_zoneWide.'),
    (864173780, 'class ResZoneTokenModify', 'authored', 'A result of zone triggers, 1 in the r806919 install, in KT_Z05I06_MainTheater. The class name hashes to the class the files give, but the type dump does not describe it. m_tokenName, m_zoneWide and m_delta, an int, are the names and types the hashes the files hold give.')
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `source` = VALUES(`source`), `evidence` = VALUES(`evidence`);

INSERT INTO `server_class_base` (`class_hash`, `position`, `base_name`) VALUES
    (89860789, 0, 'class Requirement'),
    (89860789, 1, 'class PropertyClass'),
    (1822381948, 0, 'class Result'),
    (1822381948, 1, 'class PropertyClass'),
    (1378561953, 0, 'class Result'),
    (1378561953, 1, 'class PropertyClass'),
    (864173780, 0, 'class Result'),
    (864173780, 1, 'class PropertyClass')
ON DUPLICATE KEY UPDATE `base_name` = VALUES(`base_name`);

INSERT INTO `server_class_property` (`class_hash`, `property_id`, `name`, `type`, `hash`, `container`, `offset`, `flags`, `dynamic`, `singleton`, `pointer`) VALUES
    (89860789, 0, 'm_applyNOT', 'bool', 1746328074, 'Static', 0, 31, 0, 0, 0),
    (89860789, 1, 'm_operator', 'enum Requirement::Operator', 2672792317, 'Static', 0, 2097183, 0, 0, 0),
    (89860789, 2, 'm_tokenName', 'std::string', 1713841341, 'Static', 0, 7, 0, 0, 0),
    (1822381948, 0, 'm_tokenName', 'std::string', 1713841341, 'Static', 0, 7, 0, 0, 0),
    (1822381948, 1, 'm_zoneWide', 'bool', 676281144, 'Static', 0, 7, 0, 0, 0),
    (1378561953, 0, 'm_tokenName', 'std::string', 1713841341, 'Static', 0, 7, 0, 0, 0),
    (1378561953, 1, 'm_zoneWide', 'bool', 676281144, 'Static', 0, 7, 0, 0, 0),
    (864173780, 0, 'm_tokenName', 'std::string', 1713841341, 'Static', 0, 7, 0, 0, 0),
    (864173780, 1, 'm_zoneWide', 'bool', 676281144, 'Static', 0, 7, 0, 0, 0),
    (864173780, 2, 'm_delta', 'int', 791787588, 'Static', 0, 7, 0, 0, 0)
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `type` = VALUES(`type`), `hash` = VALUES(`hash`), `container` = VALUES(`container`), `offset` = VALUES(`offset`),
    `flags` = VALUES(`flags`), `dynamic` = VALUES(`dynamic`), `singleton` = VALUES(`singleton`), `pointer` = VALUES(`pointer`);

INSERT INTO `server_class_property_option` (`class_hash`, `property_id`, `position`, `name`, `value`) VALUES
    (89860789, 0, 0, '__DEFAULT', 0),
    (89860789, 1, 0, 'ROP_AND', 0),
    (89860789, 1, 1, 'ROP_OR', 1)
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `value` = VALUES(`value`);
