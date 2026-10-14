-- Project Ambrose by Imjustchico
-- Adds the Completion dialog of WC-MAIN-C01-001_Goal0: Mr. Lincoln, WC-GTW-Registrar (39088), writes the new wizard into the registry in the quest's lines WizQst1625D7 3 to 6, each with the portrait his template's m_sIcon names, GUI/NpcPortraits/Art_Portrait_Registrar.dds, so talking to him to finish the enrollment quest plays them.
INSERT INTO `quest_dialog` (`quest`, `owner`, `goal_name`, `tag`) VALUES
    ('WC-MAIN-C01-001', 'goal', 'WC-MAIN-C01-001_Goal0', 'Completion');

INSERT INTO `quest_dialog_entry` (`quest`, `owner`, `goal_name`, `tag`, `position`, `actor_template_id`, `dialog_key`, `picture`) VALUES
    ('WC-MAIN-C01-001', 'goal', 'WC-MAIN-C01-001_Goal0', 'Completion', 0, 39088, 'WizQst1625D7_00000003', 'GUI/NpcPortraits/Art_Portrait_Registrar.dds'),
    ('WC-MAIN-C01-001', 'goal', 'WC-MAIN-C01-001_Goal0', 'Completion', 1, 39088, 'WizQst1625D7_00000004', 'GUI/NpcPortraits/Art_Portrait_Registrar.dds'),
    ('WC-MAIN-C01-001', 'goal', 'WC-MAIN-C01-001_Goal0', 'Completion', 2, 39088, 'WizQst1625D7_00000005', 'GUI/NpcPortraits/Art_Portrait_Registrar.dds'),
    ('WC-MAIN-C01-001', 'goal', 'WC-MAIN-C01-001_Goal0', 'Completion', 3, 39088, 'WizQst1625D7_00000006', 'GUI/NpcPortraits/Art_Portrait_Registrar.dds');
