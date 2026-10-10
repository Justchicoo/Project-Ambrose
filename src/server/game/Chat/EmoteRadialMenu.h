/*
 * Project Ambrose by Imjustchico
 * Reads a radial menu's slots from the blob the client keeps in EmotesRadialMenuBehavior, as its own EmotesRadialMenuBehavior::LoadRadialMenuData does: the magic 0xFACECEE8, a version below 4, a word more from version 2, a count, and for each slot its number, the global id of the emote item it plays, from version 1 the item's template id and in version 3 the id of a chat phrase; and answers whether every emote slot names an emote item the wizard holds, with that item's template, so a wizard cannot keep an emote it does not own.
 */

#ifndef AMBROSE_EMOTERADIALMENU_H
#define AMBROSE_EMOTERADIALMENU_H

#include "Types.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct RadialMenuSlot
{
    uint16 Slot = 0;
    uint64 EmoteId = 0;
    uint32 TemplateId = 0;
    uint32 ChatPhraseId = 0;

    bool operator==(RadialMenuSlot const&) const = default;
};

namespace EmoteRadialMenu
{
    constexpr uint32 Magic = 0xFACECEE8u;
    constexpr uint32 Versions = 4;
    constexpr std::size_t MenuCount = 3;
    constexpr std::size_t MaxSlots = 64;
    constexpr std::size_t MaxBlob = 4 * 4 + 2 + MaxSlots * (2 + 8 + 4 + 4);

    std::optional<std::vector<RadialMenuSlot>> Parse(std::string_view blob, std::string& problem);

    bool CheckSlots(std::span<RadialMenuSlot const> slots, std::function<std::optional<uint32>(uint64)> const& heldTemplate,
        std::function<bool(uint32)> const& isEmoteTemplate, std::string& problem);
}

#endif
