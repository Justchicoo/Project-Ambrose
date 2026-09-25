/*
 * Project Ambrose by Imjustchico
 * The zone group: 'zone info <path>' says what this server holds about one zone, its display name key and how many named places and placed objects are in it, and 'zone place <path> [name]' says where a wizard asking for that place would be put, naming the fall back to Start when the place it asked for is not there, because an operator checking a door needs to know which of the two answered. Neither reads the world database: both read the stores the reload targets fill, so what they print is what a player would be given at that moment.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "ScriptMgr.h"
#include "ZoneMgr.h"

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
            caller.Reply(fmt::format("{} in {} is {} at {:.3f}, {:.3f}, {:.3f}, {}", wanted, arguments[0],
                ZoneMgr::GetLookupName(place.Result), place.Location.X, place.Location.Y, place.Location.Z,
                place.Location.Yaw ? fmt::format("facing {:.3f}", *place.Location.Yaw) : std::string("facing nothing it says")));
            return true;
        }
    };
}

void AddSC_cs_zone()
{
    new ZoneCommands();
}
