/*
 * Project Ambrose by Imjustchico
 * Represents an item instance in the game world: the item's own global id,
 * owner id, template it was made from, quantity, color layers, whether it is locked,
 * its flags, and when it was made.
 */

#ifndef AMBROSE_ITEMINSTANCE_H
#define AMBROSE_ITEMINSTANCE_H

#include "Types.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

struct ItemInstance
{
    uint64 guid = 0;        // item_instance guid
    uint64 owner = 0;       // owner guid (character or account)
    uint32 templateId = 0;  // item template id
    uint32 quantity = 1;    // stack size
    uint32 primaryColor = 0; // primary color id
    uint32 secondaryColor = 0; // secondary color id
    uint32 pattern = 0;     // pattern id
    bool locked = false;    // whether the item is locked
    uint32 flags = 0;       // item flags
    uint64 created = 0;     // creation timestamp

    bool operator==(ItemInstance const&) const = default;
};

#endif