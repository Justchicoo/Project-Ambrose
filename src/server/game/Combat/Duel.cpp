/*
 * Project Ambrose by Imjustchico
 * Seats each participant on the first circle of its side the sigil lists that no one holds yet, copying the circle's angle and radius into the participant because the client places a participant from its own rotation and radius, and fills the Duel and CombatParticipant objects through the catalog, leaving every property the server does not decide yet at the type dump's default.
 */

#include "Duel.h"
#include "ObjectFields.h"
#include "ObjectSerializer.h"
#include "PropertyFiller.h"
#include "PropertyFlags.h"
#include "StringHash.h"

#include <fmt/format.h>

#include <algorithm>
#include <bit>
#include <utility>

namespace
{
    std::optional<int32> WholeOf(PropertyObject const& object, std::string_view name)
    {
        PropertyValue const* const value = object.Get(name);
        if (!value)
            return std::nullopt;
        if (int32 const* const signedValue = value->GetIf<int32>())
            return *signedValue;
        if (uint32 const* const unsignedValue = value->GetIf<uint32>())
            return static_cast<int32>(*unsignedValue);
        return std::nullopt;
    }
}

Duel::Duel(uint32 mapId, uint64 sigilRow, PropertyTypes::Vector3D const& position, float yaw, SigilInfo sigil)
    : _mapId(mapId), _sigilRow(sigilRow), _position(position), _yaw(yaw), _sigil(std::move(sigil))
{
}

void Duel::End(int32 winningTeam) noexcept
{
    _winningTeam = winningTeam;
    _phase = DuelPhase::Ended;
}

std::optional<std::size_t> Duel::FreeCircle(std::string_view side) const
{
    for (std::size_t index = 0; index < _sigil.Circles.size(); ++index)
    {
        if (_sigil.Circles[index].LocationType != side)
            continue;
        bool const taken = std::any_of(_participants.begin(), _participants.end(), [index](DuelParticipant const& seated) { return seated.Circle == static_cast<int32>(index); });
        if (!taken)
            return index;
    }
    return std::nullopt;
}

std::optional<SubCirclePlacement> Duel::NextSeat(bool player) const
{
    std::optional<std::size_t> const circle = FreeCircle(player ? PlayerCircle : MonsterCircle);
    if (!circle)
        return std::nullopt;
    SigilCircle const& at = _sigil.Circles[*circle];
    return SubCircle::Place(_position, _yaw, at.Rotation, at.Radius);
}

bool Duel::Seat(DuelCombatant const& combatant, std::string& problem)
{
    std::string_view const side = combatant.IsPlayer ? PlayerCircle : MonsterCircle;
    std::optional<std::size_t> const circle = FreeCircle(side);
    if (!circle)
    {
        problem = fmt::format("sigil {} has no free {}", _sigil.Name, side);
        return false;
    }
    SigilCircle const& at = _sigil.Circles[*circle];
    DuelParticipant participant;
    participant.Combatant = combatant;
    participant.Team = combatant.IsPlayer ? PlayerTeam : MonsterTeam;
    participant.Circle = static_cast<int32>(*circle);
    participant.Rotation = at.Rotation;
    participant.Radius = at.Radius;
    participant.Place = SubCircle::Place(_position, _yaw, at.Rotation, at.Radius);
    _participants.push_back(std::move(participant));
    return true;
}

void Duel::SetOwner(std::size_t participant, uint64 ownerId) noexcept
{
    if (participant < _participants.size())
        _participants[participant].Combatant.OwnerId = ownerId;
}

DuelParticipant const* Duel::FindParticipant(uint64 ownerId) const noexcept
{
    auto const found = std::find_if(_participants.begin(), _participants.end(), [ownerId](DuelParticipant const& seated) { return seated.Combatant.OwnerId == ownerId; });
    return found == _participants.end() ? nullptr : &*found;
}

PropertyObjectPtr Duel::BuildDuelObject(TypeCatalogPtr const& catalog, std::string& problem) const
{
    PropertyObjectPtr duel = catalog ? PropertyObject::Create(catalog, DuelClass) : nullptr;
    if (!duel)
    {
        problem = fmt::format("the type dump has no {}", DuelClass);
        return nullptr;
    }
    PropertyFiller(*duel, problem)
        .Set("m_duelID.m_full", _id)
        .Set("m_position", _position)
        .Set("m_yaw", _yaw)
        .Set("m_firstTeamToAct", _firstTeam)
        .Set("m_originalFirstTeamToAct", _firstTeam)
        .Set("m_roundNum", _round)
        .Set("m_bPVP", false);
    if (!problem.empty())
        return nullptr;
    return duel;
}

PropertyObjectPtr Duel::BuildParticipant(TypeCatalogPtr const& catalog, DuelParticipant const& participant, std::string& problem) const
{
    PropertyObjectPtr made = catalog ? PropertyObject::Create(catalog, ParticipantClass) : nullptr;
    if (!made)
    {
        problem = fmt::format("the type dump has no {}", ParticipantClass);
        return nullptr;
    }
    DuelCombatant const& combatant = participant.Combatant;
    PropertyFiller(*made, problem)
        .Set("m_ownerID.m_full", combatant.OwnerId)
        .Set("m_templateID.m_full", combatant.TemplateId)
        .Set("m_isPlayer", combatant.IsPlayer)
        .Set("m_zoneID.m_full", uint64{ _mapId })
        .Set("m_teamID", participant.Team)
        .Set("m_originalTeam", participant.Team)
        .Set("m_primaryMagicSchoolID", combatant.SchoolId)
        .Set("m_playerHealth", combatant.Health)
        .Set("m_maxPlayerHealth", combatant.MaxHealth)
        .Set("m_curMaxHP", combatant.MaxHealth)
        .Set("m_rotation", participant.Rotation)
        .Set("m_radius", participant.Radius)
        .Set("m_subcircle", participant.Circle)
        .Set("m_isMonster", uint32{ combatant.IsPlayer ? 0u : 1u })
        .Set("m_mobLevel", combatant.IsPlayer ? 0 : combatant.Level);
    if (!problem.empty())
        return nullptr;
    return made;
}

