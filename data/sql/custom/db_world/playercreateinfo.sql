-- Project Ambrose by Imjustchico
-- The starting state every new wizard is given on this server, as one row standing for every school: Ravenwood in Wizard City, which is where phase 4 stands a wizard up, at level 1 with no experience. Change the row to move where wizards begin, or add a row of a school's own to start that school somewhere else; the tutorial the retail game starts in belongs to a milestone that is not built, so nothing here points at it.
INSERT INTO `playercreateinfo` (`school_id`, `world`, `zone`, `zone_display`, `position_x`, `position_y`, `position_z`, `orientation`, `level`, `experience`)
VALUES (0, 0, 'WizardCity/WC_Ravenwood', 'Ravenwood', 0, 0, 0, 0, 1, 0)
ON DUPLICATE KEY UPDATE `zone` = VALUES(`zone`);
