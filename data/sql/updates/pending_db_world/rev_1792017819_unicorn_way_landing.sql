-- Project Ambrose by Imjustchico
-- Sends a wizard who accepts Unicorn Way to WC_Unicorn's "Target location Landing" instead of its Start by the Commons gate.
UPDATE `quest_result` SET `data` = 0x7D20A30D780200005001000079944A9B200057697A617264436974792F57435F537472656574732F57435F556E69636F726E080100003B53465B1700546172676574206C6F636174696F6E204C616E64696E67
WHERE `quest` = 'WC-COMMONS-MAIN-001' AND `on_start` = 1 AND `class_name` = 'class ResTeleport';
