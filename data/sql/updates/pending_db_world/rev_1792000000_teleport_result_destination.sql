-- Project Ambrose by Imjustchico
-- Gives ResTeleport the destination quest results write on it, which the type dump lists the class without: m_destinationZone and m_destinationLoc, so a quest's start, end and goal results can send the wizard somewhere while zone doors keep zone_teleport.
INSERT INTO `server_class` (`hash`, `name`, `source`, `evidence`) VALUES
    (228794493, 'class ResTeleport', 'authored', 'A result in quest, goal and start result lists, 40 of them in CSR quest files, such as WC-GRZ-PRE-002 whose start results send the wizard to GrizzleheimLite/GH_GrizzleheimHubLite. The type dump lists the class with no properties because they are the server''s own. m_destinationZone and m_destinationLoc are the names the files write, read as strings; the zone doors the client data holds write neither.')
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `source` = VALUES(`source`), `evidence` = VALUES(`evidence`);

INSERT INTO `server_class_base` (`class_hash`, `position`, `base_name`) VALUES
    (228794493, 0, 'class Result'),
    (228794493, 1, 'class PropertyClass')
ON DUPLICATE KEY UPDATE `base_name` = VALUES(`base_name`);

INSERT INTO `server_class_property` (`class_hash`, `property_id`, `name`, `type`, `hash`, `container`, `offset`, `flags`, `dynamic`, `singleton`, `pointer`) VALUES
    (228794493, 0, 'm_destinationZone', 'std::string', 2605356153, 'Static', 0, 1, 0, 0, 0),
    (228794493, 1, 'm_destinationLoc', 'std::string', 1531335483, 'Static', 0, 1, 0, 0, 0)
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `type` = VALUES(`type`), `hash` = VALUES(`hash`), `container` = VALUES(`container`), `offset` = VALUES(`offset`),
    `flags` = VALUES(`flags`), `dynamic` = VALUES(`dynamic`), `singleton` = VALUES(`singleton`), `pointer` = VALUES(`pointer`);
