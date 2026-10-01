-- Project Ambrose by Imjustchico
-- Teaches the player-object builder to send the client its EmotesRadialMenuBehavior instead of leaving the radial menu slot empty.
UPDATE `behavior_client_class`
SET `class_name` = 'class EmotesRadialMenuBehavior',
    `evidence` = 'The installed client registers EmotesRadialMenuBehavior as class EmotesRadialMenuBehavior, whose three radial page blobs are parsed on load.'
WHERE `behavior_name` = 'EmotesRadialMenuBehavior';
