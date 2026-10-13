/*
 * Project Ambrose by Imjustchico
 * The checked, reloadable world-hub routes used by Go Home and the Spiral Door: each route carries a server world key and client world name with hub and universe arrival places, every named zone and location is checked against the serving zone tables, and a failed `.reload world_hub` leaves the prior generation in service.
 */

#ifndef AMBROSE_WORLDHUBMGR_H
#define AMBROSE_WORLDHUBMGR_H

#include "Types.h"

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct WorldHub
{
    std::string World;
    std::string ClientWorld;
    std::string HubZone;
    std::string HubLocation;
    std::string UniverseZone;
    std::string UniverseLocation;
};

class WorldHubMgr
{
public:
    static constexpr std::string_view ReloadTarget = "world_hub";
    using Table = std::vector<WorldHub>;

    static WorldHubMgr& Instance();

    WorldHubMgr(WorldHubMgr const&) = delete;
    WorldHubMgr& operator=(WorldHubMgr const&) = delete;

    bool Load(std::vector<std::string>& errors);
    void RegisterReloadTargets();
    void Replace(Table table);
    void Clear();

    std::optional<WorldHub> Find(std::string_view world) const;
    std::optional<WorldHub> FindForClientWorld(std::string_view clientWorld) const;
    static std::optional<WorldHub> FindForClientWorld(Table const& table, std::string_view clientWorld);
    std::optional<WorldHub> FindForUniverseZone(std::string_view zone) const;
    static std::optional<WorldHub> FindForUniverseZone(Table const& table, std::string_view zone);
    static std::vector<WorldHub> FindUnlocked(Table const& table, std::vector<std::string> const& unlockedWorlds);
    std::optional<WorldHub> FindForZone(std::string_view zone) const;
    static std::optional<WorldHub> FindForZone(Table const& table, std::string_view zone);
    std::vector<WorldHub> FindAll(std::string_view world) const;
    std::vector<WorldHub> All() const;
    static bool Validate(Table const& table, std::vector<std::string>& errors);

private:
    WorldHubMgr() = default;

    mutable std::mutex _mutex;
    std::shared_ptr<Table const> _table = std::make_shared<Table const>();
};

#define sWorldHubMgr WorldHubMgr::Instance()

#endif
