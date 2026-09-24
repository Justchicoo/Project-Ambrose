/*
 * Project Ambrose by Imjustchico
 * Field offsets of the client engine's Type, PropertyList, Property, container and enum option objects and of the MSVC runtime's std::string and std::map node, as found in r801440 and r806919, which the extraction reads and its validation checks.
 */

#ifndef AMBROSE_CLIENTLAYOUT_H
#define AMBROSE_CLIENTLAYOUT_H

#include "Types.h"

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
};

#endif
