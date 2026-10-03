/*
 * Project Ambrose by Imjustchico
 * Lets game masters inspect their position, teleport within their current zone, and add or remove named world-database teleport points that become live at once.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "DatabaseEnv.h"
#include "GameSession.h"
#include "MovementPacking.h"
#include "ScriptMgr.h"
#include "StringUtil.h"
#include "World.h"
#include "WorldEdits.h"
#include "ZoneMgr.h"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    struct Target
    {
        std::shared_ptr<GameSession> Owner;
        GameSession* Session = nullptr;
        std::string Name;
    };

    std::string Join(std::vector<std::string> const& words, std::size_t begin, std::size_t end)
    {
        std::string joined;
        for (std::size_t index = begin; index < end; ++index)
        {
            if (!joined.empty())
                joined.push_back(' ');
            joined += words[index];
        }
        return joined;
    }

    std::string WizardName(GameSession const& session)
    {
        std::string name = session.GetCharacterName();
        return name.empty() ? fmt::format("wizard {}", session.GetCharacterId()) : name;
    }

    bool Ready(CommandCaller& caller, GameSession*& session)
    {
        session = caller.GetGameSession();
        if (session == nullptr || !session->IsShown())
        {
            caller.Reply("This command needs a wizard standing in the world");
            return false;
        }
        return true;
    }

    bool Packable(PlayerPosition const& position)
    {
        return MovementPacking::TryPackLocation(position.X).has_value() && MovementPacking::TryPackLocation(position.Y).has_value() &&
            MovementPacking::TryPackLocation(position.Z).has_value() && std::isfinite(position.Yaw);
    }

    std::optional<Target> FindTarget(CommandCaller& caller, GameSession& issuer, std::optional<std::string> const& wanted)
    {
        if (!wanted)
            return Target{ {}, &issuer, WizardName(issuer) };
        std::vector<std::shared_ptr<GameSession>> const found = sWorld.FindInWorld(*wanted);
        if (found.empty())
        {
            caller.Reply(fmt::format("No wizard in the world has the character id or name {}", *wanted));
            return std::nullopt;
        }
        if (found.size() > 1)
        {
            caller.Reply(fmt::format("{} wizards in the world are named {}; choose one by character id:", found.size(), *wanted));
            for (std::shared_ptr<GameSession> const& session : found)
                caller.Reply(fmt::format("  {} on session {}", session->GetCharacterId(), session->GetSessionId()));
            return std::nullopt;
        }
        return Target{ found.front(), found.front().get(), WizardName(*found.front()) };
    }

    bool TeleportTo(CommandCaller& caller, Target const& target, uint32 mapId, std::string zone, PlayerPosition const& destination)
    {
        auto teleported = std::make_shared<bool>(false);
        bool ran = false;
        if (target.Session == caller.GetGameSession())
        {
            ran = true;
            *teleported = target.Session->GetMapId() == mapId && target.Session->GetZonePath() == zone &&
                target.Session->TeleportWithinMap(destination);
        }
        else
            ran = sWorld.RunFor(target.Owner, [teleported, mapId, zone = std::move(zone), destination](GameSession& session)
            {
                if (session.GetMapId() == mapId && session.GetZonePath() == zone)
                    *teleported = session.TeleportWithinMap(destination);
            }, World::CommandTimeout);
        if (!ran)
        {
            caller.Reply(fmt::format("The world did not answer within {} s; {} may yet be teleported", World::CommandTimeout.count(), target.Name));
            return false;
        }
        if (!*teleported)
        {
            caller.Reply(fmt::format("{} is no longer standing in the same zone instance, or the client cannot pack that position", target.Name));
            return false;
        }
        caller.Reply(fmt::format("Teleported {} within the current zone", target.Name));
        return true;
    }

    bool Add(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        GameSession* session = nullptr;
        if (!Ready(caller, session))
            return false;
        if (arguments.empty())
        {
            caller.Reply("tele add takes a name for this wizard's current position");
            return false;
        }
        std::string const name(Ambrose::Trim(Join(arguments, 0, arguments.size())));
        if (name.empty() || name.size() > ZoneMgr::MaxGameTeleNameLength)
        {
            caller.Reply(fmt::format("A teleport name must contain 1 to {} bytes", ZoneMgr::MaxGameTeleNameLength));
            return false;
        }
        std::string const& zone = session->GetZonePath();
        ZonePlace const place = sZoneMgr.FindPlace(zone, name);
        if (place.Result == ZoneLookup::Ok || sZoneMgr.FindGameTele(zone, name))
        {
            caller.Reply(fmt::format("{} already names a teleport in {}", name, zone));
            return false;
        }
        PlayerPosition const position = session->GetMovement().GetPosition();
        if (!Packable(position))
        {
            caller.Reply("The current position is outside the client's packable range");
            return false;
        }
        GameTelePoint point{ name, position.X, position.Y, position.Z, position.Yaw };
        std::string const statement = fmt::format(
            "INSERT INTO `game_tele` (`zone_path`, `name`, `position_x`, `position_y`, `position_z`, `direction`) VALUES ('{}', '{}', {:.9g}, {:.9g}, {:.9g}, {:.9g})",
            WorldDatabase.Escape(zone), WorldDatabase.Escape(name), point.X, point.Y, point.Z, point.Yaw);
        std::string error;
        if (!WorldEdits::Apply(caller.GetName(), ".tele add", statement, error))
        {
            caller.Reply(error);
            return false;
        }
        if (!sZoneMgr.AddGameTele(zone, point))
        {
            caller.Reply(fmt::format("{} was saved, but the live teleport list could not be updated; run .reload game_tele", name));
            return false;
        }
        caller.Reply(fmt::format("Added {} in {} at {:.3f}, {:.3f}, {:.3f}, facing {:.3f}; it is live now", name, zone, position.X, position.Y, position.Z, position.Yaw));
        return true;
    }

    bool Delete(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        GameSession* session = nullptr;
        if (!Ready(caller, session))
            return false;
        if (arguments.empty())
        {
            caller.Reply("tele del takes the name of a saved GM teleport in this zone");
            return false;
        }
        std::string const name(Ambrose::Trim(Join(arguments, 0, arguments.size())));
        if (!sZoneMgr.FindGameTele(session->GetZonePath(), name))
        {
            caller.Reply(fmt::format("No saved GM teleport named {} is loaded in {}", name, session->GetZonePath()));
            return false;
        }
        std::string const statement = fmt::format("DELETE FROM `game_tele` WHERE `zone_path` = '{}' AND `name` = '{}'",
            WorldDatabase.Escape(session->GetZonePath()), WorldDatabase.Escape(name));
        std::string error;
        if (!WorldEdits::Apply(caller.GetName(), ".tele del", statement, error))
        {
            caller.Reply(error);
            return false;
        }
        if (!sZoneMgr.RemoveGameTele(session->GetZonePath(), name))
        {
            caller.Reply(fmt::format("{} was deleted, but the live teleport list could not be updated; run .reload game_tele", name));
            return false;
        }
        caller.Reply(fmt::format("Removed {} from {}; it is no longer live", name, session->GetZonePath()));
        return true;
    }

    bool Teleport(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        if (arguments.empty())
        {
            caller.Reply("tele takes a named place in this zone, optionally followed by to and a wizard id or quoted name");
            return false;
        }
        GameSession* issuer = nullptr;
        if (!Ready(caller, issuer))
            return false;
        auto const targetMarker = std::find(arguments.begin(), arguments.end(), "to");
        std::optional<std::string> wanted;
        std::size_t const nameEnd = static_cast<std::size_t>(targetMarker - arguments.begin());
        if (targetMarker != arguments.end())
        {
            if (nameEnd == 0 || nameEnd + 2 != arguments.size() || arguments[nameEnd + 1].empty())
            {
                caller.Reply("tele takes a place name, optionally followed by to and one wizard id or quoted name");
                return false;
            }
            wanted = arguments[nameEnd + 1];
        }
        std::string const name = Join(arguments, 0, nameEnd);
        if (name.empty())
        {
            caller.Reply("tele takes a named place in this zone");
            return false;
        }
        ZonePlace const place = sZoneMgr.FindPlace(issuer->GetZonePath(), name);
        PlayerPosition destination;
        if (place.Result == ZoneLookup::Ok)
            destination = { place.Location.X, place.Location.Y, place.Location.Z, place.Location.Yaw };
        else if (std::optional<GameTelePoint> const point = sZoneMgr.FindGameTele(issuer->GetZonePath(), name))
            destination = { point->X, point->Y, point->Z, point->Yaw };
        else
        {
            caller.Reply(fmt::format("No named place {} is loaded in {}", name, issuer->GetZonePath()));
            return false;
        }
        if (!Packable(destination))
        {
            caller.Reply(fmt::format("{} is outside the client's packable range", name));
            return false;
        }
        std::optional<Target> const target = FindTarget(caller, *issuer, wanted);
        if (!target)
            return false;
        std::optional<uint32> const mapId = issuer->GetMapId();
        if (!mapId || target->Session->GetMapId() != mapId || target->Session->GetZonePath() != issuer->GetZonePath())
        {
            caller.Reply(fmt::format("{} must be in the same zone instance", target->Name));
            return false;
        }
        return TeleportTo(caller, *target, *mapId, issuer->GetZonePath(), destination);
    }

    bool GoXYZ(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        if (arguments.size() != 3 && arguments.size() != 4)
        {
            caller.Reply("go xyz takes x, y and z, and optionally a yaw in radians");
            return false;
        }
        GameSession* session = nullptr;
        if (!Ready(caller, session))
            return false;
        std::optional<float> const x = Ambrose::StringTo<float>(arguments[0]);
        std::optional<float> const y = Ambrose::StringTo<float>(arguments[1]);
        std::optional<float> const z = Ambrose::StringTo<float>(arguments[2]);
        std::optional<float> const yaw = arguments.size() == 4 ? Ambrose::StringTo<float>(arguments[3]) :
            std::optional<float>(session->GetMovement().GetPosition().Yaw);
        if (!x || !y || !z || !yaw)
        {
            caller.Reply("go xyz needs numeric coordinates and an optional numeric yaw");
            return false;
        }
        PlayerPosition const destination{ *x, *y, *z, *yaw };
        if (!Packable(destination))
        {
            caller.Reply("go xyz is outside the client's packable range or has a non-finite yaw");
            return false;
        }
        if (!session->TeleportWithinMap(destination))
        {
            caller.Reply("The wizard could not be teleported within this map");
            return false;
        }
        caller.Reply(fmt::format("Teleported {} to {:.3f}, {:.3f}, {:.3f}, facing {:.3f}", WizardName(*session),
            session->GetMovement().GetPosition().X, session->GetMovement().GetPosition().Y, session->GetMovement().GetPosition().Z,
            session->GetMovement().GetPosition().Yaw));
        return true;
    }

    bool Gps(CommandCaller& caller, std::vector<std::string> const&)
    {
        GameSession* session = nullptr;
        if (!Ready(caller, session))
            return false;
        PlayerPosition const& position = session->GetMovement().GetPosition();
        caller.Reply(fmt::format("{} in {} at x={:.3f}, y={:.3f}, z={:.3f}, yaw={:.3f}; dynamic zone {}",
            WizardName(*session), session->GetZonePath(), position.X, position.Y, position.Z, position.Yaw, *session->GetMapId()));
        return true;
    }

    class TeleCommands final : public CommandScript
    {
    public:
        TeleCommands() : CommandScript("cs_tele") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "tele", .SecurityLevel = SEC_GAMEMASTER, .AvailableOnConsole = false, .Help = "teleport within this zone or manage named GM points", .Run = Teleport, .Children = {
                    { .Name = "add", .SecurityLevel = SEC_GAMEMASTER, .AvailableOnConsole = false, .Help = "save this position as a live GM teleport point", .Run = Add },
                    { .Name = "del", .SecurityLevel = SEC_GAMEMASTER, .AvailableOnConsole = false, .Help = "remove a saved GM teleport point from this zone", .Run = Delete },
                } },
                { .Name = "go", .SecurityLevel = SEC_GAMEMASTER, .AvailableOnConsole = false, .Help = "move to coordinates in this zone", .Children = {
                    { .Name = "xyz", .SecurityLevel = SEC_GAMEMASTER, .AvailableOnConsole = false, .Help = "teleport to x, y, z and optional yaw", .Run = GoXYZ },
                } },
                { .Name = "gps", .SecurityLevel = SEC_GAMEMASTER, .AvailableOnConsole = false, .Help = "show this wizard's current position and dynamic zone", .Run = Gps },
            };
        }
    };
}

void AddSC_cs_tele()
{
    new TeleCommands();
}
