/*
 * Project Ambrose by Imjustchico
 * Builds quest and dialog ObjectProperty objects from typed game models, serializes them with their client property masks and adds the four-byte SerializerBinary envelope.
 */

#include "QuestWireEncoder.h"

#include <new>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace QuestWireEncoder
{
    namespace
    {
        ObjectBuildResult Failure(std::string error)
        {
            return { nullptr, std::move(error) };
        }

        ObjectBuildResult Create(TypeCatalogPtr const& catalog, std::string_view className)
        {
            if (!catalog)
                return Failure("no type catalog is loaded");
            PropertyObjectPtr object = PropertyObject::Create(catalog, className);
            if (!object)
                return Failure("the type catalog does not contain property class " + std::string(className));
            return { std::move(object), {} };
        }

        bool Set(PropertyObject& object, std::string_view property, PropertyValue value, std::string& error)
        {
            PropertySetResult const result = object.Set(property, std::move(value));
            if (result == PropertySetResult::Ok)
                return true;
            error = object.GetClass().Name + "::" + std::string(property) + " rejected its value: " + std::string(PropertyObject::GetResultName(result));
            return false;
        }

        template<typename T>
        bool SetValue(PropertyObject& object, std::string_view property, T&& value, std::string& error)
        {
            return Set(object, property, PropertyValue(std::forward<T>(value)), error);
        }

        bool SetStringList(PropertyObject& object, std::string_view property, std::vector<std::string> const& strings, std::string& error)
        {
            PropertyValue::List values;
            values.reserve(strings.size());
            for (std::string const& text : strings)
                values.emplace_back(text);
            return SetValue(object, property, std::move(values), error);
        }

        std::string_view MadlibClass(QuestMadlibs::ArgumentValue const& value)
        {
            return std::visit([](auto const& argument) -> std::string_view
            {
                using Value = std::remove_cvref_t<decltype(argument)>;
                if constexpr (std::is_same_v<Value, std::string>)
                    return "MadlibArgT<std::string>";
                else if constexpr (std::is_same_v<Value, QuestMadlibs::ConstStringArgument>)
                    return "MadlibArgT<std::string const>";
                else if constexpr (std::is_same_v<Value, std::u16string>)
                    return "MadlibArgT<std::wstring>";
                else if constexpr (std::is_same_v<Value, int32>)
                    return "MadlibArgT<int>";
                else if constexpr (std::is_same_v<Value, uint32>)
                    return "MadlibArgT<unsigned int>";
                else if constexpr (std::is_same_v<Value, uint64>)
                    return "MadlibArgT<unsigned __int64>";
                else if constexpr (std::is_same_v<Value, float>)
                    return "MadlibArgT<float>";
                else
                    return "MadlibArgT<double>";
            }, value);
        }

        PropertyValue MadlibValue(QuestMadlibs::ArgumentValue const& value)
        {
            return std::visit([](auto const& argument) -> PropertyValue
            {
                using Value = std::remove_cvref_t<decltype(argument)>;
                if constexpr (std::is_same_v<Value, QuestMadlibs::ConstStringArgument>)
                    return PropertyValue(argument.Value);
                else
                    return PropertyValue(argument);
            }, value);
        }

        ObjectBuildResult BuildServiceOption(TypeCatalogPtr const& catalog, ServiceOption const& option)
        {
            return std::visit([&](auto const& entry) -> ObjectBuildResult
            {
                using Value = std::remove_cvref_t<decltype(entry)>;
                if constexpr (std::is_same_v<Value, PrepEntry>)
                    return BuildPrepEntry(catalog, entry);
                else if constexpr (std::is_same_v<Value, GoalEntry>)
                    return BuildGoalEntry(catalog, entry);
                else
                    return BuildInteractableOption(catalog, entry);
            }, option);
        }

        bool SetServiceFields(PropertyObject& object, ServiceOptionFields const& fields, std::string& error)
        {
            return SetValue(object, "m_serviceName", fields.ServiceName, error) &&
                SetValue(object, "m_iconKey", fields.IconKey, error) &&
                SetValue(object, "m_displayKey", fields.DisplayKey, error) &&
                SetValue(object, "m_serviceIndex", fields.ServiceIndex, error) &&
                SetValue(object, "m_forceInteract", fields.ForceInteract, error);
        }

        bool SetGoalEntryFields(PropertyObject& object, GoalEntry const& entry, std::string& error)
        {
            return SetServiceFields(object, entry, error) &&
                SetValue(object, "m_questID", entry.QuestId, error) &&
                SetValue(object, "m_goalID", entry.GoalId, error) &&
                SetValue(object, "m_goalTitle", entry.GoalTitle, error) &&
                SetValue(object, "m_questTitle", entry.QuestTitle, error) &&
                SetValue(object, "m_goalNameID", entry.GoalNameId, error);
        }

        ObjectBuildResult BuildLoot(TypeCatalogPtr const& catalog, LootInfo const& info)
        {
            return std::visit([&](auto const& loot) -> ObjectBuildResult
            {
                using Value = std::remove_cvref_t<decltype(loot)>;
                std::string_view className;
                int64 lootType = 0;
                if constexpr (std::is_same_v<Value, GoldLootInfo>)
                {
                    className = "class GoldLootInfo";
                    lootType = 1;
                }
                else if constexpr (std::is_same_v<Value, MagicXPLootInfo>)
                {
                    className = "class MagicXPLootInfo";
                    lootType = 5;
                }
                else if constexpr (std::is_same_v<Value, ItemLootInfo>)
                {
                    className = "class ItemLootInfo";
                    lootType = 3;
                }
                else
                {
                    className = "class AddSpellLootInfo";
                    lootType = 6;
                }

                ObjectBuildResult result = Create(catalog, className);
                if (!result.Ok())
                    return result;
                std::string error;
                if (!SetValue(*result.Object, "m_lootType", lootType, error))
                    return Failure(std::move(error));
                if constexpr (std::is_same_v<Value, GoldLootInfo>)
                {
                    if (!SetValue(*result.Object, "m_goldAmount", loot.GoldAmount, error))
                        return Failure(std::move(error));
                }
                else if constexpr (std::is_same_v<Value, MagicXPLootInfo>)
                {
                    if (!SetValue(*result.Object, "m_magicSchool", loot.MagicSchool, error) ||
                        !SetValue(*result.Object, "m_experience", loot.Experience, error))
                        return Failure(std::move(error));
                }
                else if constexpr (std::is_same_v<Value, ItemLootInfo>)
                {
                    if (!SetValue(*result.Object, "m_itemID", loot.ItemId, error) ||
                        !SetValue(*result.Object, "m_numItems", loot.NumItems, error))
                        return Failure(std::move(error));
                }
                else if (!SetValue(*result.Object, "m_spellName", loot.SpellName, error) ||
                    !SetValue(*result.Object, "m_internalName", loot.InternalName, error) ||
                    !SetValue(*result.Object, "m_spellID", loot.SpellId, error))
                    return Failure(std::move(error));
                return result;
            }, info);
        }

        ObjectBuildResult BuildNpcDialogEntry(TypeCatalogPtr const& catalog, Dialogs::ActorDialogEntry const& entry)
        {
            ObjectBuildResult result = Create(catalog, "class NPCDialogEntry");
            if (!result.Ok())
                return result;
            std::string error;
            if (!SetValue(*result.Object, "m_dialog", entry.DialogKey, error) ||
                !SetValue(*result.Object, "m_picture", entry.Picture, error) ||
                !SetValue(*result.Object, "m_guiDisplay", entry.GuiDisplay, error) ||
                !SetValue(*result.Object, "m_soundFile", entry.Sound, error) ||
                !SetValue(*result.Object, "m_action", entry.Action, error) ||
                !SetValue(*result.Object, "m_dialogEvent", entry.DialogEvent, error) ||
                !SetValue(*result.Object, "m_actorTemplateID", entry.ActorTemplateId, error) ||
                !SetStringList(*result.Object, "m_dialogAnimationList", entry.Animations, error))
                return Failure(std::move(error));
            if (entry.PersonaName && !SetValue(*result.Object, "m_personaName", *entry.PersonaName, error))
                return Failure(std::move(error));
            if (entry.NameOverride && !SetValue(*result.Object, "m_nameOverride", *entry.NameOverride, error))
                return Failure(std::move(error));
            return result;
        }

        ObjectBuildResult BuildActorMadlib(TypeCatalogPtr const& catalog, Dialogs::ActorMadlib const& madlib)
        {
            ObjectBuildResult block = BuildMadlibBlock(catalog, madlib.Block);
            if (!block.Ok())
                return block;
            ObjectBuildResult result = Create(catalog, "class ActorMadlib");
            if (!result.Ok())
                return result;
            std::string error;
            if (!SetValue(*result.Object, "m_madlibBlock", std::move(block.Object), error) ||
                !SetValue(*result.Object, "m_index", madlib.Index, error))
                return Failure(std::move(error));
            return result;
        }
    }

    ObjectBuildResult BuildMadlibBlock(TypeCatalogPtr const& catalog, QuestMadlibs::Block const& block)
    {
        ObjectBuildResult result = Create(catalog, "class MadlibBlock");
        if (!result.Ok())
            return result;
        PropertyValue::List arguments;
        arguments.reserve(block.Arguments.size());
        for (QuestMadlibs::Argument const& argument : block.Arguments)
        {
            if (argument.Token.empty())
                return Failure("a madlib argument has an empty token");
            ObjectBuildResult item = Create(catalog, MadlibClass(argument.Value));
            if (!item.Ok())
                return item;
            std::string error;
            if (!SetValue(*item.Object, "m_madlibToken", argument.Token, error) ||
                !Set(*item.Object, "m_madlibArgument", MadlibValue(argument.Value), error))
                return Failure(std::move(error));
            arguments.emplace_back(std::move(item.Object));
        }
        std::string error;
        if (!SetValue(*result.Object, "m_madlibs", std::move(arguments), error) ||
            !SetValue(*result.Object, "m_blockToken", block.BlockToken, error))
            return Failure(std::move(error));
        return result;
    }

    ObjectBuildResult BuildGoalCompilation(TypeCatalogPtr const& catalog, GoalCompilation const& compilation)
    {
        ObjectBuildResult result = Create(catalog, "class GoalCompilation");
        if (!result.Ok())
            return result;
        PropertyValue::List goals;
        goals.reserve(compilation.Goals.size());
        for (GoalEntryFull const& goal : compilation.Goals)
        {
            ObjectBuildResult item = BuildGoalEntryFull(catalog, goal);
            if (!item.Ok())
                return item;
            goals.emplace_back(std::move(item.Object));
        }
        std::string error;
        if (!Set(*result.Object, "m_goals", std::move(goals), error))
            return Failure(std::move(error));
        return result;
    }

    ObjectBuildResult BuildClientTagList(TypeCatalogPtr const& catalog, ClientTagList const& tags)
    {
        ObjectBuildResult result = Create(catalog, "class ClientTagList");
        if (!result.Ok())
            return result;
        std::string error;
        if (!SetStringList(*result.Object, "m_clientTags", tags.Tags, error))
            return Failure(std::move(error));
        return result;
    }

    ObjectBuildResult BuildAssociatedWorldsList(TypeCatalogPtr const& catalog, AssociatedWorldsList const& worlds)
    {
        ObjectBuildResult result = Create(catalog, "class AssociatedWorldsList");
        if (!result.Ok())
            return result;
        std::string error;
        if (!SetStringList(*result.Object, "m_associatedWorlds", worlds.Worlds, error))
            return Failure(std::move(error));
        return result;
    }

    ObjectBuildResult BuildLootInfoList(TypeCatalogPtr const& catalog, LootInfoList const& loot)
    {
        ObjectBuildResult result = Create(catalog, "class LootInfoList");
        if (!result.Ok())
            return result;
        PropertyValue::List items;
        items.reserve(loot.Loot.size());
        for (LootInfo const& info : loot.Loot)
        {
            ObjectBuildResult item = BuildLoot(catalog, info);
            if (!item.Ok())
                return item;
            items.emplace_back(std::move(item.Object));
        }
        std::string error;
        if (!Set(*result.Object, "m_loot", std::move(items), error))
            return Failure(std::move(error));
        if (loot.GoldInfo)
        {
            ObjectBuildResult gold = BuildLoot(catalog, LootInfo(*loot.GoldInfo));
            if (!gold.Ok())
                return gold;
            if (!SetValue(*result.Object, "m_goldInfo", std::move(gold.Object), error))
                return Failure(std::move(error));
        }
        return result;
    }

    ObjectBuildResult BuildActorDialog(TypeCatalogPtr const& catalog, Dialogs::ActorDialog const& dialog)
    {
        ObjectBuildResult result = Create(catalog, "class ActorDialog");
        if (!result.Ok())
            return result;
        PropertyValue::List entries;
        entries.reserve(dialog.Entries.size());
        for (Dialogs::ActorDialogEntry const& entry : dialog.Entries)
        {
            ObjectBuildResult item = BuildNpcDialogEntry(catalog, entry);
            if (!item.Ok())
                return item;
            entries.emplace_back(std::move(item.Object));
        }
        PropertyValue::List madlibs;
        madlibs.reserve(dialog.Madlibs.size());
        for (Dialogs::ActorMadlib const& madlib : dialog.Madlibs)
        {
            ObjectBuildResult item = BuildActorMadlib(catalog, madlib);
            if (!item.Ok())
                return item;
            madlibs.emplace_back(std::move(item.Object));
        }
        std::string error;
        if (!SetValue(*result.Object, "m_dialogTag", dialog.Tag, error) ||
            !Set(*result.Object, "m_dialogEntries", std::move(entries), error) ||
            !Set(*result.Object, "m_madlibs", std::move(madlibs), error) ||
            !SetStringList(*result.Object, "m_dialogEvents", dialog.DialogEvents, error) ||
            !SetValue(*result.Object, "m_noAggroWhileDialogIsUp", dialog.NoAggroWhileDialogIsUp, error) ||
            !SetValue(*result.Object, "m_noAggroNoDelay", dialog.NoAggroNoDelay, error))
            return Failure(std::move(error));
        return result;
    }

    ObjectBuildResult BuildPrepEntry(TypeCatalogPtr const& catalog, PrepEntry const& entry)
    {
        ObjectBuildResult result = Create(catalog, "class PrepEntry");
        if (!result.Ok())
            return result;
        std::string error;
        if (!SetServiceFields(*result.Object, entry, error) ||
            !SetValue(*result.Object, "m_prepText", entry.PrepText, error))
            return Failure(std::move(error));
        return result;
    }

    ObjectBuildResult BuildGoalEntry(TypeCatalogPtr const& catalog, GoalEntry const& entry)
    {
        ObjectBuildResult result = Create(catalog, "class GoalEntry");
        if (!result.Ok())
            return result;
        std::string error;
        if (!SetGoalEntryFields(*result.Object, entry, error))
            return Failure(std::move(error));
        return result;
    }

    ObjectBuildResult BuildGoalEntryFull(TypeCatalogPtr const& catalog, GoalEntryFull const& entry)
    {
        ObjectBuildResult result = Create(catalog, "class GoalEntryFull");
        if (!result.Ok())
            return result;
        std::string error;
        if (!SetGoalEntryFields(*result.Object, entry, error) ||
            !SetValue(*result.Object, "m_personaName", entry.PersonaName, error) ||
            !SetValue(*result.Object, "m_goalType", static_cast<int32>(entry.Type), error) ||
            !SetValue(*result.Object, "m_goalStatus", entry.Status, error) ||
            !SetValue(*result.Object, "m_goalCount", entry.Count, error) ||
            !SetValue(*result.Object, "m_goalTotal", entry.Total, error) ||
            !SetValue(*result.Object, "m_useTally", entry.UseTally, error) ||
            !SetValue(*result.Object, "m_tallyText", entry.TallyText, error) ||
            !SetValue(*result.Object, "m_tallyText2", entry.TallyText2, error) ||
            !SetValue(*result.Object, "m_goalLocation", entry.Location, error) ||
            !SetValue(*result.Object, "m_goalDestinationZone", entry.DestinationZone, error) ||
            !SetValue(*result.Object, "m_goalImage1", entry.Image1, error) ||
            !SetValue(*result.Object, "m_goalImage2", entry.Image2, error) ||
            !SetStringList(*result.Object, "m_clientTags", entry.ClientTags, error))
            return Failure(std::move(error));
        if (entry.Madlibs)
        {
            ObjectBuildResult madlibs = BuildMadlibBlock(catalog, *entry.Madlibs);
            if (!madlibs.Ok())
                return madlibs;
            if (!SetValue(*result.Object, "m_goalMadlibs", std::move(madlibs.Object), error))
                return Failure(std::move(error));
        }
        return result;
    }

    ObjectBuildResult BuildInteractableOption(TypeCatalogPtr const& catalog, InteractableOption const& option)
    {
        ObjectBuildResult result = Create(catalog, "class InteractableOption");
        if (!result.Ok())
            return result;
        std::string error;
        if (!SetServiceFields(*result.Object, option, error) ||
            !SetValue(*result.Object, "m_optionIndex", option.OptionIndex, error))
            return Failure(std::move(error));
        return result;
    }

    ObjectBuildResult BuildServiceMementoBase(TypeCatalogPtr const& catalog, ServiceMementoBase const& memento)
    {
        ObjectBuildResult result = Create(catalog, "class ServiceMementoBase");
        if (!result.Ok())
            return result;
        PropertyValue::List options;
        options.reserve(memento.Options.size());
        for (ServiceOption const& option : memento.Options)
        {
            ObjectBuildResult item = BuildServiceOption(catalog, option);
            if (!item.Ok())
                return item;
            options.emplace_back(std::move(item.Object));
        }
        std::string error;
        if (!Set(*result.Object, "m_serviceOptions", std::move(options), error))
            return Failure(std::move(error));
        if (memento.PersonaMadlibs)
        {
            ObjectBuildResult madlibs = BuildMadlibBlock(catalog, *memento.PersonaMadlibs);
            if (!madlibs.Ok())
                return madlibs;
            if (!SetValue(*result.Object, "m_personaMadlibs", std::move(madlibs.Object), error))
                return Failure(std::move(error));
        }
        if (!SetValue(*result.Object, "m_npcNameKey", memento.NpcNameKey, error) ||
            !SetValue(*result.Object, "m_npcTextKey", memento.NpcTextKey, error) ||
            !SetValue(*result.Object, "m_npcIcon", memento.NpcIcon, error) ||
            !SetValue(*result.Object, "m_npcGreetingSound", memento.NpcGreetingSound, error) ||
            !SetValue(*result.Object, "m_npcFarewellSound", memento.NpcFarewellSound, error) ||
            !SetValue(*result.Object, "m_bTurnPlayerToFace", memento.TurnPlayerToFace, error) ||
            !SetValue(*result.Object, "m_clickToInteractOnly", memento.ClickToInteractOnly, error))
            return Failure(std::move(error));
        return result;
    }

    BlobEncodeResult Encode(PropertyObject const& object, uint32 mask, BlobEnvelope::Packing packing)
    {
        SerializerOptions options;
        options.Mask = mask;
        EncodeResult encoded = ObjectSerializer::Encode(&object, options);
        if (!encoded.Ok())
            return { {}, "ObjectProperty encoding failed: " + std::string(ObjectSerializer::GetStatusName(encoded.Status)) + ": " + encoded.Detail };
        try
        {
            return { BlobEnvelope::Wrap(encoded.Bytes, packing), {} };
        }
        catch (std::length_error const& error)
        {
            return { {}, "SerializerBinary envelope rejected the payload: " + std::string(error.what()) };
        }
        catch (std::bad_alloc const&)
        {
            return { {}, "SerializerBinary envelope could not allocate the payload" };
        }
    }
}
