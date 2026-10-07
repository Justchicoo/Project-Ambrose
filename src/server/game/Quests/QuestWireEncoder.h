/*
 * Project Ambrose by Imjustchico
 * Typed quest, goal, loot, dialog and service wire models plus builders that use the loaded client schema and wrap encoded ObjectProperty data in SerializerBinary blobs.
 */

#ifndef AMBROSE_QUESTWIREENCODER_H
#define AMBROSE_QUESTWIREENCODER_H

#include "ActorDialog.h"
#include "BlobEnvelope.h"
#include "ObjectSerializer.h"
#include "QuestMadlibs.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace QuestWireEncoder
{
    inline constexpr uint32 GoalsAndRewardsMask = PropertyFlags::Bit(PropertyFlag::Save);
    inline constexpr uint32 ServiceMementoMask = PropertyFlags::Bit(PropertyFlag::Public);
    inline constexpr uint32 ActorDialogMask = PropertyFlags::Bit(PropertyFlag::AuthorityTransmit);

    struct ObjectBuildResult
    {
        PropertyObjectPtr Object;
        std::string Error;

        bool Ok() const noexcept { return Object && Error.empty(); }
    };

    struct BlobEncodeResult
    {
        std::vector<uint8> Bytes;
        std::string Error;

        bool Ok() const noexcept { return Error.empty() && !Bytes.empty(); }
    };

    struct ClientTagList
    {
        std::vector<std::string> Tags;
    };

    struct AssociatedWorldsList
    {
        std::vector<std::string> Worlds;
    };

    struct ServiceOptionFields
    {
        std::string ServiceName;
        std::string IconKey;
        std::string DisplayKey;
        uint32 ServiceIndex = 0;
        bool ForceInteract = false;
    };

    struct PrepEntry : ServiceOptionFields
    {
        std::string PrepText;
    };

    struct GoalEntry : ServiceOptionFields
    {
        uint64 QuestId = 0;
        uint64 GoalId = 0;
        std::string GoalTitle;
        std::string QuestTitle;
        uint32 GoalNameId = 0;
    };

    struct InteractableOption : ServiceOptionFields
    {
        int32 OptionIndex = 0;
    };

    using ServiceOption = std::variant<PrepEntry, GoalEntry, InteractableOption>;

    struct GoalEntryFull : GoalEntry
    {
        std::string PersonaName;
        Quests::GoalType Type = Quests::GoalType::Unknown;
        int32 Status = 0;
        int32 Count = 0;
        int32 Total = 0;
        bool UseTally = false;
        std::string TallyText;
        std::string TallyText2;
        std::string Location;
        std::string DestinationZone;
        std::string Image1;
        std::string Image2;
        std::vector<std::string> ClientTags;
        std::optional<QuestMadlibs::Block> Madlibs;
    };

    struct GoalCompilation
    {
        std::vector<GoalEntryFull> Goals;
    };

    struct GoldLootInfo
    {
        int32 GoldAmount = 0;
    };

    struct MagicXPLootInfo
    {
        std::string MagicSchool;
        int32 Experience = 0;
    };

    struct ItemLootInfo
    {
        uint64 ItemId = 0;
        int32 NumItems = 0;
    };

    struct AddSpellLootInfo
    {
        std::string SpellName;
        std::string InternalName;
        uint32 SpellId = 0;
    };

    using LootInfo = std::variant<GoldLootInfo, MagicXPLootInfo, ItemLootInfo, AddSpellLootInfo>;

    struct LootInfoList
    {
        std::vector<LootInfo> Loot;
        std::optional<GoldLootInfo> GoldInfo;
    };

    struct ServiceMementoBase
    {
        std::vector<ServiceOption> Options;
        std::optional<QuestMadlibs::Block> PersonaMadlibs;
        std::string NpcNameKey;
        std::string NpcTextKey;
        std::string NpcIcon;
        std::string NpcGreetingSound;
        std::string NpcFarewellSound;
        bool TurnPlayerToFace = false;
        bool ClickToInteractOnly = false;
    };

    ObjectBuildResult BuildMadlibBlock(TypeCatalogPtr const& catalog, QuestMadlibs::Block const& block);
    ObjectBuildResult BuildGoalCompilation(TypeCatalogPtr const& catalog, GoalCompilation const& compilation);
    ObjectBuildResult BuildClientTagList(TypeCatalogPtr const& catalog, ClientTagList const& tags);
    ObjectBuildResult BuildAssociatedWorldsList(TypeCatalogPtr const& catalog, AssociatedWorldsList const& worlds);
    ObjectBuildResult BuildLootInfoList(TypeCatalogPtr const& catalog, LootInfoList const& loot);
    ObjectBuildResult BuildActorDialog(TypeCatalogPtr const& catalog, Dialogs::ActorDialog const& dialog);
    ObjectBuildResult BuildPrepEntry(TypeCatalogPtr const& catalog, PrepEntry const& entry);
    ObjectBuildResult BuildGoalEntry(TypeCatalogPtr const& catalog, GoalEntry const& entry);
    ObjectBuildResult BuildGoalEntryFull(TypeCatalogPtr const& catalog, GoalEntryFull const& entry);
    ObjectBuildResult BuildInteractableOption(TypeCatalogPtr const& catalog, InteractableOption const& option);
    ObjectBuildResult BuildServiceMementoBase(TypeCatalogPtr const& catalog, ServiceMementoBase const& memento);

    BlobEncodeResult Encode(PropertyObject const& object, uint32 mask, BlobEnvelope::Packing packing = BlobEnvelope::Packing::Store);
}

#endif
