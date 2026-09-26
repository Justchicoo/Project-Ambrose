/*
 * Project Ambrose by Imjustchico
 * Default client engine and MSVC runtime layout values, with per-field evidence recording which offsets runtime probes derive and which remain assumed.
 */

#ifndef AMBROSE_CLIENTLAYOUT_H
#define AMBROSE_CLIENTLAYOUT_H

#include "Types.h"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct ClientLayoutEvidence
{
    std::string Field;
    uint64 Value = 0;
    std::string Status;
    std::string ConfirmedBy;
};

struct ClientLayout
{
    uint64 StringSize = 0x10;
    uint64 StringCapacity = 0x18;
    uint64 StringInlineCapacity = 15;
    uint64 StringObjectSize = 0x20;

    uint64 MapNodeLeft = 0x00;
    uint64 MapNodeParent = 0x08;
    uint64 MapNodeRight = 0x10;
    uint64 MapNodeColor = 0x18;
    uint64 MapNodeIsNil = 0x19;
    uint64 MapNodeKey = 0x20;
    uint64 MapNodeValue = 0x28;

    uint64 TypeName = 0x38;
    uint64 TypeHash = 0x58;
    uint64 TypePointer = 0x88;
    uint64 TypePropertyList = 0x90;

    uint64 ListSingleton = 0x09;
    uint64 ListBase = 0x18;
    uint64 ListProperties = 0x58;
    uint64 ListName = 0xB8;
    uint64 ListEntrySize = 0x10;

    uint64 PropertyContainer = 0x40;
    uint64 PropertyId = 0x50;
    uint64 PropertyName = 0x58;
    uint64 PropertyHash = 0x64;
    uint64 PropertyOffset = 0x68;
    uint64 PropertyType = 0x70;
    uint64 PropertyFlags = 0x80;
    uint64 PropertyOptions = 0x98;

    uint64 OptionSize = 0x48;
    uint64 OptionValue = 0x00;
    uint64 OptionName = 0x28;

    uint64 ContainerNameSlot = 1;
    uint64 ContainerDynamicSlot = 4;

    std::map<std::string, std::string> DerivedFields;

    void ConfirmDerived(std::string field, std::string confirmedBy)
    {
        DerivedFields.insert_or_assign(std::move(field), std::move(confirmedBy));
    }

    std::vector<ClientLayoutEvidence> Evidence() const
    {
        std::vector<ClientLayoutEvidence> evidence = {
            { "std::string.size", StringSize, "assumed", "MSVC x64 std::string reference layout" },
            { "std::string.capacity", StringCapacity, "assumed", "MSVC x64 std::string reference layout" },
            { "std::string.inline_capacity", StringInlineCapacity, "assumed", "MSVC x64 std::string reference layout" },
            { "std::string.object_size", StringObjectSize, "assumed", "MSVC x64 std::string reference layout" },
            { "std::map.node.left", MapNodeLeft, "assumed", "MSVC x64 std::map reference layout" },
            { "std::map.node.parent", MapNodeParent, "assumed", "MSVC x64 std::map reference layout" },
            { "std::map.node.right", MapNodeRight, "assumed", "MSVC x64 std::map reference layout" },
            { "std::map.node.color", MapNodeColor, "assumed", "MSVC x64 std::map reference layout" },
            { "std::map.node.is_nil", MapNodeIsNil, "assumed", "MSVC x64 std::map reference layout" },
            { "std::map.node.key", MapNodeKey, "assumed", "MSVC x64 std::map reference layout" },
            { "std::map.node.value", MapNodeValue, "assumed", "MSVC x64 std::map reference layout" },
            { "Type.name", TypeName, "assumed", "r801440/r806919 reference layout" },
            { "Type.hash", TypeHash, "assumed", "r801440/r806919 reference layout" },
            { "Type.pointer", TypePointer, "assumed", "r801440/r806919 reference layout" },
            { "Type.property_list", TypePropertyList, "assumed", "r801440/r806919 reference layout" },
            { "PropertyList.singleton", ListSingleton, "assumed", "r801440/r806919 reference layout" },
            { "PropertyList.base", ListBase, "assumed", "r801440/r806919 reference layout" },
            { "PropertyList.properties", ListProperties, "assumed", "r801440/r806919 reference layout" },
            { "PropertyList.name", ListName, "assumed", "r801440/r806919 reference layout" },
            { "PropertyList.entry_size", ListEntrySize, "assumed", "r801440/r806919 reference layout" },
            { "Property.container", PropertyContainer, "assumed", "r801440/r806919 reference layout" },
            { "Property.id", PropertyId, "assumed", "r801440/r806919 reference layout" },
            { "Property.name", PropertyName, "assumed", "r801440/r806919 reference layout" },
            { "Property.hash", PropertyHash, "assumed", "r801440/r806919 reference layout" },
            { "Property.offset", PropertyOffset, "assumed", "r801440/r806919 reference layout" },
            { "Property.type", PropertyType, "assumed", "r801440/r806919 reference layout" },
            { "Property.flags", PropertyFlags, "assumed", "r801440/r806919 reference layout" },
            { "Property.options", PropertyOptions, "assumed", "r801440/r806919 reference layout" },
            { "EnumOption.size", OptionSize, "assumed", "r801440/r806919 reference layout" },
            { "EnumOption.value", OptionValue, "assumed", "r801440/r806919 reference layout" },
            { "EnumOption.name", OptionName, "assumed", "r801440/r806919 reference layout" },
            { "Container.name_slot", ContainerNameSlot, "assumed", "r801440/r806919 reference layout" },
            { "Container.dynamic_slot", ContainerDynamicSlot, "assumed", "r801440/r806919 reference layout" }
        };
        for (ClientLayoutEvidence& item : evidence)
            if (auto const found = DerivedFields.find(item.Field); found != DerivedFields.end())
            {
                item.Status = "derived";
                item.ConfirmedBy = found->second;
            }
        return evidence;
    }

    std::optional<std::string> FirstUnresolvedField() const
    {
        for (ClientLayoutEvidence const& item : Evidence())
            if (item.Status != "derived")
                return item.Field;
        return std::nullopt;
    }
};

#endif
