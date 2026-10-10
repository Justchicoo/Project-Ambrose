/*
 * Project Ambrose by Imjustchico
 * Teleports within a zone. '.tele <place> [wizard]' moves the caller's wizard, or a wizard named by character id or name, to one of its zone's own locations, such as Start, or to a game_tele point in that zone, and the wizard and everyone who sees it are sent MSG_SERVERTELEPORT, so it snaps there with no loading screen; '.go xyz <x> <y> <z> [yaw]' moves the caller's wizard to coordinates, keeping its facing unless a yaw is given, and refuses ones a position cannot be sent as; '.gps' says the caller's zone, place, facing and zone instance; '.tele add <name>' keeps the caller's place as a game_tele point that works at once, unless the name is already a location of the caller's zone, and '.tele del <name>' removes one. '.tele npc <name or id> [wizard] [distance]' stands a wizard 120 units, or the distance given, from the nearest thing placed in its zone that answers to that override name, zone tag, template name, template id or global id, on the side the wizard stood, facing it, so a test can open an NPC's menu without walking there; '.tele zone <zone path> [location] [wizard]' sends a wizard to another zone the way the client changes zones, through MSG_ZONETRANSFERREQUEST, its acknowledgement and MSG_SERVERTRANSFER, and refuses a zone path or location the server does not hold before anything is sent, so the wizard stays. The move runs on the world thread, where positions are kept, and is answered once it has.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "GameSession.h"
#include "GameTeleMgr.h"
#include "NpcApproach.h"
#include "ObjectTemplateMgr.h"
#include "ScriptMgr.h"
#include "StringUtil.h"
#include "World.h"
#include "ZoneMgr.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    std::shared_ptr<GameSession> CallerWizard(CommandCaller& caller)
    {
        if (caller.GetCharacterId() == 0)
        {
            caller.Reply("This command moves your own wizard, so it is typed in game");
            return nullptr;
        }
        std::shared_ptr<GameSession> const session = sWorld.FindSessionByCharacterId(caller.GetCharacterId());
        if (!session)
            caller.Reply("Your wizard is not in the world");
        return session;
    }

    std::shared_ptr<GameSession> NamedWizard(CommandCaller& caller, std::string const& name)
    {
        std::vector<std::shared_ptr<GameSession>> const found = sWorld.FindInWorld(name);
        if (found.size() == 1)
            return found.front();
        if (found.empty())
            caller.Reply(fmt::format("No wizard in the world has the character id or name {}", name));
        else
            caller.Reply(fmt::format("{} wizards in the world are named {}; name one by its character id", found.size(), name));
        return nullptr;
    }

    struct Moved
    {
        bool Done = false;
        std::string Zone;
        std::string Problem;
    };

    bool Move(CommandCaller& caller, std::shared_ptr<GameSession> const& wizard, std::optional<PlayerPosition> place, std::string const& placeName, bool keepFacing = false)
    {
        auto const moved = std::make_shared<Moved>();
        bool const ran = sWorld.RunFor(wizard, [moved, place, placeName, keepFacing](GameSession& session)
        {
            moved->Zone = session.GetZonePath();
            std::optional<PlayerPosition> target = place;
            if (target && keepFacing)
                target->Yaw = session.GetMovement().GetPosition().Yaw;
            if (!target)
            {
                std::shared_ptr<ZoneLocations const> const locations = sZoneMgr.GetLocations();
                ZonePlace const location = locations ? locations->Find(session.GetZonePath(), placeName) : ZonePlace{};
                if (location.Result == ZoneLookup::Ok)
                    target = PlayerPosition{ location.Location.X, location.Location.Y, location.Location.Z, location.Location.Yaw };
                else if (std::optional<GameTele> const tele = sGameTeleMgr.Find(placeName))
                {
                    if (tele->Zone != session.GetZonePath())
                    {
                        moved->Problem = fmt::format("{} is in {}, and .tele moves a wizard within its own zone, {}", tele->Name, tele->Zone, session.GetZonePath());
                        return;
                    }
                    target = PlayerPosition{ tele->X, tele->Y, tele->Z, tele->Yaw };
                }
                else
                {
                    moved->Problem = fmt::format("{} is neither a location of {} nor a teleport point", placeName, session.GetZonePath());
                    return;
                }
            }
            moved->Done = session.TeleportWithinMap(*target, sWorld.GetSessions(), moved->Problem);
        }, World::CommandTimeout);
        if (!ran)
        {
            caller.Reply(fmt::format("The world did not answer within {} s, so the wizard may yet move", World::CommandTimeout.count()));
            return false;
        }
        if (!moved->Done)
        {
            caller.Reply(fmt::format("Not moved: {}", moved->Problem));
            return false;
        }
        PlayerPosition const& now = wizard->GetMovement().GetPosition();
        caller.Reply(fmt::format("Moved {} to {} in {}, at ({:.2f}, {:.2f}, {:.2f})", wizard->GetCharacterName().empty() ? std::string("the wizard") : wizard->GetCharacterName(),
            placeName, moved->Zone, now.X, now.Y, now.Z));
        return true;
    }

    constexpr float NpcStandDistance = 120.0f;

    std::optional<float> ReadNumber(CommandCaller& caller, std::string const& text)
    {
        std::optional<float> const value = Ambrose::StringTo<float>(text);
        if (!value)
            caller.Reply(fmt::format("{} is not a number", text));
        return value;
    }

    class TeleCommands : public CommandScript
    {
    public:
        TeleCommands() : CommandScript("cs_tele") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "tele", .SecurityLevel = SEC_GAMEMASTER, .Help = "teleport within a zone to one of its locations or a teleport point: <place> [wizard]", .Run = Tele, .Children = {
                    { .Name = "add", .SecurityLevel = SEC_GAMEMASTER, .AvailableOnConsole = false, .Help = "keep where you stand as a teleport point: <name>", .Run = TeleAdd },
                    { .Name = "del", .SecurityLevel = SEC_GAMEMASTER, .Help = "remove a teleport point: <name>", .Run = TeleDel },
                    { .Name = "zone", .SecurityLevel = SEC_GAMEMASTER, .Help = "travel to another zone, through its loading screen: <zone path> [location] [wizard]", .Run = TeleZone },
                    { .Name = "npc", .SecurityLevel = SEC_GAMEMASTER, .Help = "stand a little way from something placed in your zone, facing it: <name or id> [wizard] [distance]", .Run = TeleNpc },
                } },
                { .Name = "go", .SecurityLevel = SEC_GAMEMASTER, .AvailableOnConsole = false, .Help = "go somewhere in your zone", .Children = {
                    { .Name = "xyz", .SecurityLevel = SEC_GAMEMASTER, .AvailableOnConsole = false, .Help = "go to coordinates in your zone: <x> <y> <z> [yaw]", .Run = GoXyz },
                } },
                { .Name = "gps", .SecurityLevel = SEC_GAMEMASTER, .AvailableOnConsole = false, .Help = "say your zone, place, facing and zone instance", .Run = Gps },
            };
        }

    private:
        static bool Tele(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.empty() || arguments.size() > 2)
            {
                caller.Reply("Give a location of your zone, such as Start, or a teleport point, and the wizard to move if not your own");
                return false;
            }
            std::shared_ptr<GameSession> const wizard = arguments.size() == 2 ? NamedWizard(caller, arguments[1]) : CallerWizard(caller);
            return wizard && Move(caller, wizard, std::nullopt, arguments[0]);
        }

        static bool TeleNpc(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.empty() || arguments.size() > 3)
            {
                caller.Reply("Give the name, template id or global id of something placed in the zone, in quotes when it holds spaces, the wizard to move if not your own, and how far "
                             "from it to stand if not 120");
                return false;
            }
            float distance = NpcStandDistance;
            if (arguments.size() == 3)
            {
                std::optional<float> const given = ReadNumber(caller, arguments[2]);
                if (!given || *given < 0.0f)
                    return false;
                distance = *given;
            }
            std::shared_ptr<GameSession> const wizard = arguments.size() >= 2 ? NamedWizard(caller, arguments[1]) : CallerWizard(caller);
            if (!wizard)
                return false;
            auto const where = std::make_shared<std::pair<std::string, PlayerPosition>>();
            bool const ran = sWorld.RunFor(wizard, [where](GameSession& session)
            {
                *where = { session.GetZonePath(), session.GetMovement().GetPosition() };
            }, World::CommandTimeout);
            if (!ran)
            {
                caller.Reply(fmt::format("The world did not answer within {} s, so nobody was moved", World::CommandTimeout.count()));
                return false;
            }
            std::shared_ptr<ZoneObjects const> const objects = sZoneMgr.GetObjects();
            std::vector<ZoneObjectSpawn> const* placed = objects ? objects->In(where->first) : nullptr;
            if (!placed)
            {
                caller.Reply(fmt::format("Nothing is placed in {}, so nobody was moved", where->first));
                return false;
            }
            NpcStand const stand = NpcApproach::StandBeside(*placed, arguments[0], [](ZoneObjectSpawn const& object)
            {
                std::shared_ptr<ObjectTemplate const> const found = object.TemplateId ? sObjectTemplateMgr.GetTemplate(static_cast<uint32>(object.TemplateId)) : nullptr;
                return found ? found->ObjectName : std::string();
            }, where->second, distance);
            if (!stand.Place)
            {
                caller.Reply(fmt::format("Not moved: in {}, {}", where->first, stand.Problem));
                return false;
            }
            if (stand.Matches > 1)
                caller.Reply(fmt::format("{} things in {} answer to {}; going to the nearest", stand.Matches, where->first, arguments[0]));
            return Move(caller, wizard, stand.Place, fmt::format("beside {}", stand.Name));
        }

        static bool GoXyz(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.size() < 3 || arguments.size() > 4)
            {
                caller.Reply("Give x, y and z, and the facing in radians if you like");
                return false;
            }
            PlayerPosition place;
            std::optional<float> const x = ReadNumber(caller, arguments[0]);
            std::optional<float> const y = x ? ReadNumber(caller, arguments[1]) : std::nullopt;
            std::optional<float> const z = y ? ReadNumber(caller, arguments[2]) : std::nullopt;
            std::optional<float> const yaw = arguments.size() == 4 && z ? ReadNumber(caller, arguments[3]) : std::optional<float>(0.0f);
            if (!x || !y || !z || !yaw)
                return false;
            place = PlayerPosition{ *x, *y, *z, *yaw };
            std::shared_ptr<GameSession> const wizard = CallerWizard(caller);
            return wizard && Move(caller, wizard, place, fmt::format("({}, {}, {})", *x, *y, *z), arguments.size() == 3);
        }

        static bool Gps(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (!arguments.empty())
            {
                caller.Reply(".gps takes nothing after it");
                return false;
            }
            std::shared_ptr<GameSession> const wizard = CallerWizard(caller);
            if (!wizard)
                return false;
            auto const said = std::make_shared<std::string>();
            bool const ran = sWorld.RunFor(wizard, [said](GameSession& session)
            {
                PlayerPosition const& at = session.GetMovement().GetPosition();
                *said = fmt::format("Zone {}, x {:.2f}, y {:.2f}, z {:.2f}, yaw {:.3f}, zone instance {}", session.GetZonePath(), at.X, at.Y, at.Z, at.Yaw,
                    session.GetMapId() ? fmt::format("{}", *session.GetMapId()) : std::string("none"));
            }, World::CommandTimeout);
            caller.Reply(ran ? *said : fmt::format("The world did not answer within {} s", World::CommandTimeout.count()));
            return ran;
        }

        static bool TeleAdd(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.size() != 1)
            {
                caller.Reply("Give the new teleport point's name, in quotes when it holds spaces");
                return false;
            }
            std::shared_ptr<GameSession> const wizard = CallerWizard(caller);
            if (!wizard)
                return false;
            auto const tele = std::make_shared<GameTele>();
            bool const ran = sWorld.RunFor(wizard, [tele, name = arguments[0]](GameSession& session)
            {
                PlayerPosition const& at = session.GetMovement().GetPosition();
                *tele = GameTele{ name, session.GetZonePath(), at.X, at.Y, at.Z, at.Yaw };
            }, World::CommandTimeout);
            if (!ran || tele->Zone.empty())
            {
                caller.Reply("Your wizard's place could not be read, so no point was added");
                return false;
            }
            std::shared_ptr<ZoneLocations const> const locations = sZoneMgr.GetLocations();
            if (locations && locations->Find(tele->Zone, tele->Name).Result == ZoneLookup::Ok)
            {
                caller.Reply(fmt::format("Not added: {} is already a location of {}, which .tele {} reaches first", tele->Name, tele->Zone, tele->Name));
                return false;
            }
            std::string error;
            if (!sGameTeleMgr.Add(*tele, caller.GetName(), error))
            {
                caller.Reply(fmt::format("Not added: {}", error));
                return false;
            }
            caller.Reply(fmt::format("Added teleport point {} in {} at ({:.2f}, {:.2f}, {:.2f}); .tele {} works now", tele->Name, tele->Zone, tele->X, tele->Y, tele->Z, tele->Name));
            return true;
        }

        static bool TeleZone(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.empty() || arguments.size() > 3)
            {
                caller.Reply("Give a zone path, such as WizardCity/WC_Ravenwood, a location in it if not Start, and the wizard to send if not your own");
                return false;
            }
            std::string const zone = arguments[0];
            std::string const location = arguments.size() >= 2 ? arguments[1] : std::string(ZoneLocations::StartName);
            std::shared_ptr<ZoneTemplates const> const templates = sZoneMgr.GetTemplates();
            if (!templates || !templates->Has(zone))
            {
                caller.Reply(fmt::format("No zone has the path {}, so nobody was moved", zone));
                return false;
            }
            ZonePlace const place = sZoneMgr.FindPlace(zone, location);
            if (place.Result != ZoneLookup::Ok)
            {
                caller.Reply(fmt::format("{} has no location named {}, so nobody was moved", zone, location));
                return false;
            }
            std::shared_ptr<GameSession> const wizard = arguments.size() == 3 ? NamedWizard(caller, arguments[2]) : CallerWizard(caller);
            if (!wizard)
                return false;
            auto const problem = std::make_shared<std::string>();
            auto const asked = std::make_shared<bool>(false);
            ZoneTransfer transfer{ zone, zone, location, PlayerPosition{ place.Location.X, place.Location.Y, place.Location.Z, place.Location.Yaw } };
            bool const ran = sWorld.RunFor(wizard, [problem, asked, transfer](GameSession& session) mutable
            {
                *asked = session.RequestZoneTransfer(std::move(transfer), *problem);
            }, World::CommandTimeout);
            if (!ran)
            {
                caller.Reply(fmt::format("The world did not answer within {} s, so the wizard may yet travel", World::CommandTimeout.count()));
                return false;
            }
            caller.Reply(*asked ? fmt::format("Sending the wizard to {} in {}", location, zone) : fmt::format("Not sent: {}", *problem));
            return *asked;
        }

        static bool TeleDel(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.size() != 1)
            {
                caller.Reply("Give the teleport point's name");
                return false;
            }
            std::string error;
            if (!sGameTeleMgr.Remove(arguments[0], caller.GetName(), error))
            {
                caller.Reply(fmt::format("Not removed: {}", error));
                return false;
            }
            caller.Reply(fmt::format("Removed teleport point {}", arguments[0]));
            return true;
        }
    };
}

void AddSC_cs_tele()
{
    new TeleCommands();
}
