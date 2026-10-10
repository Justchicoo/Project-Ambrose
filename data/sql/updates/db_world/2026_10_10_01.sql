-- Project Ambrose by Imjustchico
-- Adds the behavior_client_class row the street monsters' templates need, such as Unicorn Way's ghosts and pirate skeletons, which name MobMonsterMagicBehavior and were otherwise left out of every zone.
INSERT INTO `behavior_client_class` (`behavior_name`, `class_name`, `evidence`) VALUES
    ('MobMonsterMagicBehavior', 'class MobMonsterMagicBehavior', 'The r806919 client program holds the string MobMonsterMagicBehavior and the RTTI names of class MobMonsterMagicBehavior and class MobMonsterMagicBehaviorTemplate, and the type dump lists class MobMonsterMagicBehavior on BehaviorInstance; the factory call itself was not traced.')
ON DUPLICATE KEY UPDATE `class_name` = VALUES(`class_name`), `evidence` = VALUES(`evidence`);
