-- Project Ambrose by Imjustchico
-- Adds class ResRemoveDynaMod, the quest result that takes a wizard's Dynamod away by its tag, which the client program registers under that name but neither the type dump nor the install describes, and names two of the three ResAddDynaMod bools the quest files spell out, m_useQuestAsOriginator and m_bShowsObject, each proven by hashing to the property the zone files hold.
INSERT INTO `server_class` (`hash`, `name`, `source`, `evidence`) VALUES
    (552459800, 'class ResRemoveDynaMod', 'authored', 'A result in the ResultList of quest templates, 1,245 of them across 640 quest files of a 2015-era quest package, 1,026 in a goal''s m_completeResults, 207 in a quest''s m_startResults and 12 in its m_endResults, and none anywhere in the install. The r806919 client program registers a factory under this name, which hashes to 552459800, but describes no properties for it: only the server runs it. Every one the quest files hold carries exactly m_dynaModClientTag, a tag text such as a quest light''s, and m_useQuestAsOriginator, 0 in every one read, and names no zone, so it takes the wizard''s mod of that tag in whatever zone it was added; the same tag is often removed and re-added in one list. Both properties are typed as ResAddDynaMod''s properties of the same names are, and the type dump gives std::string and bool to every property of those names.')
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `source` = VALUES(`source`), `evidence` = VALUES(`evidence`);

INSERT INTO `server_class_base` (`class_hash`, `position`, `base_name`) VALUES
    (552459800, 0, 'class Result'),
    (552459800, 1, 'class PropertyClass')
ON DUPLICATE KEY UPDATE `base_name` = VALUES(`base_name`);

INSERT INTO `server_class_property` (`class_hash`, `property_id`, `name`, `type`, `hash`, `container`, `offset`, `flags`, `dynamic`, `singleton`, `pointer`) VALUES
    (552459800, 0, 'm_dynaModClientTag', 'std::string', 1601665858, 'Static', 0, 1, 0, 0, 0),
    (552459800, 1, 'm_useQuestAsOriginator', 'bool', 1110452292, 'Static', 0, 1, 0, 0, 0),
    (3938346, 1, 'm_useQuestAsOriginator', 'bool', 1110452292, 'Static', 0, 1, 0, 0, 0),
    (3938346, 3, 'm_bShowsObject', 'bool', 35044672, 'Static', 0, 1, 0, 0, 0)
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `type` = VALUES(`type`), `hash` = VALUES(`hash`), `container` = VALUES(`container`), `offset` = VALUES(`offset`), `flags` = VALUES(`flags`), `dynamic` = VALUES(`dynamic`), `singleton` = VALUES(`singleton`), `pointer` = VALUES(`pointer`);
