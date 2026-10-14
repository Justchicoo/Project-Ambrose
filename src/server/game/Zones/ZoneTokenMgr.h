/*
 * Project Ambrose by Imjustchico
 * Every zone's tokens from the token containers zone_object holds for its ZoneTokenInfo placements (sZoneTokenMgr), read at start and again by `.reload zone_token`, which builds the new set off to the side and swaps it in only when every token reads, keeping the old set and reporting each error otherwise; and the states of the tokens each zone instance keeps for all its wizards, until its map is forgotten.
 */

#ifndef AMBROSE_ZONETOKENMGR_H
#define AMBROSE_ZONETOKENMGR_H

#include "TypeRegistry.h"
#include "ZoneTokens.h"

#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class ZoneTokenMgr
{
public:
    static constexpr std::string_view ReloadTarget = "zone_token";
    static constexpr std::string_view EnableResultClass = "class ResZoneTokenEnable";
    static constexpr std::string_view DisableResultClass = "class ResZoneTokenDisable";
    static constexpr std::string_view ModifyResultClass = "class ResZoneTokenModify";

    static ZoneTokenMgr& Instance();
    static std::vector<ZoneTokenDefinition> Read(TypeCatalogPtr const& catalog, std::span<uint8 const> container, uint32 objectId, std::string& error);

    ZoneTokenMgr(ZoneTokenMgr const&) = delete;
    ZoneTokenMgr& operator=(ZoneTokenMgr const&) = delete;

    bool Load(std::vector<std::string>& errors);
    void RegisterReloadTargets();
    void Replace(std::map<std::string, ZoneTokenSet, std::less<>> zones);
    void Clear();

    std::shared_ptr<ZoneTokenSet const> Find(std::string_view zone) const;
    ZoneTokenState GetInstanceState(uint32 mapId, ZoneTokenDefinition const& token) const;
    uint64 GetInstanceChangeCount(uint32 mapId) const;
    bool EnableInInstance(uint32 mapId, ZoneTokenDefinition const& token);
    bool DisableInInstance(uint32 mapId, ZoneTokenDefinition const& token);
    bool ModifyInInstance(uint32 mapId, ZoneTokenDefinition const& token, int32 delta);
    void ForgetMap(uint32 mapId);

private:
    ZoneTokenMgr() = default;

    mutable std::mutex _mutex;
    std::map<std::string, std::shared_ptr<ZoneTokenSet const>, std::less<>> _zones;
    std::map<uint32, ZoneTokenStates> _instances;
};

#define sZoneTokenMgr ZoneTokenMgr::Instance()

#endif
