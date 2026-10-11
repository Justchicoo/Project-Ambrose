/*
 * Project Ambrose by Imjustchico
 * The two ids an object in the world carries. A runtime GID names one object for as long as this process runs and is handed out from a line of its own that starts far above every id this server stores, so it can never be mistaken for a wizard's id; an item's GID is stored with the item and handed out from one line for the whole server, which starts above every wizard's id and below every runtime GID and resumes above the highest item id the characters database has ever used; a quest or goal GID a wizard's quest log keeps for life comes the same way from a line of its own, above every item id and below every runtime GID; a permID names the same placed object across every restart, because it is worked out from the zone, the template and the id the zone's own data gives the object, none of which a restart or a fresh extraction changes.
 */

#ifndef AMBROSE_OBJECTGUID_H
#define AMBROSE_OBJECTGUID_H

#include "GuidGenerator.h"
#include "Types.h"

#include <optional>
#include <string_view>

namespace ObjectGuid
{
    inline constexpr uint64 RuntimeBase = uint64{ 1 } << 52;
    inline constexpr uint64 ItemBase = uint64{ 1 } << 48;
    inline constexpr uint64 QuestBase = uint64{ 1 } << 50;

    uint64 PermId(std::string_view zonePath, uint64 templateId, uint32 objectId) noexcept;
    GuidGenerator& RuntimeGuids() noexcept;
    std::optional<uint64> NextRuntime() noexcept;
    bool IsRuntime(uint64 guid) noexcept;
    GuidGenerator& ItemGuids() noexcept;
    bool IsItem(uint64 guid) noexcept;
    GuidGenerator& QuestGuids() noexcept;
    bool IsQuest(uint64 guid) noexcept;
}

#endif
