-- Project Ambrose by Imjustchico
-- Adds the classes a zone's spawnData.xml and its triggers' spawn results are written in that the r806919 type dump does not describe: WizSpawnObjectInfo, the SpawnObjectInfo with a loot table some spawners place, and the trigger results ResSpawn and ResDespawn, which start a spawner and take a spawner's objects away with a despawn effect named by its string.
INSERT INTO `server_class` (`hash`, `name`, `source`, `evidence`) VALUES
    (1839222684, 'class WizSpawnObjectInfo', 'authored', 'The object a SpawnItem places in many spawners, 1 of 5 in WC_Ravenwood and 5 of 11 in WC_Hub, the others placing a plain SpawnObjectInfo. The class name hashes to the class the files give and the client program has a class of that name, but neither the type dump nor the install''s class file describes it. Its properties are SpawnObjectInfo''s in the files, followed by a string whose hash m_lootTable of type std::string gives, always empty, and three fields no name fits yet, two 32-bit and one 8-bit, kept under their hashes.'),
    (723600258, 'class ResSpawn', 'authored', 'A result in the ResultList of zone triggers, 6524 of them in the install, the most common. The class name hashes to the class the files give, but the type dump does not describe it. m_spawnID, a gid naming a SpawnObject''s m_id in the same zone, and m_activate are the names and types the hashes the files hold give. WC_Duel_Arena''s Trigger Start Dueling spawns the arena''s inactive spawners with it.'),
    (1383450208, 'class ResDespawn', 'authored', 'A result in the ResultList of zone triggers, 4943 of them in the install. The class name hashes to the class the files give, but the type dump does not describe it. m_spawnID, m_templateID and m_despawnEffect are the names and types the hashes the files hold give; m_despawnEffect is empty in 4930 and WispDespawn in 13, a name whose KiStringHash the client''s despawn handler compares DespawnInfo.m_despawnEffect with.')
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `source` = VALUES(`source`), `evidence` = VALUES(`evidence`);

INSERT INTO `server_class_base` (`class_hash`, `position`, `base_name`) VALUES
    (1839222684, 0, 'class SpawnObjectInfo'),
    (1839222684, 1, 'class CoreObjectInfo'),
    (1839222684, 2, 'class PropertyClass'),
    (723600258, 0, 'class Result'),
    (723600258, 1, 'class PropertyClass'),
    (1383450208, 0, 'class Result'),
    (1383450208, 1, 'class PropertyClass')
ON DUPLICATE KEY UPDATE `base_name` = VALUES(`base_name`);

INSERT INTO `server_class_property` (`class_hash`, `property_id`, `name`, `type`, `hash`, `container`, `offset`, `flags`, `dynamic`, `singleton`, `pointer`) VALUES
    (1839222684, 0, 'm_templateID.m_full', 'unsigned __int64', 633907631, 'Static', 0, 33554439, 0, 0, 0),
    (1839222684, 1, 'm_nObjectID', 'unsigned int', 748496927, 'Static', 0, 7, 0, 0, 0),
    (1839222684, 2, 'm_location', 'class Vector3D', 2239683611, 'Static', 0, 7, 0, 0, 0),
    (1839222684, 3, 'm_orientation', 'class Vector3D', 2344058766, 'Static', 0, 7, 0, 0, 0),
    (1839222684, 4, 'm_fScale', 'float', 503137701, 'Static', 0, 7, 0, 0, 0),
    (1839222684, 5, 'm_zoneTag', 'std::string', 3405382643, 'Static', 0, 7, 0, 0, 0),
    (1839222684, 6, 'm_startState', 'std::string', 2166880458, 'Static', 0, 7, 0, 0, 0),
    (1839222684, 7, 'm_overrideName', 'std::string', 1990707228, 'Static', 0, 8388615, 0, 0, 0),
    (1839222684, 8, 'm_globalDynamic', 'bool', 1951691017, 'Static', 0, 7, 0, 0, 0),
    (1839222684, 9, 'm_bUndetectable', 'bool', 1907454341, 'Static', 0, 7, 0, 0, 0),
    (1839222684, 10, 'm_spawnRequirements', 'class SharedPointer<class RequirementList>', 2309120870, 'Static', 0, 7, 0, 0, 1),
    (1839222684, 11, 'm_loadingType', 'enum CoreObjectInfo::LoadingType', 3522422550, 'Static', 0, 2097159, 0, 0, 0),
    (1839222684, 12, 'm_kStartNodeType', 'enum SpawnObjectInfo::StartNodeType', 1545814623, 'Static', 0, 2097159, 0, 0, 0),
    (1839222684, 13, 'm_startNode', 'unsigned int', 1598161889, 'Static', 0, 7, 0, 0, 0),
    (1839222684, 14, 'm_pathID', 'gid', 821494322, 'Static', 0, 7, 0, 0, 0),
    (1839222684, 15, 'm_uniqueLoc', 'char', 400014505, 'Static', 0, 7, 0, 0, 0),
    (1839222684, 16, 'm_lootTable', 'std::string', 2539512673, 'Static', 0, 7, 0, 0, 0),
    (1839222684, 17, '#1254251407', 'unsigned int', 1254251407, 'Static', 0, 7, 0, 0, 0),
    (1839222684, 18, '#1645979895', 'unsigned int', 1645979895, 'Static', 0, 7, 0, 0, 0),
    (1839222684, 19, '#3028959106', 'bool', 3028959106, 'Static', 0, 7, 0, 0, 0),
    (723600258, 0, 'm_spawnID', 'gid', 1481718190, 'Static', 0, 7, 0, 0, 0),
    (723600258, 1, 'm_activate', 'bool', 142527940, 'Static', 0, 7, 0, 0, 0),
    (1383450208, 0, 'm_spawnID', 'gid', 1481718190, 'Static', 0, 7, 0, 0, 0),
    (1383450208, 1, 'm_templateID', 'int', 1075344419, 'Static', 0, 7, 0, 0, 0),
    (1383450208, 2, 'm_despawnEffect', 'std::string', 2763880570, 'Static', 0, 7, 0, 0, 0)
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `type` = VALUES(`type`), `hash` = VALUES(`hash`), `container` = VALUES(`container`), `offset` = VALUES(`offset`),
    `flags` = VALUES(`flags`), `dynamic` = VALUES(`dynamic`), `singleton` = VALUES(`singleton`), `pointer` = VALUES(`pointer`);

INSERT INTO `server_class_property_option` (`class_hash`, `property_id`, `position`, `name`, `value`) VALUES
    (1839222684, 11, 0, 'STATIC_CLIENT_SERVER', 0),
    (1839222684, 11, 1, 'STATIC_CLIENT', 1),
    (1839222684, 11, 2, 'STATIC_SERVER', 2),
    (1839222684, 11, 3, 'DYNAMIC_SERVER', 3),
    (1839222684, 12, 0, 'SNT_RANDOM', 0),
    (1839222684, 12, 1, 'SNT_RANDOM_UNIQUE', 1),
    (1839222684, 12, 2, 'SNT_FIRST', 2),
    (1839222684, 12, 3, 'SNT_LAST', 3),
    (1839222684, 12, 4, 'SNT_SPECIFIC', 4),
    (1839222684, 13, 0, '__DEFAULT', 0)
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `value` = VALUES(`value`);
