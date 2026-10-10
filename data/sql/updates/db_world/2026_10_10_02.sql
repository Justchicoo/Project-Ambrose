-- Project Ambrose by Imjustchico
-- Adds WC-MAIN-C01-001, the enrollment quest Merle Ambrose (template 38168, WC-HUB-NPC01, placed only in WizardCity/Interiors/WC_Headmistress_House) gives a new wizard: Ravenwood's and every school's triggers require ReqHasQuest WC-MAIN-C01-001 (WC_Ravenwood triggers.xml Stop_Music_EnrollmentQuest, SchoolFire Stinger_Fire), its title is QuestTitle_1625D7 and its lines WizQst1625D7, the Prep lines 1 and 2 sending the wizard to Ravenwood to talk to Mr. Lincoln, WC-GTW-Registrar (39088), placed in WizardCity/WC_Ravenwood, whose name ZoneLocName_1451497 gives the goal's location. The install holds no quest template, so the level is authored as 1, for a wizard's first quest, the goal is named here, and its rewards come with the loot tables.
INSERT INTO `quest_template` (`name`, `title_key`, `info_key`, `level`, `mainline`) VALUES
    ('WC-MAIN-C01-001', 'QuestTitle_1625D7', 'WizQst1625D7_00000000', 1, 1);

INSERT INTO `quest_goal` (`quest`, `goal_name`, `type`, `title_key`, `location_key`, `destination_zone`, `persona_name`) VALUES
    ('WC-MAIN-C01-001', 'WC-MAIN-C01-001_Goal0', 'persona', 'WizardQuestGoals_TalkNPC', 'ZoneLocName_1451497', 'WizardCity/WC_Ravenwood', 'WC-GTW-Registrar');

INSERT INTO `quest_start_goal` (`quest`, `goal_name`) VALUES
    ('WC-MAIN-C01-001', 'WC-MAIN-C01-001_Goal0');

INSERT INTO `quest_dialog` (`quest`, `owner`, `goal_name`, `tag`) VALUES
    ('WC-MAIN-C01-001', 'quest', '', 'Prep');

INSERT INTO `quest_dialog_entry` (`quest`, `owner`, `goal_name`, `tag`, `position`, `actor_template_id`, `dialog_key`) VALUES
    ('WC-MAIN-C01-001', 'quest', '', 'Prep', 0, 38168, 'WizQst1625D7_00000001'),
    ('WC-MAIN-C01-001', 'quest', '', 'Prep', 1, 38168, 'WizQst1625D7_00000002');
