/*
 * Project Ambrose by Imjustchico
 * The named places a game master teleports to, from world.game_tele: read at start and again by `.reload game_tele`, which keeps the list it had and reports every bad row when a row is wrong, and changed live by '.tele add' and '.tele del', which write the table through the world edit journal first, so a point works at once and is exported with the other edits.
 */

#ifndef AMBROSE_GAMETELEMGR_H
#define AMBROSE_GAMETELEMGR_H

#include "Types.h"

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct GameTele
{
    std::string Name;
    std::string Zone;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float Yaw = 0.0f;
};

class GameTeleMgr
{
public:
    static constexpr std::string_view ReloadTarget = "game_tele";
    static constexpr std::size_t MaxNameLength = 64;

    static GameTeleMgr& Instance();

    GameTeleMgr(GameTeleMgr const&) = delete;
    GameTeleMgr& operator=(GameTeleMgr const&) = delete;

    bool Load(std::vector<std::string>& errors);
    void RegisterReloadTargets();
    void Clear();

    std::optional<GameTele> Find(std::string_view name) const;
    std::size_t Count() const;
    bool Add(GameTele tele, std::string const& who, std::string& error);
    bool Remove(std::string_view name, std::string const& who, std::string& error);

    static std::string Key(std::string_view name);

private:
    GameTeleMgr() = default;

    mutable std::mutex _mutex;
    std::map<std::string, GameTele, std::less<>> _byKey;
};

#define sGameTeleMgr GameTeleMgr::Instance()

#endif
