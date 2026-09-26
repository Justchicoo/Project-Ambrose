-- Project Ambrose by Imjustchico
-- Keys core_object_type by the core type alone and adds core_template_type. A game object's CoreObject header is its core type, its template's type and its template id; the client picks the class to build from the core type its template gives, through the factory GameClient::InitializeCoreObjects and WizardClientModules::InitWizardCoreObjects fill with one creator per core type, so one class may stand behind several core types and the header's second byte is the template's, not the class's. core_template_type says which core type and template type each template class gives, as the client's own templates answer through their virtual CoreType and TemplateType.
DROP TABLE IF EXISTS `core_object_type`;

CREATE TABLE `core_object_type` (
    `core_type` TINYINT UNSIGNED NOT NULL,
    `class_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `evidence` TEXT NOT NULL,
    PRIMARY KEY (`core_type`),
    CONSTRAINT `chk_core_object_type_not_plain` CHECK (`core_type` <> 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `core_template_type` (
    `template_class` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `core_type` TINYINT UNSIGNED NOT NULL,
    `template_type` TINYINT UNSIGNED NOT NULL,
    `evidence` TEXT NOT NULL,
    PRIMARY KEY (`template_class`),
    CONSTRAINT `fk_core_template_type_core` FOREIGN KEY (`core_type`) REFERENCES `core_object_type` (`core_type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `core_object_type` (`core_type`, `class_name`, `evidence`) VALUES
    (2, 'class ClientObject', 'r806919 GameClient::InitializeCoreObjects registers core types 2, 5 and 9 through one helper whose creator builds 0x230 bytes with the ClientObject constructor, the one that stores ClientObject''s virtual table.'),
    (5, 'class ClientObject', 'Registered with core types 2 and 9 by the same GameClient::InitializeCoreObjects helper and the same ClientObject creator.'),
    (9, 'class ClientObject', 'Registered with core types 2 and 5 by the same GameClient::InitializeCoreObjects helper and the same ClientObject creator; ItemTemplate gives core type 9.'),
    (104, 'class WizClientObject', 'WizardClientModules::InitWizardCoreObjects registers core type 104 with a creator whose object''s GetType registers WizClientObject; every MSG_LOGINCOMPLETE.Data the r806919 client accepted opens 68 02 01000000, core type 104, template type 2 and template 1, and decodes whole as a WizClientObject.'),
    (115, 'class WizClientObjectItem', 'WizardClientModules::InitWizardCoreObjects registers core type 115; every entry of ClientWizEquipmentBehavior.m_itemList in an accepted player object opens with core type 115, template type 9 and the item''s template id, and decodes whole as a WizClientObjectItem.')
ON DUPLICATE KEY UPDATE `class_name` = VALUES(`class_name`), `evidence` = VALUES(`evidence`);

INSERT INTO `core_template_type` (`template_class`, `core_type`, `template_type`, `evidence`) VALUES
    ('class GameObjectTemplate', 2, 2, 'An r806919 GameObjectTemplate built by its own type and asked through its virtual table answers core type 2 at slot 14 and template type 2 at slot 16, which CoreObjectFactory::CreateInstanceFromInfo and SerializerCoreObjects::PreLoadObject read.'),
    ('class WizGameObjectTemplate', 104, 2, 'An r806919 WizGameObjectTemplate answers core type 104 at slot 14 and template type 2 at slot 16; the player template, PlayerObject.xml, is one, and its object travels as 68 02.'),
    ('class ItemTemplate', 9, 9, 'An r806919 ItemTemplate answers core type 9 at slot 14 and template type 9 at slot 16.'),
    ('class WizItemTemplate', 115, 9, 'An r806919 WizItemTemplate answers core type 115 at slot 14 and template type 9 at slot 16, which is the header every equipped item in an accepted player object carries.')
ON DUPLICATE KEY UPDATE `core_type` = VALUES(`core_type`), `template_type` = VALUES(`template_type`), `evidence` = VALUES(`evidence`);
