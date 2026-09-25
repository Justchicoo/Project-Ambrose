-- Project Ambrose by Imjustchico
-- Adds what serializing game objects needs to know about the client beyond its type dump: server_class with its bases and properties, the classes the client uses that its dump does not describe, in the dump's own terms and checked against the client's hash formula whenever they load; core_object_type, the block and type a game object's CoreObject header gives for a class; and behavior_client_class, the class the client builds for each behavior a template names, which no dump can say because the pairing lives in the client program's code. Every row says what proves it.
CREATE TABLE IF NOT EXISTS `server_class` (
    `hash` INT UNSIGNED NOT NULL,
    `name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `evidence` TEXT NOT NULL,
    PRIMARY KEY (`hash`),
    UNIQUE KEY `uk_server_class_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `server_class_base` (
    `class_hash` INT UNSIGNED NOT NULL,
    `position` TINYINT UNSIGNED NOT NULL,
    `base_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    PRIMARY KEY (`class_hash`, `position`),
    CONSTRAINT `fk_server_class_base_class` FOREIGN KEY (`class_hash`) REFERENCES `server_class` (`hash`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `server_class_property` (
    `class_hash` INT UNSIGNED NOT NULL,
    `property_id` INT UNSIGNED NOT NULL,
    `name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `type` VARCHAR(512) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `hash` INT UNSIGNED NOT NULL,
    `container` VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT 'Static',
    `offset` INT UNSIGNED NOT NULL DEFAULT 0,
    `flags` INT UNSIGNED NOT NULL DEFAULT 0,
    `dynamic` TINYINT(1) NOT NULL DEFAULT 0,
    `singleton` TINYINT(1) NOT NULL DEFAULT 0,
    `pointer` TINYINT(1) NOT NULL DEFAULT 0,
    PRIMARY KEY (`class_hash`, `property_id`),
    UNIQUE KEY `uk_server_class_property_name` (`class_hash`, `name`),
    CONSTRAINT `fk_server_class_property_class` FOREIGN KEY (`class_hash`) REFERENCES `server_class` (`hash`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `core_object_type` (
    `block` TINYINT UNSIGNED NOT NULL,
    `type` TINYINT UNSIGNED NOT NULL,
    `class_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `evidence` TEXT NOT NULL,
    PRIMARY KEY (`block`, `type`),
    UNIQUE KEY `uk_core_object_type_class` (`class_name`),
    CONSTRAINT `chk_core_object_type_not_plain` CHECK (`block` <> 0 OR `type` <> 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `behavior_client_class` (
    `behavior_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `class_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NULL,
    `evidence` TEXT NOT NULL,
    PRIMARY KEY (`behavior_name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `server_class` (`hash`, `name`, `evidence`) VALUES
    (1616662572, 'BasicMobileBehavior', 'The player object the r806919 client accepts in MSG_LOGINCOMPLETE.Data holds it in the BasicMobileBehavior slot of m_inactiveBehaviors, with no bytes of its own after its CoreObject header because its one property is not transmitted. The client program registers it under the bare name BasicMobileBehavior, which hashes to 1616662572, while class BasicMobileBehavior hashes to 586180150, which is why a dump that reads class names never found it. Its property is the one BehaviorInstance declares, where the dump gives its id, offset and flags.')
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `evidence` = VALUES(`evidence`);

INSERT INTO `server_class_base` (`class_hash`, `position`, `base_name`) VALUES
    (1616662572, 0, 'class BehaviorInstance'),
    (1616662572, 1, 'class PropertyClass')
ON DUPLICATE KEY UPDATE `base_name` = VALUES(`base_name`);

INSERT INTO `server_class_property` (`class_hash`, `property_id`, `name`, `type`, `hash`, `container`, `offset`, `flags`, `dynamic`, `singleton`, `pointer`) VALUES
    (1616662572, 0, 'm_behaviorTemplateNameID', 'unsigned int', 223437287, 'Static', 104, 39, 0, 0, 0)
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `type` = VALUES(`type`), `hash` = VALUES(`hash`), `container` = VALUES(`container`), `offset` = VALUES(`offset`), `flags` = VALUES(`flags`), `dynamic` = VALUES(`dynamic`), `singleton` = VALUES(`singleton`), `pointer` = VALUES(`pointer`);

INSERT INTO `core_object_type` (`block`, `type`, `class_name`, `evidence`) VALUES
    (104, 2, 'class WizClientObject', 'Every MSG_LOGINCOMPLETE.Data the r806919 client accepted opens 68 02 01000000, block 104, type 2 and template 1, the PlayerObject template, and decodes whole as a WizClientObject.'),
    (115, 9, 'class WizClientObjectItem', 'In the same player object, every entry of ClientWizEquipmentBehavior.m_itemList opens with block 115, type 9 and the item''s template id, and decodes whole as a WizClientObjectItem.')
ON DUPLICATE KEY UPDATE `class_name` = VALUES(`class_name`), `evidence` = VALUES(`evidence`);

INSERT INTO `behavior_client_class` (`behavior_name`, `class_name`, `evidence`) VALUES
    ('WizardEquipmentBehavior', 'class ClientWizEquipmentBehavior', 'Slot 0 of the 39 behaviors in the client''s ObjectData/PlayerObject.xml; slot 0 of m_inactiveBehaviors in a player object the r806919 client accepted holds this class.'),
    ('WizardInventoryBehavior', 'class ClientWizInventoryBehavior', 'Slot 1 of PlayerObject.xml''s behaviors; slot 1 of the accepted player object holds this class.'),
    ('BasicEffectsBehavior', 'class BaseGameEffectBehavior', 'Slot 2 of PlayerObject.xml''s behaviors; slot 2 of the accepted player object holds this class.'),
    ('BasicMobileBehavior', 'BasicMobileBehavior', 'Slot 3 of PlayerObject.xml''s behaviors; slot 3 of the accepted player object holds this class, which server_class describes.'),
    ('AnimationBehavior', 'class AnimationBehavior', 'Slot 4 of PlayerObject.xml''s behaviors; slot 4 of the accepted player object holds this class.'),
    ('BasicObjectStateBehavior', 'class ObjectStateBehavior', 'Slot 5 of PlayerObject.xml''s behaviors; slot 5 of the accepted player object holds this class.'),
    ('BasicMagicSchoolBehavior', 'class ClientMagicSchoolBehavior', 'Slot 6 of PlayerObject.xml''s behaviors; slot 6 of the accepted player object holds this class.'),
    ('PathMovementBehavior', NULL, 'Slot 7 of PlayerObject.xml''s behaviors; slot 7 of the accepted player object is null, so the client takes a player without it.'),
    ('BasicSpellbookBehavior', 'class ClientSpellbookBehavior', 'Slot 8 of PlayerObject.xml''s behaviors; slot 8 of the accepted player object holds this class.'),
    ('BasicTreasureBookBehavior', 'class ClientTreasureBookBehavior', 'Slot 9 of PlayerObject.xml''s behaviors; slot 9 of the accepted player object holds this class.'),
    ('WizardCharacterBehavior', 'class WizardCharacterBehavior', 'Slot 10 of PlayerObject.xml''s behaviors; slot 10 of the accepted player object holds this class.'),
    ('WizPlayerNameBehavior', 'class ClientWizPlayerNameBehavior', 'Slot 11 of PlayerObject.xml''s behaviors; slot 11 of the accepted player object holds this class.'),
    ('WizardStorageBehavior', 'class ClientWizStorageBehavior', 'Slot 12 of PlayerObject.xml''s behaviors; slot 12 of the accepted player object holds this class.'),
    ('WizardMinigameBehavior', 'class ClientMinigameBehavior', 'Slot 13 of PlayerObject.xml''s behaviors; slot 13 of the accepted player object holds this class.'),
    ('DynaModBehavior', 'class ClientDynaModBehavior', 'Slot 14 of PlayerObject.xml''s behaviors; slot 14 of the accepted player object holds this class.'),
    ('CollisionBehavior', 'class CollisionBehaviorClient', 'Slot 15 of PlayerObject.xml''s behaviors; slot 15 of the accepted player object holds this class.'),
    ('PetOwnerBehavior', 'class ClientPetOwnerBehavior', 'Slot 16 of PlayerObject.xml''s behaviors; slot 16 of the accepted player object holds this class.'),
    ('FidgetBehavior', 'class FidgetBehavior', 'Slot 17 of PlayerObject.xml''s behaviors; slot 17 of the accepted player object holds this class.'),
    ('EffectsBehavior', 'class EffectsBehavior', 'Slot 18 of PlayerObject.xml''s behaviors; slot 18 of the accepted player object holds this class.'),
    ('AtticBehavior', 'class ClientAtticBehavior', 'Slot 19 of PlayerObject.xml''s behaviors; slot 19 of the accepted player object holds this class.'),
    ('AlchemyBehavior', 'class ClientAlchemyBehavior', 'Slot 20 of PlayerObject.xml''s behaviors; slot 20 of the accepted player object holds this class.'),
    ('MountOwnerBehavior', 'class ClientMountOwnerBehavior', 'Slot 21 of PlayerObject.xml''s behaviors; slot 21 of the accepted player object holds this class.'),
    ('PetSnackBehavior', 'class ClientPetSnackBehavior', 'Slot 22 of PlayerObject.xml''s behaviors; slot 22 of the accepted player object holds this class.'),
    ('LadderBehavior', 'class LadderBehavior', 'Slot 23 of PlayerObject.xml''s behaviors; slot 23 of the accepted player object holds this class.'),
    ('LeashBehavior', NULL, 'Slot 24 of PlayerObject.xml''s behaviors; slot 24 of the accepted player object is null, so the client takes a player without it.'),
    ('MountRiderBehavior', 'class ClientMountRiderBehavior', 'Slot 25 of PlayerObject.xml''s behaviors; slot 25 of the accepted player object holds this class.'),
    ('FishingBehavior', 'class FishingBehavior', 'Slot 26 of PlayerObject.xml''s behaviors; slot 26 of the accepted player object holds this class.'),
    ('MonsterMagicBehavior', 'class MonsterMagicBehavior', 'Slot 27 of PlayerObject.xml''s behaviors; slot 27 of the accepted player object holds this class.'),
    ('TutorialLogBehavior', 'class TutorialLogBehavior', 'Slot 28 of PlayerObject.xml''s behaviors; slot 28 of the accepted player object holds this class.'),
    ('PetTomeBehavior', 'class PetTomeBehavior', 'Slot 29 of PlayerObject.xml''s behaviors; slot 29 of the accepted player object holds this class.'),
    ('ExpansionBehavior', 'class ClientExpansionBehavior', 'Slot 30 of PlayerObject.xml''s behaviors; slot 30 of the accepted player object holds this class.'),
    ('InfractionBehavior', NULL, 'Slot 31 of PlayerObject.xml''s behaviors; slot 31 of the accepted player object is null, so the client takes a player without it.'),
    ('CastleToursFavoritesBehavior', 'class CastleToursFavoritesBehavior', 'Slot 32 of PlayerObject.xml''s behaviors; slot 32 of the accepted player object holds this class.'),
    ('WishlistBehavior', 'class WishlistBehavior', 'Slot 33 of PlayerObject.xml''s behaviors; slot 33 of the accepted player object holds this class.'),
    ('HiddenQuestsBehavior', 'class HiddenQuestsBehavior', 'Slot 34 of PlayerObject.xml''s behaviors; slot 34 of the accepted player object holds this class.'),
    ('AdvPvPEloBehavior', NULL, 'Slot 35 of PlayerObject.xml''s behaviors; slot 35 of the accepted player object is null, so the client takes a player without it.'),
    ('ZoneTokenBehavior', NULL, 'Slot 36 of PlayerObject.xml''s behaviors; slot 36 of the accepted player object is null, so the client takes a player without it.'),
    ('BadgeBehavior', NULL, 'Slot 37 of PlayerObject.xml''s behaviors; slot 37 of the accepted player object is null, so the client takes a player without it.'),
    ('EmotesRadialMenuBehavior', NULL, 'Slot 38 of PlayerObject.xml''s behaviors; slot 38 of the accepted player object is null, so the client takes a player without it.')
ON DUPLICATE KEY UPDATE `class_name` = VALUES(`class_name`), `evidence` = VALUES(`evidence`);
