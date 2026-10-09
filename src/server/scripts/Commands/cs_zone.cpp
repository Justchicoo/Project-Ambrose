/*
 * Project Ambrose by Imjustchico
 * The zone group: 'zone info <path>' says what this server holds about one zone, its display name key and how many named places and placed objects are in it, 'zone place <path> [name]' says where a wizard asking for that place would be put, naming the fall back to Start when the place it asked for is not there, because an operator checking a door needs to know which of the two answered, and 'zone teleports <path>' lists each door, a trigger holding a ResTeleport, with where zone_teleport sends it, flagging a door with no destination and one behind requirements. Neither reads the world database: both read the stores the reload targets fill, so what they print is what a player would be given at that moment.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "ScriptMgr.h"
#include "ZoneMgr.h"
#include "ZoneTeleportMgr.h"
#include "ZoneTriggerMgr.h"

#include <fmt/format.h>

#include <optional>
#include <string>
#include <vector>

namespace
{
    class ZoneCommands : public CommandScript
    {
    public:
        ZoneCommands() : CommandScript("cs_zone") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "zone", .SecurityLevel = SEC_GAMEMASTER, .Help = "the zones this server has loaded", .Children = {
                    { .Name = "info", .SecurityLevel = SEC_GAMEMASTER, .Help = "what this server holds about one zone", .Run = Info },
                    { .Name = "place", .SecurityLevel = SEC_GAMEMASTER, .Help = "where a wizard asking for a place in a zone would be put", .Run = Place },
                    { .Name = "teleports", .SecurityLevel = SEC_GAMEMASTER, .Help = "each door in a zone and where it leads, flagging one with no destination: <zone path>", .Run = Teleports },
                } },
            };
        }

    private:
        static bool Info(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.empty() || arguments[0].empty())
            {
                caller.Reply("zone info takes the path of a zone, such as WizardCity/WC_Hub");
                return false;
            }
            std::optional<std::string> const described = sZoneMgr.Describe(arguments[0]);
            if (!described)
            {
                caller.Reply(fmt::format("No zone {} is loaded; {} zone(s) are", arguments[0], sZoneMgr.GetTemplates()->Count()));
                return false;
            }
            caller.Reply(*described);
            return true;
        }

        static bool Place(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.empty() || arguments[0].empty())
            {
                caller.Reply("zone place takes the path of a zone and, after it, the name of a place in it");
                return false;
            }
            std::string const wanted = arguments.size() > 1 ? arguments[1] : std::string(ZoneLocations::StartName);
            ZonePlace const place = sZoneMgr.FindPlace(arguments[0], wanted);
            if (!place.Found())
            {
                caller.Reply(fmt::format("{} in {} gives {}", wanted, arguments[0], ZoneMgr::GetLookupName(place.Result)));
                return false;
            }
            caller.Reply(fmt::format("{} in {} is {} at {:.3f}, {:.3f}, {:.3f}, facing {:.3f}", wanted, arguments[0],
                ZoneMgr::GetLookupName(place.Result), place.Location.X, place.Location.Y, place.Location.Z, place.Location.Yaw));
            return true;
        }

        static bool Teleports(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.empty() || arguments[0].empty())
            {
                caller.Reply("zone teleports takes the path of a zone, such as WizardCity/WC_Hub");
                return false;
            }
            std::shared_ptr<ZoneTriggerData const> const data = sZoneTriggerMgr.Find(arguments[0]);
            if (!data)
            {
                caller.Reply(fmt::format("{} has no triggers loaded", arguments[0]));
                return false;
            }
            std::size_t doors = 0;
            std::size_t missing = 0;
            for (ZoneTrigger const& trigger : data->Triggers)
            {
                if (!trigger.Teleports)
                    continue;
                ++doors;
                std::optional<ZoneTeleport> const door = sZoneTeleportMgr.Find(arguments[0], trigger.Name);
                if (!door)
                    ++missing;
                caller.Reply(door ? fmt::format("{} '{}' leads to {} in {}{}{}", trigger.Index, trigger.Name, door->DestLocation, door->DestZone,
                                        door->SameZone ? ", within the zone" : "", trigger.HasRequirements ? ", behind requirements" : "")
                                  : fmt::format("{} '{}' has NO DESTINATION in zone_teleport{}", trigger.Index, trigger.Name, trigger.HasRequirements ? ", behind requirements" : ""));
            }
            caller.Reply(fmt::format("{} has {} door(s), {} with no destination", arguments[0], doors, missing));
            return true;
        }
    };
}

void AddSC_cs_zone()
{
    new ZoneCommands();
}