bool Duel::FillBehavior(PropertyObject& behavior, std::string& problem) const
{
    if (!behavior.IsA(BehaviorClass))
    {
        problem = fmt::format("{} is not a {}", behavior.GetClass().Name, BehaviorClass);
        return false;
    }
    PropertyObjectPtr duel = BuildDuelObject(behavior.GetCatalog(), problem);
    if (!duel)
        return false;
    PropertyFiller(behavior, problem).Set("m_pDuel", std::move(duel)).Set("m_sigilTemplateID", _sigil.TemplateId);
    return problem.empty();
}

bool Duel::DecorateCircle(PropertyObject& circle, uint64 globalId, std::string& problem)
{
    _id = globalId;
    PropertyInfo const* const property = circle.GetClass().FindProperty("m_inactiveBehaviors");
    PropertyValue const* const behaviors = circle.Get("m_inactiveBehaviors");
    PropertyValue::List const* const list = behaviors ? behaviors->GetList() : nullptr;
    if (!property || !list)
    {
        problem = fmt::format("{} has no m_inactiveBehaviors", circle.GetClass().Name);
        return false;
    }
    std::size_t const ordinal = circle.GetClass().PropertyByName.at(property->Name);
    bool filled = false;
    for (std::size_t index = 0; index < list->size(); ++index)
    {
        PropertyObject* const behavior = circle.EditObjectAt(ordinal, index);
        if (!behavior)
            continue;
        if (behavior->IsA(BehaviorClass))
            filled = FillBehavior(*behavior, problem);
        else if (behavior->IsA(StateBehaviorClass))
            PropertyFiller(*behavior, problem).Set("m_stateList", PropertyValue::List{ PropertyValue(DuelStates::CircleAdded), PropertyValue(DuelStates::CircleNotInteracting) });
        if (!problem.empty())
            return false;
    }
    if (!filled)
        problem = fmt::format("the circle's template gives it no {}", BehaviorClass);
    return filled;
}

bool Duel::EncodeParticipants(TypeCatalogPtr const& catalog, std::string& problem)
{
    std::vector<std::string> encoded;
    encoded.reserve(_participants.size());
    for (DuelParticipant const& participant : _participants)
    {
        PropertyObjectPtr const made = BuildParticipant(catalog, participant, problem);
        std::optional<std::string> bytes = made ? EncodeParticipant(*made, problem) : std::nullopt;
        if (!bytes)
            return false;
        encoded.push_back(std::move(*bytes));
    }
    _encoded = std::move(encoded);
    return true;
}

std::optional<std::string> Duel::EncodeParticipant(PropertyObject const& participant, std::string& problem)
{
    ObjectField const* const field = ObjectFields::Find("MSG_COMBATADD", "ParticipantData");
    if (!field)
    {
        problem = "MSG_COMBATADD's ParticipantData is not declared";
        return std::nullopt;
    }
    SerializerOptions options;
    options.Mask = PropertyFlags::Bit(PropertyFlag::Public);
    options.Flags = SerializerFlag::None;
    EncodeResult encoded = ObjectSerializer::EncodeField(*field, &participant, options);
    if (!encoded.Ok())
    {
        problem = std::move(encoded.Detail);
        return std::nullopt;
    }
    return std::string(encoded.Bytes.begin(), encoded.Bytes.end());
}

std::optional<CreatureCombatStats> Duel::ReadCreature(PropertyObject const& objectTemplate, std::string& problem)
{
    PropertyValue const* const behaviors = objectTemplate.Get("m_behaviors");
    PropertyValue::List const* const list = behaviors ? behaviors->GetList() : nullptr;
    if (list)
        for (PropertyValue const& value : *list)
        {
            PropertyObject const* const behavior = value.AsObject();
            if (!behavior || !behavior->IsA(NpcBehaviorClass))
                continue;
            CreatureCombatStats stats;
            std::optional<int32> const health = WholeOf(*behavior, "m_nStartingHealth");
            std::optional<int32> const level = WholeOf(*behavior, "m_nLevel");
            PropertyValue const* const school = behavior->Get("m_schoolOfFocus");
            std::string const* const schoolName = school ? school->GetIf<std::string>() : nullptr;
            if (!health || !level || !schoolName)
            {
                problem = fmt::format("its {} lacks a starting health, level or school of focus", NpcBehaviorClass);
                return std::nullopt;
            }
            stats.Health = *health;
            stats.Level = *level;
            stats.School = *schoolName;
            stats.SchoolId = std::bit_cast<int32>(StringHash::StringId(*schoolName));
            return stats;
        }
    problem = fmt::format("it has no {}, so it is not a creature that duels", NpcBehaviorClass);
    return std::nullopt;
}
