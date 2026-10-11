-- Project Ambrose by Imjustchico
-- Adds what a duel needs to place its circle: the core type a DynamicTriggerTemplate gives, the classes the client builds for the circle's DuelBehavior and CountdownBehavior slots, and zone_object.sigil_template, the sigil record a placed sigil uses, which the extractor reads from its m_templateName.
INSERT INTO `behavior_client_class` (`behavior_name`, `class_name`, `evidence`) VALUES
    ('DuelBehavior', 'class WizardClientDuelBehavior', 'The r806919 core registers class ClientDuelBehavior under DuelBehavior without replacing, then the Wizard client layer registers class WizardClientDuelBehavior under the same name with replace set, which deletes the core factory; the Duel Circle template names DuelBehavior in its fourth slot.'),
    ('CountdownBehavior', 'class ClientCountdownBehavior', 'The r806919 core registers class CountdownBehavior under CountdownBehavior without replacing, then the Wizard client layer registers class ClientCountdownBehavior under the same name with replace set; the Duel Circle template names CountdownBehavior in its second slot.')
ON DUPLICATE KEY UPDATE `class_name` = VALUES(`class_name`), `evidence` = VALUES(`evidence`);
ALTER TABLE `zone_object` ADD COLUMN `sigil_template` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '' AFTER `spawn_requirements`;
INSERT INTO `core_template_type` (`template_class`, `core_type`, `template_type`, `evidence`) VALUES
    ('class DynamicTriggerTemplate', 5, 2, 'An r806919 DynamicTriggerTemplate answers core type 5 at virtual table slot 14, a byte nothing else reads, and template type 2 at slot 16; the Duel Circle a duel places is one, and core type 5 builds class ClientObject.')
ON DUPLICATE KEY UPDATE `core_type` = VALUES(`core_type`), `template_type` = VALUES(`template_type`), `evidence` = VALUES(`evidence`);
