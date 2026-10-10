/*
 * Project Ambrose by Imjustchico
 * Reads and checks a radial menu's slots; see EmoteRadialMenu.h.
 */

#include "EmoteRadialMenu.h"

#include <fmt/format.h>

#include <cstring>
#include <set>

namespace
{
    class Reader
    {
    public:
        explicit Reader(std::string_view bytes) : _bytes(bytes) {}

        template<typename T>
        bool Read(T& value)
        {
            if (_bytes.size() - _at < sizeof(T))
                return false;
            std::memcpy(&value, _bytes.data() + _at, sizeof(T));
            _at += sizeof(T);
            return true;
        }

        std::size_t Left() const noexcept { return _bytes.size() - _at; }

    private:
        std::string_view _bytes;
        std::size_t _at = 0;
    };
}

std::optional<std::vector<RadialMenuSlot>> EmoteRadialMenu::Parse(std::string_view blob, std::string& problem)
{
    problem.clear();
    std::vector<RadialMenuSlot> slots;
    if (blob.empty())
        return slots;
    if (blob.size() > MaxBlob)
    {
        problem = fmt::format("it is {} bytes, more than the {} a menu of {} slots takes", blob.size(), MaxBlob, MaxSlots);
        return std::nullopt;
    }
    Reader reader(blob);
    uint32 magic = 0;
    uint32 version = 0;
    if (!reader.Read(magic) || magic != Magic)
    {
        problem = "it does not open with the radial menu's magic 0xFACECEE8";
        return std::nullopt;
    }
    if (!reader.Read(version) || version >= Versions)
    {
        problem = fmt::format("its version is {}, and the client reads versions below {}", version, Versions);
        return std::nullopt;
    }
    uint32 extra = 0;
    uint16 count = 0;
    if ((version >= 2 && !reader.Read(extra)) || !reader.Read(count))
    {
        problem = "it ends before its count of slots";
        return std::nullopt;
    }
    if (count > MaxSlots)
    {
        problem = fmt::format("it holds {} slots, more than {}", count, MaxSlots);
        return std::nullopt;
    }
    std::set<uint16> seen;
    for (uint16 index = 0; index < count; ++index)
    {
        RadialMenuSlot slot;
        if (!reader.Read(slot.Slot) || !reader.Read(slot.EmoteId) || (version >= 1 && !reader.Read(slot.TemplateId)) || (version == 3 && !reader.Read(slot.ChatPhraseId)))
        {
            problem = fmt::format("it ends inside slot {} of {}", index + 1, count);
            return std::nullopt;
        }
        if (!seen.insert(slot.Slot).second)
        {
            problem = fmt::format("it fills slot {} twice", slot.Slot);
            return std::nullopt;
        }
        slots.push_back(slot);
    }
    if (reader.Left() != 0)
    {
        problem = fmt::format("{} byte(s) follow its last slot", reader.Left());
        return std::nullopt;
    }
    return slots;
}

bool EmoteRadialMenu::CheckSlots(std::span<RadialMenuSlot const> slots, std::function<std::optional<uint32>(uint64)> const& heldTemplate,
    std::function<bool(uint32)> const& isEmoteTemplate, std::string& problem)
{
    problem.clear();
    for (RadialMenuSlot const& slot : slots)
    {
        if (slot.EmoteId == 0)
            continue;
        std::optional<uint32> const held = heldTemplate(slot.EmoteId);
        if (!held)
        {
            problem = fmt::format("slot {} names item {}, which the wizard does not hold", slot.Slot, slot.EmoteId);
            return false;
        }
        if (!isEmoteTemplate(*held))
        {
            problem = fmt::format("slot {} names item {} of template {}, which is not an emote", slot.Slot, slot.EmoteId, *held);
            return false;
        }
        if (slot.TemplateId != 0 && slot.TemplateId != *held)
        {
            problem = fmt::format("slot {} names item {} as template {}, but it is of template {}", slot.Slot, slot.EmoteId, slot.TemplateId, *held);
            return false;
        }
    }
    return true;
}
