/*
 * Project Ambrose by Imjustchico
 * Every zone's volumes and triggers from zone_volume, zone_trigger, zone_trigger_event and the ResClientNotifyText results in zone_trigger_result, read at start and again by `.reload zone_trigger`, which builds the new set off to the side, validates it and swaps it in only when every row is good, keeping the old set and reporting each error otherwise; and each zone instance's trigger state, so a wizard entering a volume posts the volume's enter events, and EnterZone when it arrives, into its own instance's triggers, and its client may post only the events zone_client_event lists for the zone.
 */

#ifndef AMBROSE_ZONETRIGGERMGR_H
#define AMBROSE_ZONETRIGGERMGR_H

#include "TypeRegistry.h"
#include "ZoneTriggers.h"
#include "ZoneVolume.h"

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct ZoneTriggerData
{
    std::vector<ZoneVolume> Volumes;
    std::map<uint32, std::vector<std::string>> EnterEvents;
    std::map<uint32, std::vector<std::string>> ExitEvents;
    std::vector<ZoneTrigger> Triggers;
    std::set<std::string, std::less<>> ClientEvents;
};

class ZoneTriggerMgr
{
public:
    static constexpr std::string_view ReloadTarget = "zone_trigger";
    static constexpr std::string_view EnterZoneEvent = "EnterZone";

    static constexpr std::string_view NotifyTextClass = "class ResClientNotifyText";
    static constexpr std::string_view TeleportClass = "class ResTeleport";

    static ZoneTriggerMgr& Instance();
    static std::optional<ZoneNotifyText> ReadNotifyText(TypeCatalogPtr const& catalog, std::span<uint8 const> data, std::string& error);

    ZoneTriggerMgr(ZoneTriggerMgr const&) = delete;
    ZoneTriggerMgr& operator=(ZoneTriggerMgr const&) = delete;

    bool Load(std::vector<std::string>& errors);
    void RegisterReloadTargets();
    void Replace(std::map<std::string, ZoneTriggerData, std::less<>> zones);
    void Clear();

    std::shared_ptr<ZoneTriggerData const> Find(std::string_view zone) const;
    std::vector<std::string> Post(uint32 mapId, std::string_view zone, std::string_view event, uint64 wizard, ZoneTriggers::Clock::time_point now,
        std::vector<ZoneNotifyText>* texts = nullptr, std::vector<std::string>* doors = nullptr, bool doorsIgnoreRequirements = false);
    void ForgetMap(uint32 mapId);

private:
    ZoneTriggerMgr() = default;

    mutable std::mutex _mutex;
    std::map<std::string, std::shared_ptr<ZoneTriggerData const>, std::less<>> _zones;
    std::map<uint32, ZoneTriggers> _instances;
};

#define sZoneTriggerMgr ZoneTriggerMgr::Instance()

#endif
