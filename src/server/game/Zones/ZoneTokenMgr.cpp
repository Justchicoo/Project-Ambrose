/*
 * Project Ambrose by Imjustchico
 * Reads each ZoneTokenInfo placement's token container through the type dump, refusing a container that does not decode, a token that does not read and a name a zone holds twice, swaps a whole good set in at once, dropping every instance's token states with it, and keeps each zone instance's states until its map is forgotten.
 */

#include "ZoneTokenMgr.h"
#include "DatabaseEnv.h"
#include "ReloadMgr.h"
#include "ResultMgr.h"

#include <fmt/format.h>

#include <utility>

ZoneTokenMgr& ZoneTokenMgr::Instance()
{
    static ZoneTokenMgr instance;
    return instance;
}

std::vector<ZoneTokenDefinition> ZoneTokenMgr::Read(TypeCatalogPtr const& catalog, std::span<uint8 const> container, uint32 objectId, std::string& error)
{
    std::vector<ZoneTokenDefinition> tokens;
    std::shared_ptr<PropertyObject const> const decoded = ResultMgr::Decode(catalog, container, error);
    if (!decoded)
        return tokens;
    PropertyValue const* const list = decoded->Get("m_tokens");
    PropertyValue::List const* const entries = list ? list->GetList() : nullptr;
    if (!entries)
    {
        error = fmt::format("it holds a {} with no m_tokens list", decoded->GetClass().Name);
        return tokens;
    }
    for (PropertyValue const& entry : *entries)
    {
        PropertyObject const* const token = entry.AsObject();
        std::optional<ZoneTokenDefinition> definition = ZoneTokenStates::DefinitionOf(token ? std::shared_ptr<PropertyObject const>(decoded, token) : nullptr, objectId, error);
        if (!definition)
        {
            tokens.clear();
            return tokens;
        }
        tokens.push_back(std::move(*definition));
    }
    return tokens;
}

bool ZoneTokenMgr::Load(std::vector<std::string>& errors)
{
    if (!WorldDatabase.IsOpen())
    {
        errors.push_back("the world database is not open, so no zone tokens were read");
        return false;
    }
    QueryResult rows;
    if (!WorldDatabase.TryQuery("SELECT `zone_path`, `object_id`, `token_container` FROM `zone_object` WHERE `token_container` IS NOT NULL ORDER BY `zone_path`, `id`", rows))
    {
        errors.push_back("zone_object's token containers could not be read");
        return false;
    }
    std::size_t const before = errors.size();
    std::map<std::string, ZoneTokenSet, std::less<>> zones;
    if (rows)
    {
        TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
        do
        {
            Field const* row = rows->Fetch();
            std::string const zone = row[0].Get<std::string>();
            uint32 const objectId = row[1].Get<uint32>();
            std::string error;
            std::vector<ZoneTokenDefinition> tokens = Read(catalog, row[2].Get<std::vector<uint8>>(), objectId, error);
            if (!error.empty())
            {
                errors.push_back(fmt::format("{} object {} has a token container that does not read: {}", zone, objectId, error));
                continue;
            }
            ZoneTokenSet& held = zones[zone];
            for (ZoneTokenDefinition& token : tokens)
            {
                if (held.Find(token.Name))
                    errors.push_back(fmt::format("{} holds the token {} twice", zone, token.Name));
                else
                    held.Tokens.push_back(std::move(token));
            }
        } while (rows->NextRow());
    }
    if (errors.size() != before)
        return false;
    Replace(std::move(zones));
    return true;
}

void ZoneTokenMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(ReloadTarget), [this](std::vector<std::string>& errors) { return Load(errors); });
}

void ZoneTokenMgr::Replace(std::map<std::string, ZoneTokenSet, std::less<>> zones)
{
    std::map<std::string, std::shared_ptr<ZoneTokenSet const>, std::less<>> shared;
    for (auto& [zone, tokens] : zones)
        shared.emplace(zone, std::make_shared<ZoneTokenSet const>(std::move(tokens)));
    std::lock_guard const lock(_mutex);
    _zones = std::move(shared);
    _instances.clear();
}

void ZoneTokenMgr::Clear()
{
    std::lock_guard const lock(_mutex);
    _zones.clear();
    _instances.clear();
}

std::shared_ptr<ZoneTokenSet const> ZoneTokenMgr::Find(std::string_view zone) const
{
    std::lock_guard const lock(_mutex);
    auto const found = _zones.find(zone);
    return found == _zones.end() ? nullptr : found->second;
}

ZoneTokenState ZoneTokenMgr::GetInstanceState(uint32 mapId, ZoneTokenDefinition const& token) const
{
    std::lock_guard const lock(_mutex);
    auto const found = _instances.find(mapId);
    return found == _instances.end() ? ZoneTokenStates().Get(token) : found->second.Get(token);
}

uint64 ZoneTokenMgr::GetInstanceChangeCount(uint32 mapId) const
{
    std::lock_guard const lock(_mutex);
    auto const found = _instances.find(mapId);
    return found == _instances.end() ? 0 : found->second.GetChangeCount();
}

bool ZoneTokenMgr::EnableInInstance(uint32 mapId, ZoneTokenDefinition const& token)
{
    std::lock_guard const lock(_mutex);
    return _instances[mapId].Enable(token);
}

bool ZoneTokenMgr::DisableInInstance(uint32 mapId, ZoneTokenDefinition const& token)
{
    std::lock_guard const lock(_mutex);
    return _instances[mapId].Disable(token);
}

bool ZoneTokenMgr::ModifyInInstance(uint32 mapId, ZoneTokenDefinition const& token, int32 delta)
{
    std::lock_guard const lock(_mutex);
    return _instances[mapId].Modify(token, delta);
}

void ZoneTokenMgr::ForgetMap(uint32 mapId)
{
    std::lock_guard const lock(_mutex);
    _instances.erase(mapId);
}
