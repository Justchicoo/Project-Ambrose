/*
 * Project Ambrose by Imjustchico
 * Where each zone's doors lead, from zone_teleport: a trigger whose results hold a ResTeleport sends the wizard who fires it to the row's destination zone and location, read at start and again by `.reload zone_teleport`, which builds the new set off to the side, refuses a row naming a zone or location the zones do not hold or whose same_zone flag disagrees with its zones, and swaps it in only when every row is good, keeping the old set and reporting each error otherwise; and the one-transfer rule, which takes the first door in data order that has a destination when several fire on one event.
 */

#ifndef AMBROSE_ZONETELEPORTMGR_H
#define AMBROSE_ZONETELEPORTMGR_H

#include "Types.h"

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct ZoneTeleport
{
    std::string Zone;
    std::string TriggerName;
    std::string DestZone;
    std::string DestLocation;
    uint32 TransitionId = 0;
    bool SameZone = false;
};

class ZoneTeleportMgr
{
public:
    static constexpr std::string_view ReloadTarget = "zone_teleport";

    using Table = std::map<std::pair<std::string, std::string>, ZoneTeleport, std::less<>>;

    static ZoneTeleportMgr& Instance();

    ZoneTeleportMgr(ZoneTeleportMgr const&) = delete;
    ZoneTeleportMgr& operator=(ZoneTeleportMgr const&) = delete;

    static std::optional<ZoneTeleport> FirstWithDestination(Table const& table, std::string_view zone, std::vector<std::string> const& doors);

    bool Load(std::vector<std::string>& errors);
    void RegisterReloadTargets();
    void Replace(Table table);
    void Clear();

    std::optional<ZoneTeleport> Find(std::string_view zone, std::string_view trigger) const;
    std::optional<ZoneTeleport> FirstWithDestination(std::string_view zone, std::vector<std::string> const& doors) const;
    std::vector<ZoneTeleport> InZone(std::string_view zone) const;

private:
    ZoneTeleportMgr() = default;

    mutable std::mutex _mutex;
    std::shared_ptr<Table const> _table = std::make_shared<Table const>();
};

#define sZoneTeleportMgr ZoneTeleportMgr::Instance()

#endif
