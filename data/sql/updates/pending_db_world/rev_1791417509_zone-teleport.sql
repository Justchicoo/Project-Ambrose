-- Project Ambrose by Imjustchico
-- The hand-reviewed doors of the Wizard City starting area in zone_teleport, by client identifiers only: the Commons to and from Ravenwood, the Shopping District, Golem Court, the Library, Unicorn Way and the Headmistress House.
DELETE FROM `zone_teleport` WHERE `zone` IN ('WizardCity/WC_Hub', 'WizardCity/WC_Ravenwood', 'WizardCity/WC_Shop_Area', 'WizardCity/WC_Golem_Tower', 'WizardCity/Interiors/WC_Library',
    'WizardCity/WC_Streets/WC_Unicorn', 'WizardCity/Interiors/WC_Headmistress_House');
INSERT INTO `zone_teleport` (`zone`, `trigger_name`, `dest_zone`, `dest_location`, `transition_id`, `same_zone`) VALUES
    ('WizardCity/WC_Hub', 'TeleportToRavenwoodTrigger', 'WizardCity/WC_Ravenwood', 'Target location (Ravenwood Hub Exit)', 0, 0),
    ('WizardCity/WC_Hub', 'TeleportToShoppingDistrict', 'WizardCity/WC_Shop_Area', 'Target location (WC_Shop Hub)', 0, 0),
    ('WizardCity/WC_Hub', 'TeleportToGolemCourt', 'WizardCity/WC_Golem_Tower', 'Target location (WC_Golem_Tower Hub Entrance)', 0, 0),
    ('WizardCity/WC_Hub', 'TeleportToLibraryTrigger', 'WizardCity/Interiors/WC_Library', 'Target location (Library Hub Entrance)', 0, 0),
    ('WizardCity/WC_Hub', 'Trigger Teleport - Street1', 'WizardCity/WC_Streets/WC_Unicorn', 'Target location (Hub Street1 Entrance)', 0, 0),
    ('WizardCity/WC_Hub', 'Teleport location (WC_Hub WC_Headmistress_House Entrance)', 'WizardCity/Interiors/WC_Headmistress_House', 'Target location (WC_Hub WC_Headmistress Entrance)', 0, 0),
    ('WizardCity/WC_Ravenwood', 'Teleport location (to Commons)', 'WizardCity/WC_Hub', 'Target location(WC_Hub Ravenwood)', 0, 0),
    ('WizardCity/WC_Shop_Area', 'Teleport location (WC_Shop Hub)', 'WizardCity/WC_Hub', 'Target location (WC_Hub Shops)', 0, 0),
    ('WizardCity/WC_Golem_Tower', 'Teleport location (WC_Golem_Tower Hub Exit)', 'WizardCity/WC_Hub', 'Target location (WC_Hub WC_Golem_Tower Exit)', 0, 0),
    ('WizardCity/Interiors/WC_Library', 'Teleport location (Library Hub Exit)', 'WizardCity/WC_Hub', 'Target location (WC_Hub Library)', 0, 0),
    ('WizardCity/WC_Streets/WC_Unicorn', 'Teleport location (Street1 Hub Exit)', 'WizardCity/WC_Hub', 'Target location (WC_Hub Street1 Exit)', 0, 0),
    ('WizardCity/Interiors/WC_Headmistress_House', 'Trigger Teleport Outside', 'WizardCity/WC_Hub', 'Target location (WC_Headmistress_House WC_Hub Exit)', 0, 0);
