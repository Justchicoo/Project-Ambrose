-- Project Ambrose by Imjustchico
-- Gives ResWait, ResStartStagedCinematic and ResLoot the property types the zones' own triggers.xml write, which 713 zones failed to decode against: ResWait's m_secondsToWait is a double (hash 2403101108) and ResStartStagedCinematic's m_bIncludeAllPlayersInZone a bool (hash 1613790949), not the float and int first authored, and ResLoot also carries the std::string m_sourceType (hash 2305508654).
INSERT INTO `server_class_property` (`class_hash`, `property_id`, `name`, `type`, `hash`, `container`, `offset`, `flags`, `dynamic`, `singleton`, `pointer`) VALUES
    (526762782, 0, 'm_secondsToWait', 'double', 2403101108, 'Static', 0, 1, 0, 0, 0),
    (145615551, 0, 'm_bIncludeAllPlayersInZone', 'bool', 1613790949, 'Static', 0, 1, 0, 0, 0),
    (475964190, 1, 'm_sourceType', 'std::string', 2305508654, 'Static', 0, 7, 0, 0, 0)
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `type` = VALUES(`type`), `hash` = VALUES(`hash`), `container` = VALUES(`container`), `offset` = VALUES(`offset`),
    `flags` = VALUES(`flags`), `dynamic` = VALUES(`dynamic`), `singleton` = VALUES(`singleton`), `pointer` = VALUES(`pointer`);
