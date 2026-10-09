/*
 * Project Ambrose by Imjustchico
 * The npc group: 'npc spawn <template id> [wizard]' places a temporary object of that template in front of a wizard, the game master who types it when none is named, which every wizard near it is shown and nothing saves; 'npc delete [global id] [wizard]' takes away a spawned object, the one given or else the nearest a spawner or a game master placed near the wizard, with its despawn effect, and a spawner's object comes back after its respawn time. Both run on the world thread, where the instances are kept, and are answered once they have.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "GameSession.h"
#include "MapMgr.h"
#include "ScriptMgr.h"
#include "Settings.h"
#include "SpawnerMgr.h"
#include "StringUtil.h"
#include "World.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    constexpr float SpawnDistance = 150.0f;
    constexpr float DeleteRange = 600.0f;

    std::shared_ptr<GameSession> FindWizard(CommandCaller& caller, std::vector<std::string>::const_iterator begin, std::vector<std::string>::const_iterator end)
    {
        if (begin == end)
        {
            std::shared_ptr<GameSession> self = caller.GetCharacterId() != 0 ? sWorld.FindSessionByCharacterId(caller.GetCharacterId()) : nullptr;
            if (!self)
                caller.Reply("Name the wizard to act beside, by character id or name, since this caller plays none");
            return self;
        }
        std::string const wizard = fmt::format("{}", fmt::join(begin, end, " "));
        std::vector<std::shared_ptr<GameSession>> const found = sWorld.FindInWorld(wizard);
        if (found.size() == 1)
            return found.front();
        if (found.empty())
            caller.Reply(fmt::format("No wizard in the world has the character id or name {}", wizard));
        else
        {
            caller.Reply(fmt::format("{} wizards in the world are named {}; name one by its character id:", found.size(), wizard));
            for (std::shared_ptr<GameSession> const& session : found)
                caller.Reply(fmt::format("  {} on session {}", session->GetCharacterId(), session->GetSessionId()));
        }
        return nullptr;
    }

    std::chrono::milliseconds ReleaseDelay()
    {
        return std::chrono::milliseconds(sSettings.Get<uint32>("Zone.MobileIdReleaseDelay"));
    }

    struct Outcome
    {
        std::optional<uint64> GlobalId;
        std::string Problem;
        std::string Zone;
    };

    class NpcCommands : public CommandScript
    {
    public:
        NpcCommands() : CommandScript("cs_npc") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "npc", .SecurityLevel = SEC_GAMEMASTER, .Help = "objects placed while the world runs", .Children = {
                    { .Name = "spawn", .SecurityLevel = SEC_GAMEMASTER, .Help = "place a temporary object of a template in front of a wizard, shown to everyone near", .Run = Spawn },
                    { .Name = "delete", .SecurityLevel = SEC_GAMEMASTER, .Help = "take away a spawned object with its despawn effect, the nearest one when none is given", .Run = Delete },
                } },
            };
        }

    private:
        static bool Spawn(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            std::optional<uint32> const templateId = arguments.empty() ? std::nullopt : Ambrose::StringTo<uint32>(arguments[0]);
            if (!templateId || *templateId == 0)
            {
                caller.Reply("npc spawn takes a template id, and then a wizard when a console runs it, such as npc spawn 38232");
                return false;
            }
            std::shared_ptr<GameSession> const wizard = FindWizard(caller, arguments.begin() + 1, arguments.end());
            if (!wizard)
                return false;
            auto const outcome = std::make_shared<Outcome>();
            bool const ran = sWorld.RunFor(wizard, [outcome, id = *templateId](GameSession& session)
            {
                Map* const map = session.GetMapId() ? sMapMgr.Find(*session.GetMapId()) : nullptr;
                if (!map)
                {
                    outcome->Problem = "the wizard is not in a zone instance";
                    return;
                }
                PlayerPosition const& at = session.GetMovement().GetPosition();
                PropertyTypes::Vector3D const place{ at.X + SpawnDistance * std::cos(at.Yaw), at.Y + SpawnDistance * std::sin(at.Yaw), at.Z };
                MapObjectChanges changes;
                outcome->GlobalId = SpawnerMgr::SpawnTemporary(*map, id, place, at.Yaw + 3.14159265f, sSpawnerMgr.WorldContext(std::chrono::steady_clock::now(),
                    ReleaseDelay()), changes);
                outcome->Zone = map->GetZonePath();
                if (!outcome->GlobalId && !changes.Problems.empty())
                    outcome->Problem = changes.Problems.front().Text;
                sMapMgr.QueueChanges(std::move(changes));
            }, World::CommandTimeout);
            if (!ran)
            {
                caller.Reply("The world did not run the spawn in time");
                return false;
            }
            if (!outcome->GlobalId)
            {
                caller.Reply(fmt::format("Template {} was not spawned: {}", *templateId, outcome->Problem.empty() ? std::string("it could not be placed") : outcome->Problem));
                return false;
            }
            caller.Reply(fmt::format("Spawned template {} as object {} in {}", *templateId, *outcome->GlobalId, outcome->Zone));
            return true;
        }

        static bool Delete(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            std::optional<uint64> const given = arguments.empty() ? std::nullopt : Ambrose::StringTo<uint64>(arguments[0]);
            std::shared_ptr<GameSession> const wizard = FindWizard(caller, arguments.begin() + (given ? 1 : 0), arguments.end());
            if (!wizard)
                return false;
            auto const outcome = std::make_shared<Outcome>();
            bool const ran = sWorld.RunFor(wizard, [outcome, given](GameSession& session)
            {
                Map* const map = session.GetMapId() ? sMapMgr.Find(*session.GetMapId()) : nullptr;
                if (!map)
                {
                    outcome->Problem = "the wizard is not in a zone instance";
                    return;
                }
                PlayerPosition const& at = session.GetMovement().GetPosition();
                MapObject const* const target = given ? map->FindObject(*given) : SpawnerMgr::FindNearest(*map, { at.X, at.Y, at.Z }, DeleteRange);
                if (!target || target->Origin == MapObjectOrigin::Zone)
                {
                    outcome->Problem = given ? fmt::format("object {} is not one a spawner or a game master placed here", *given)
                                             : std::string("no spawned object stands near the wizard");
                    return;
                }
                uint64 const id = target->GlobalId;
                SpawnerContext const context = sSpawnerMgr.WorldContext(std::chrono::steady_clock::now(), ReleaseDelay());
                MapObjectChanges changes;
                if (SpawnerMgr::Despawn(*map, id, SpawnerMgr::DefaultDespawnEffect, session.GetWorldGuid(), context, changes))
                    outcome->GlobalId = id;
                outcome->Zone = map->GetZonePath();
                sMapMgr.QueueChanges(std::move(changes));
            }, World::CommandTimeout);
            if (!ran)
            {
                caller.Reply("The world did not run the delete in time");
                return false;
            }
            if (!outcome->GlobalId)
            {
                caller.Reply(fmt::format("Nothing was deleted: {}", outcome->Problem));
                return false;
            }
            caller.Reply(fmt::format("Deleted object {} in {}", *outcome->GlobalId, outcome->Zone));
            return true;
        }
    };
}

void AddSC_cs_npc()
{
    new NpcCommands();
}
