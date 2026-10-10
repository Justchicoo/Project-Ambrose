/*
 * Project Ambrose by Imjustchico
 * A row of item_instance with its place in character_inventory: the item's own global id, the template it was made from, how many it holds, its color layers, whether it is locked, its flags, when it was made and the backpack slot it arrived in; and an item a wizard wears, the same row with the name of the equipment slot character_equipment puts it in.
 */

#ifndef AMBROSE_CHARACTERITEM_H
#define AMBROSE_CHARACTERITEM_H

#include "Types.h"

#include <string>

struct CharacterItem
{
    uint64 Guid = 0;
    uint32 TemplateId = 0;
    uint32 Quantity = 1;
    uint8 PrimaryColor = 0;
    uint8 SecondaryColor = 0;
    uint8 Pattern = 0;
    bool Locked = false;
    uint32 Flags = 0;
    uint64 Created = 0;
    uint32 Slot = 0;

    bool operator==(CharacterItem const&) const = default;
};

struct CharacterEquippedItem
{
    CharacterItem Item;
    std::string Slot;

    bool operator==(CharacterEquippedItem const&) const = default;
};

#endif
