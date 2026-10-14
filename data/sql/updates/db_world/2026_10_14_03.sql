-- Project Ambrose by Imjustchico
-- Adds class ResAddGold and class ResAddMagicXP, the quest results that give a wizard gold and experience, which neither the type dump nor the install describes, with the properties the quest files spell out for them.
INSERT INTO `server_class` (`hash`, `name`, `source`, `evidence`) VALUES
    (703672453, 'class ResAddGold', 'authored', 'A result in the ResultList of quest templates, 2,246 of them in 2,245 files of a 2015-era quest package, most in a quest''s m_endResults, and none anywhere in the install. Every one holds m_gold, the gold given, and m_sourceType, where it came from; the type dump gives int and std::string to every property of those names. Only the server runs it.'),
    (1320311385, 'class ResAddMagicXP', 'authored', 'A result in the ResultList of quest templates, 2,286 of them in 2,286 files of a 2015-era quest package, most in a quest''s m_endResults, and none anywhere in the install. Every one holds m_experience, the experience given, m_sourceType, where it came from, and m_magicSchool; the type dump gives int and std::string to every property of those names. Only the server runs it.')
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `source` = VALUES(`source`), `evidence` = VALUES(`evidence`);

INSERT INTO `server_class_base` (`class_hash`, `position`, `base_name`) VALUES
    (703672453, 0, 'class Result'),
    (703672453, 1, 'class PropertyClass'),
    (1320311385, 0, 'class Result'),
    (1320311385, 1, 'class PropertyClass')
ON DUPLICATE KEY UPDATE `base_name` = VALUES(`base_name`);

INSERT INTO `server_class_property` (`class_hash`, `property_id`, `name`, `type`, `hash`, `container`, `offset`, `flags`, `dynamic`, `singleton`, `pointer`) VALUES
    (703672453, 0, 'm_gold', 'int', 219423808, 'Static', 0, 1, 0, 0, 0),
    (703672453, 1, 'm_sourceType', 'std::string', 2305508654, 'Static', 0, 1, 0, 0, 0),
    (1320311385, 0, 'm_experience', 'int', 759357570, 'Static', 0, 1, 0, 0, 0),
    (1320311385, 1, 'm_sourceType', 'std::string', 2305508654, 'Static', 0, 1, 0, 0, 0),
    (1320311385, 2, 'm_magicSchool', 'std::string', 1597012900, 'Static', 0, 1, 0, 0, 0)
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `type` = VALUES(`type`), `hash` = VALUES(`hash`), `container` = VALUES(`container`), `offset` = VALUES(`offset`), `flags` = VALUES(`flags`), `dynamic` = VALUES(`dynamic`), `singleton` = VALUES(`singleton`), `pointer` = VALUES(`pointer`);
