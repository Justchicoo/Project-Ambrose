/*
 * Project Ambrose by Imjustchico
 * One duel as the server holds it: its id, which is the global id of the circle object the client draws it with, the instance and the sigil it stands on with the sigil's place, yaw and circles, its round, phase and first team, and its participants, each seated on the first free circle of its own side, players on the player circles and creatures on the monster circles, with the place and facing SubCircle gives that circle. It takes the global id its circle is given as its own and fills the circle's behaviors, the Duel its WizardClientDuelBehavior carries and the start state of each of its state categories, so the ring plays its opening animation and settles into its idle loop on its own, the CombatParticipant each MSG_COMBATADD carries, written with the Public mask and no serializer flags as the client's MSG_CombatAdd reads it, and reads the health, level and school a creature's NPCBehaviorTemplate gives it; with the states a duel puts its participants in, Sigil then Stationary as it starts and Idle as it ends, and the start states of the circle's two state categories, OnAdd and NotInteracting.
 */

#ifndef AMBROSE_DUEL_H
#define AMBROSE_DUEL_H

#include "PropertyObject.h"
#include "SigilInfo.h"
#include "StringHash.h"
#include "SubCircle.h"
#include "TypeRegistry.h"
#include "Types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace DuelStates
{
    inline constexpr uint32 Sigil = StringHash::StringId("Sigil");
    inline constexpr uint32 Stationary = StringHash::StringId("Stationary");
    inline constexpr uint32 Idle = StringHash::StringId("Idle");
    inline constexpr uint32 CircleAdded = StringHash::StringId("OnAdd");
    inline constexpr uint32 CircleNotInteracting = StringHash::StringId("NotInteracting");
}

enum class DuelPhase : uint8
{
    Starting = 0,
    PrePlanning = 1,
    Planning = 2,
    PreExecution = 3,
    Execution = 4,
    Resolution = 5,
    Victory = 6,
    Ended = 7
};

struct DuelCombatant
{
    uint64 OwnerId = 0;
    uint64 TemplateId = 0;
    bool IsPlayer = false;
    int32 SchoolId = 0;
    int32 Health = 0;
    int32 MaxHealth = 0;
    int32 Level = 0;
};

struct DuelParticipant
{
    DuelCombatant Combatant;
    int32 Team = 0;
    int32 Circle = 0;
    float Rotation = 0.0f;
    float Radius = 0.0f;
    SubCirclePlacement Place;
};

struct CreatureCombatStats
{
    int32 Health = 0;
    int32 Level = 0;
    std::string School;
    int32 SchoolId = 0;
};

class Duel
{
public:
    static constexpr std::string_view DuelClass = "class Duel";
    static constexpr std::string_view ParticipantClass = "class CombatParticipant";
    static constexpr std::string_view BehaviorClass = "class WizardClientDuelBehavior";
    static constexpr std::string_view NpcBehaviorClass = "class NPCBehaviorTemplate";
    static constexpr std::string_view StateBehaviorClass = "class ObjectStateBehavior";
    static constexpr std::string_view PlayerCircle = "PlayerCircle";
    static constexpr std::string_view MonsterCircle = "MonsterCircle";
    static constexpr int32 PlayerTeam = 0;
    static constexpr int32 MonsterTeam = 1;

    Duel(uint32 mapId, uint64 sigilRow, PropertyTypes::Vector3D const& position, float yaw, SigilInfo sigil);

    uint64 GetId() const noexcept { return _id; }
    void SetId(uint64 id) noexcept { _id = id; }
    uint32 GetMapId() const noexcept { return _mapId; }
    uint64 GetSigilRow() const noexcept { return _sigilRow; }
    PropertyTypes::Vector3D const& GetPosition() const noexcept { return _position; }
    float GetYaw() const noexcept { return _yaw; }
    SigilInfo const& GetSigil() const noexcept { return _sigil; }
    DuelPhase GetPhase() const noexcept { return _phase; }
    void SetPhase(DuelPhase phase) noexcept { _phase = phase; }
    int32 GetRound() const noexcept { return _round; }
    int32 GetFirstTeam() const noexcept { return _firstTeam; }
    int32 GetWinningTeam() const noexcept { return _winningTeam; }
    void End(int32 winningTeam) noexcept;
    bool IsEnded() const noexcept { return _phase == DuelPhase::Ended; }

    std::optional<SubCirclePlacement> NextSeat(bool player) const;
    bool Seat(DuelCombatant const& combatant, std::string& problem);
    void SetOwner(std::size_t participant, uint64 ownerId) noexcept;
    std::vector<DuelParticipant> const& GetParticipants() const noexcept { return _participants; }
    DuelParticipant const* FindParticipant(uint64 ownerId) const noexcept;

    PropertyObjectPtr BuildDuelObject(TypeCatalogPtr const& catalog, std::string& problem) const;
    PropertyObjectPtr BuildParticipant(TypeCatalogPtr const& catalog, DuelParticipant const& participant, std::string& problem) const;
    bool FillBehavior(PropertyObject& behavior, std::string& problem) const;
    bool DecorateCircle(PropertyObject& circle, uint64 globalId, std::string& problem);
    bool EncodeParticipants(TypeCatalogPtr const& catalog, std::string& problem);
    std::vector<std::string> const& GetEncodedParticipants() const noexcept { return _encoded; }

    static std::optional<std::string> EncodeParticipant(PropertyObject const& participant, std::string& problem);
    static std::optional<CreatureCombatStats> ReadCreature(PropertyObject const& objectTemplate, std::string& problem);

private:
    std::optional<std::size_t> FreeCircle(std::string_view side) const;

    uint64 _id = 0;
    uint32 _mapId = 0;
    uint64 _sigilRow = 0;
    PropertyTypes::Vector3D _position;
    float _yaw = 0.0f;
    SigilInfo _sigil;
    DuelPhase _phase = DuelPhase::Starting;
    int32 _round = 0;
    int32 _firstTeam = PlayerTeam;
    int32 _winningTeam = PlayerTeam;
    std::vector<DuelParticipant> _participants;
    std::vector<std::string> _encoded;
};

#endif
