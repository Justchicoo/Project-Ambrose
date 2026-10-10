-- Project Ambrose by Imjustchico
-- Gives the player object the EmotesRadialMenuBehavior its template names in slot 38, which the client keeps the radial menu's emote and chat-phrase slots in, so a wizard's slots have a behavior to load into.
INSERT INTO `behavior_client_class` (`behavior_name`, `class_name`, `evidence`) VALUES
    ('EmotesRadialMenuBehavior', 'class EmotesRadialMenuBehavior', 'The type dump lists class EmotesRadialMenuBehavior on BehaviorInstance with m_radialMenuData, m_radialMenu2Data and m_radialMenu3Data, and the r806919 client program''s EmotesRadialMenuBehavior::LoadRadialMenuData (0x141e77b00) reads each as a blob of slots behind the magic 0xFACECEE8, skipping one that is empty.')
ON DUPLICATE KEY UPDATE `class_name` = VALUES(`class_name`), `evidence` = VALUES(`evidence`);
