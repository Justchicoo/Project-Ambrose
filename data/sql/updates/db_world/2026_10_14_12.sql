-- Project Ambrose by Imjustchico
-- Adds WC-COMMONS-MAIN-001, Unicorn Way, the first quest Merle Ambrose (template 38168, WC-HUB-NPC01, in WizardCity/Interiors/WC_Headmistress_House) gives a new wizard, named as WC_Unicorn's DarknessTheme trigger names it, its title QuestTitle_1625BF and its lines WizQst1625BF: Prep lines 0 to 2 by Ambrose; on accepting, its start ResTeleport sends the wizard to the Start of WizardCity/WC_Streets/WC_Unicorn and its Start dialog, Ambrose's line 4 and Gamma's (87110) line 5 pointing at Private Connelly, plays where the wizard arrives; its one goal is to talk to Private Connelly, WC-ST01-NPC04 (38119), at ZoneLocName_595390, whose Completion lines are 7, 10 and 15. Each line shows its speaker's m_sIcon. The ResTeleport bytes are what `client encode` writes for a ResTeleport with that zone and location. WC-MAIN-C01-001, To Ravenwood!, now waits for Unicorn Way to be complete, the nearest of its wiki prequests that is authored, so Ambrose offers Unicorn Way first. The level is 1, for a wizard's first quest.
INSERT INTO `quest_template` (`name`, `title_key`, `info_key`, `level`, `mainline`) VALUES
    ('WC-COMMONS-MAIN-001', 'QuestTitle_1625BF', 'WizQst1625BF_00000001', 1, 1);

INSERT INTO `quest_goal` (`quest`, `goal_name`, `type`, `title_key`, `location_key`, `destination_zone`, `persona_name`) VALUES
    ('WC-COMMONS-MAIN-001', 'WC-COMMONS-MAIN-001_Goal0', 'persona', 'WizardQuestGoals_TalkNPC', 'ZoneLocName_595390', 'WizardCity/WC_Streets/WC_Unicorn', 'WC-ST01-NPC04');

INSERT INTO `quest_start_goal` (`quest`, `goal_name`) VALUES
    ('WC-COMMONS-MAIN-001', 'WC-COMMONS-MAIN-001_Goal0');

INSERT INTO `quest_result` (`quest`, `goal_name`, `on_start`, `position`, `class_name`, `data`) VALUES
    ('WC-COMMONS-MAIN-001', '', 1, 0, 'class ResTeleport', 0x7D20A30DE80100005001000079944A9B200057697A617264436974792F57435F537472656574732F57435F556E69636F726E780000003B53465B05005374617274);

INSERT INTO `quest_dialog` (`quest`, `owner`, `goal_name`, `tag`) VALUES
    ('WC-COMMONS-MAIN-001', 'quest', '', 'Prep'),
    ('WC-COMMONS-MAIN-001', 'quest', '', 'Start'),
    ('WC-COMMONS-MAIN-001', 'goal', 'WC-COMMONS-MAIN-001_Goal0', 'Completion');

INSERT INTO `quest_dialog_entry` (`quest`, `owner`, `goal_name`, `tag`, `position`, `actor_template_id`, `dialog_key`, `picture`) VALUES
    ('WC-COMMONS-MAIN-001', 'quest', '', 'Prep', 0, 38168, 'WizQst1625BF_00000000', 'GUI/NpcPortraits/Art_Portrait_Ambrose.dds'),
    ('WC-COMMONS-MAIN-001', 'quest', '', 'Prep', 1, 38168, 'WizQst1625BF_00000001', 'GUI/NpcPortraits/Art_Portrait_Ambrose.dds'),
    ('WC-COMMONS-MAIN-001', 'quest', '', 'Prep', 2, 38168, 'WizQst1625BF_00000002', 'GUI/NpcPortraits/Art_Portrait_Ambrose.dds'),
    ('WC-COMMONS-MAIN-001', 'quest', '', 'Start', 0, 38168, 'WizQst1625BF_00000004', 'GUI/NpcPortraits/Art_Portrait_Ambrose.dds'),
    ('WC-COMMONS-MAIN-001', 'quest', '', 'Start', 1, 87110, 'WizQst1625BF_00000005', 'GUI/NpcPortraits/Portrait_Owl_Gamma_A_NPC.dds'),
    ('WC-COMMONS-MAIN-001', 'goal', 'WC-COMMONS-MAIN-001_Goal0', 'Completion', 0, 38119, 'WizQst1625BF_00000007', 'GUI/NpcPortraits/Human_WCGuard_A.dds'),
    ('WC-COMMONS-MAIN-001', 'goal', 'WC-COMMONS-MAIN-001_Goal0', 'Completion', 1, 38119, 'WizQst1625BF_00000010', 'GUI/NpcPortraits/Human_WCGuard_A.dds'),
    ('WC-COMMONS-MAIN-001', 'goal', 'WC-COMMONS-MAIN-001_Goal0', 'Completion', 2, 38119, 'WizQst1625BF_00000015', 'GUI/NpcPortraits/Human_WCGuard_A.dds');

INSERT INTO `requirement_list` (`id`, `operator`, `apply_not`, `parent`) VALUES
    ('quest:WC-MAIN-C01-001', 'AND', 0, NULL);

INSERT INTO `requirement` (`list_id`, `position`, `type`, `quest_name`, `entry_name`, `is_quest_registry`) VALUES
    ('quest:WC-MAIN-C01-001', 0, 'ReqHasEntry', 'WC-COMMONS-MAIN-001', 'Complete', 1);

UPDATE `quest_template` SET `requirement_list_id` = 'quest:WC-MAIN-C01-001' WHERE `name` = 'WC-MAIN-C01-001';
