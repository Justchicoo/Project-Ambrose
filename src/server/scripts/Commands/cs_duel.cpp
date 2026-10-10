/*
 * Project Ambrose by Imjustchico
 * The duel group: 'duel start <creature template id> [wizard]' starts a duel at the combat sigil nearest a wizard, the game master who types it when none is named, seating the wizard on the sigil's first player circle and a creature of that template, placed on its first monster circle, and placing the duel circle the sigil's placement names with the duel its WizardClientDuelBehavior carries; 'duel end [wizard]' ends the duel a wizard is in with the wizard's team winning, which frees the wizard and takes the circle and creature away. Both run on the world thread, where the instances and duels are kept, and are answered once they have.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "DuelMgr.h"
#include "GameSession.h"
#include "MapMgr.h"
#include "ObjectTemplateMgr.h"
#include "ScriptMgr.h"
#include "Settings.h"
#include "SigilMgr.h"
#include "SpawnerMgr.h"
#include "StringUtil.h"
#include "TypeRegistry.h"
#include "World.h"
#include "ZoneMgr.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <bit>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    constexpr std::string_view CombatSigilClass = "class CombatSigilInfo";
    constexpr float SigilRange = 3000.0f;

    std::shared_ptr<GameSession> FindWizard(CommandCaller& caller, std::vector<std::string>::const_iterator begin, std::vector<std::string>::const_iterator end)
    {
        if (begin == end)
        {
            std::shared_ptr<GameSession> self = caller.GetCharacterId() != 0 ? sWorld.FindSessionByCharacterId(caller.GetCharacterId()) : nullptr;
            if (!self)
                caller.Reply("Name the wizard who duels, by character id or name, since this caller plays none");
            return self;
        }
        std::string const wizard = fmt::format("{}", fmt::join(begin, end, " "));
        std::vector<std::shared_ptr<GameSession>> const found = sWorld.FindInWorld(wizard);
        if (found.size() == 1)
            return found.front();
        if (found.empty())
            caller.Reply(fmt::format("No wizard in the world has the character id or name {}", wizard));
        else
            caller.Reply(fmt::format("{} wizards in the world are named {}; name one by its character id", found.size(), wizard));
        return nullptr;
    }

    std::chrono::milliseconds ReleaseDelay()
    {
        return std::chrono::milliseconds(sSettings.Get<uint32>("Zone.MobileIdReleaseDelay"));
    }

    ZoneObjectSpawn const* NearestSigil(std::vector<ZoneObjectSpawn> const& rows, Map const& map, PlayerPosition const& at, float& distance)
    {
        ZoneObjectSpawn const* nearest = nullptr;
        float best = SigilRange * SigilRange;
        for (ZoneObjectSpawn const& row : rows)
        {
            if (row.ClassName != CombatSigilClass || row.SigilTemplate.empty() || sDuelMgr.FindBySigil(map.GetDynamicZoneId(), row.Id))
                continue;
            float const dx = row.Position.X - at.X;
            float const dy = row.Position.Y - at.Y;
            float const dz = row.Position.Z - at.Z;
            float const squared = dx * dx + dy * dy + dz * dz;
            if (squared <= best)
            {
                best = squared;
                nearest = &row;
            }
        }
        distance = std::sqrt(best);
        return nearest;
    }

    struct Outcome
    {
        std::optional<uint64> DuelId;
        std::string Problem;
        std::string Detail;
    };

    void StartAt(GameSession& session, uint32 creatureTemplate, Outcome& outcome)
    {
        Map* const map = session.GetMapId() ? sMapMgr.Find(*session.GetMapId()) : nullptr;
        PlayerStats const* const stats = session.GetStats();
        if (!map || !stats)
        {
            outcome.Problem = "the wizard is not in a zone instance";
            return;
        }
        if (Duel const* const already = sDuelMgr.FindByParticipant(session.GetWorldGuid()))
        {
            outcome.Problem = fmt::format("the wizard is already in duel {}", already->GetId());
            return;
        }
        std::shared_ptr<ZoneObjects const> const zones = sZoneMgr.GetObjects();
        std::vector<ZoneObjectSpawn> const* const rows = zones ? zones->In(map->GetZonePath()) : nullptr;
        float distance = 0.0f;
        ZoneObjectSpawn const* const sigilRow = rows ? NearestSigil(*rows, *map, session.GetMovement().GetPosition(), distance) : nullptr;
        if (!sigilRow)
        {
            outcome.Problem = fmt::format("no free combat sigil of {} stands within {} of the wizard", map->GetZonePath(), SigilRange);
            return;
        }
        std::shared_ptr<SigilStore const> const sigils = sSigilMgr.GetSigils();
        SigilInfo const* const sigil = sigils ? sigils->FindByName(sigilRow->SigilTemplate) : nullptr;
        if (!sigil)
        {
            outcome.Problem = fmt::format("zone_object row {} names sigil {}, which the sigils read from the install do not hold", sigilRow->Id, sigilRow->SigilTemplate);
            return;
        }
        std::shared_ptr<ObjectTemplate const> const creature = sObjectTemplateMgr.GetTemplate(creatureTemplate);
        if (!creature || !creature->Object)
        {
            outcome.Problem = fmt::format("template {} cannot be read", creatureTemplate);
            return;
        }
        std::optional<CreatureCombatStats> const creatureStats = Duel::ReadCreature(*creature->Object, outcome.Problem);
        if (!creatureStats)
        {
            outcome.Problem = fmt::format("template {} cannot duel: {}", creatureTemplate, outcome.Problem);
            return;
        }
        std::shared_ptr<ObjectTemplate const> const playerTemplate = sObjectTemplateMgr.GetPlayer();

        Duel duel(map->GetDynamicZoneId(), sigilRow->Id, sigilRow->Position, sigilRow->Orientation.Z, *sigil);
        DuelCombatant wizard;
        wizard.OwnerId = session.GetWorldGuid();
        wizard.TemplateId = playerTemplate ? playerTemplate->TemplateId : 0;
        wizard.IsPlayer = true;
        wizard.SchoolId = std::bit_cast<int32>(stats->GetSchoolId());
        wizard.Health = stats->GetHitpoints();
        wizard.MaxHealth = stats->GetMaxHitpoints();
        wizard.Level = stats->GetLevel();
        DuelCombatant monster;
        monster.TemplateId = creatureTemplate;
        monster.SchoolId = creatureStats->SchoolId;
        monster.Health = creatureStats->Health;
        monster.MaxHealth = creatureStats->Health;
        monster.Level = creatureStats->Level;
        if (!duel.Seat(wizard, outcome.Problem) || !duel.Seat(monster, outcome.Problem))
            return;

        SpawnerContext const context = sSpawnerMgr.WorldContext(std::chrono::steady_clock::now(), ReleaseDelay());
        MapObjectChanges changes;
        DuelParticipant const& seat = duel.GetParticipants()[1];
        std::optional<uint64> const creatureId = SpawnerMgr::SpawnTemporary(*map, creatureTemplate, seat.Place.Position, seat.Place.Yaw, context, changes);
        if (!creatureId)
        {
            outcome.Problem = changes.Problems.empty() ? std::string("the creature could not be placed") : changes.Problems.front().Text;
            sMapMgr.QueueChanges(std::move(changes));
            return;
        }
        duel.SetOwner(1, *creatureId);
        std::optional<uint64> const circleId = SpawnerMgr::SpawnTemporary(*map, sigilRow->TemplateId, sigilRow->Position, sigilRow->Orientation.Z, context, changes,
            [&duel](PropertyObject& circle, uint64 globalId, std::string& problem) { return duel.DecorateCircle(circle, globalId, problem); });
        if (!circleId || !duel.EncodeParticipants(sTypeRegistry.GetCatalog(), outcome.Problem))
        {
            if (outcome.Problem.empty())
                outcome.Problem = changes.Problems.empty() ? std::string("the duel circle could not be placed") : changes.Problems.back().Text;
            if (circleId)
                SpawnerMgr::Despawn(*map, *circleId, std::nullopt, 0, context, changes);
            SpawnerMgr::Despawn(*map, *creatureId, std::nullopt, 0, context, changes);
            sMapMgr.QueueChanges(std::move(changes));
            return;
        }
        sMapMgr.QueueChanges(std::move(changes));
        Duel const& started = sDuelMgr.Add(std::move(duel));
        outcome.DuelId = started.GetId();
        outcome.Detail = fmt::format("at sigil {} (zone_object row {}, {:.0f} away) in {}, the wizard on circle {} and creature {} on circle {}", sigil->Name, sigilRow->Id,
            distance, map->GetZonePath(), started.GetParticipants()[0].Circle, *creatureId, started.GetParticipants()[1].Circle);
    }

    class DuelCommands : public CommandScript
    {
    public:
        DuelCommands() : CommandScript("cs_duel") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "duel", .SecurityLevel = SEC_GAMEMASTER, .Help = "duels a game master starts and ends", .Children = {
                    { .Name = "start", .SecurityLevel = SEC_GAMEMASTER, .Help = "start a duel at the nearest combat sigil between a wizard and a creature of a template", .Run = Start },
                    { .Name = "end", .SecurityLevel = SEC_GAMEMASTER, .Help = "end the duel a wizard is in, freeing the wizard", .Run = End },
                } },
            };
        }

    private:
        static bool Start(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            std::optional<uint32> const templateId = arguments.empty() ? std::nullopt : Ambrose::StringTo<uint32>(arguments[0]);
            if (!templateId || *templateId == 0)
            {
                caller.Reply("duel start takes a creature template id, and then a wizard when a console runs it, such as duel start 35085");
                return false;
            }
            std::shared_ptr<GameSession> const wizard = FindWizard(caller, arguments.begin() + 1, arguments.end());
            if (!wizard)
                return false;
            auto const outcome = std::make_shared<Outcome>();
            bool const ran = sWorld.RunFor(wizard, [outcome, id = *templateId](GameSession& session) { StartAt(session, id, *outcome); }, World::CommandTimeout);
            if (!ran)
            {
                caller.Reply("The world did not run the duel start in time");
                return false;
            }
            if (!outcome->DuelId)
            {
                caller.Reply(fmt::format("No duel was started: {}", outcome->Problem));
                return false;
            }
            caller.Reply(fmt::format("Started duel {} {}", *outcome->DuelId, outcome->Detail));
            return true;
        }

        static bool End(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            std::shared_ptr<GameSession> const wizard = FindWizard(caller, arguments.begin(), arguments.end());
            if (!wizard)
                return false;
            auto const outcome = std::make_shared<Outcome>();
            bool const ran = sWorld.RunFor(wizard, [outcome](GameSession& session)
            {
                outcome->DuelId = sDuelMgr.EndFor(session.GetWorldGuid(), Duel::PlayerTeam);
                if (!outcome->DuelId)
                    outcome->Problem = "the wizard is not in a duel";
            }, World::CommandTimeout);
            if (!ran)
            {
                caller.Reply("The world did not run the duel end in time");
                return false;
            }
            if (!outcome->DuelId)
            {
                caller.Reply(fmt::format("No duel was ended: {}", outcome->Problem));
                return false;
            }
            caller.Reply(fmt::format("Ended duel {}, the wizard's team winning", *outcome->DuelId));
            return true;
        }
    };
}

void AddSC_cs_duel()
{
    new DuelCommands();
}
