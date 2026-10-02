-- Project Ambrose by Imjustchico
-- Adds zone_client_event, the events a wizard's client may post into its zone's triggers with MSG_POSTZONEEVENTFROMCLIENT, zone by zone; an event a zone does not list is refused, so a client cannot claim it walked into a volume or arrived.
CREATE TABLE IF NOT EXISTS `zone_client_event` (
    `zone_path` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `event_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    PRIMARY KEY (`zone_path`, `event_name`),
    CONSTRAINT `fk_zone_client_event_template` FOREIGN KEY (`zone_path`) REFERENCES `zone_template` (`zone_path`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
