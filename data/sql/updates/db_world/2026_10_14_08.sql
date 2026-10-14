-- Project Ambrose by Imjustchico
-- Adds the quest results the type dump does not describe that start a quest's cinematics and beats: ResPostEvent, which posts an event into the wizard's zone, ResWait, which holds the rest of its result list back, and ResStartStagedCinematic, which starts a staged cinematic for the wizard or everyone in its zone.
INSERT INTO `server_class` (`hash`, `name`, `source`, `evidence`) VALUES
    (412163800, 'class ResPostEvent', 'authored', 'A result in quest and goal result lists, 358 of them in 238 quest files, posting an event zone triggers listen for, such as the tutorial''s Storm and WalkAmbrose. The class name hashes to the class the files give, but the type dump does not describe it. m_eventName and m_subEventName are the names the files write, read as strings.'),
    (526762782, 'class ResWait', 'authored', 'A result in quest and goal result lists, 115 of them in 86 quest files, between the beats of a cinematic sequence. The class name hashes to the class the files give, but the type dump does not describe it. m_secondsToWait is the name the files write, read as a float.'),
    (145615551, 'class ResStartStagedCinematic', 'authored', 'A result in goal result lists, 4 of them, naming a staged cinematic of Cinematics/ in Root.wad and the stage it starts at. The class name hashes to the class the files give, but the type dump does not describe it. m_bIncludeAllPlayersInZone, read as an int, m_cinematicName and m_stageName, read as strings, are the names the files write.')
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `source` = VALUES(`source`), `evidence` = VALUES(`evidence`);

INSERT INTO `server_class_base` (`class_hash`, `position`, `base_name`) VALUES
    (412163800, 0, 'class Result'),
    (412163800, 1, 'class PropertyClass'),
    (526762782, 0, 'class Result'),
    (526762782, 1, 'class PropertyClass'),
    (145615551, 0, 'class Result'),
    (145615551, 1, 'class PropertyClass')
ON DUPLICATE KEY UPDATE `base_name` = VALUES(`base_name`);

INSERT INTO `server_class_property` (`class_hash`, `property_id`, `name`, `type`, `hash`, `container`, `offset`, `flags`, `dynamic`, `singleton`, `pointer`) VALUES
    (412163800, 0, 'm_eventName', 'std::string', 3493260286, 'Static', 0, 1, 0, 0, 0),
    (412163800, 1, 'm_subEventName', 'std::string', 3298450984, 'Static', 0, 1, 0, 0, 0),
    (526762782, 0, 'm_secondsToWait', 'float', 595492126, 'Static', 0, 1, 0, 0, 0),
    (145615551, 0, 'm_bIncludeAllPlayersInZone', 'int', 1611309772, 'Static', 0, 1, 0, 0, 0),
    (145615551, 1, 'm_cinematicName', 'std::string', 2611527497, 'Static', 0, 1, 0, 0, 0),
    (145615551, 2, 'm_stageName', 'std::string', 2436165968, 'Static', 0, 1, 0, 0, 0)
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `type` = VALUES(`type`), `hash` = VALUES(`hash`), `container` = VALUES(`container`), `offset` = VALUES(`offset`),
    `flags` = VALUES(`flags`), `dynamic` = VALUES(`dynamic`), `singleton` = VALUES(`singleton`), `pointer` = VALUES(`pointer`);
