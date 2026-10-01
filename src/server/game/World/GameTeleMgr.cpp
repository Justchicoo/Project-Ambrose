/*
 * Project Ambrose by Imjustchico
 * Reads game_tele into a list keyed by the lowercased name, refusing an empty or overlong name, an empty zone and a place or facing that is not a finite number, and swaps the list in only when every row is good; adds and removes a point through WorldEdits, so the database and the journal take it before the live list does.
 */

#include "GameTeleMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ReloadMgr.h"
#include "StringUtil.h"
#include "WorldEdits.h"
#include "WorldSqlScript.h"

#include <fmt/format.h>

#include <cmath>
#include <utility>

GameTeleMgr& GameTeleMgr::Instance()
{
    static GameTeleMgr instance;
    return instance;
}

std::string GameTeleMgr::Key(std::string_view name)
{
    return Ambrose::ToLower(Ambrose::Trim(name));
}

bool GameTeleMgr::Load(std::vector<std::string>& errors)
{
    if (!WorldDatabase.IsOpen())
    {
        errors.push_back("the world database is not open, so the teleport points were not read");
        return false;
    }
    QueryResult rows;
    if (!WorldDatabase.TryQuery("SELECT `name`, `zone`, `x`, `y`, `z`, `yaw` FROM `game_tele`", rows))
    {
        errors.push_back("game_tele could not be read");
        return false;
    }
    std::map<std::string, GameTele, std::less<>> loaded;
    std::size_t const before = errors.size();
    if (rows)
    {
        do
        {
            Field const* row = rows->Fetch();
            GameTele tele{ row[0].Get<std::string>(), row[1].Get<std::string>(), row[2].Get<float>(), row[3].Get<float>(), row[4].Get<float>(), row[5].Get<float>() };
            std::string const key = Key(tele.Name);
            if (key.empty() || tele.Name.size() > MaxNameLength)
                errors.push_back(fmt::format("game_tele row {} has no usable name", Ambrose::ForLog(tele.Name, 80)));
            else if (tele.Zone.empty())
                errors.push_back(fmt::format("game_tele point {} names no zone", tele.Name));
            else if (!std::isfinite(tele.X) || !std::isfinite(tele.Y) || !std::isfinite(tele.Z) || !std::isfinite(tele.Yaw))
                errors.push_back(fmt::format("game_tele point {} has a place or facing that is not a number", tele.Name));
            else if (!loaded.emplace(key, std::move(tele)).second)
                errors.push_back(fmt::format("game_tele names {} twice", key));
        } while (rows->NextRow());
    }
    if (errors.size() != before)
        return false;
    std::size_t const count = loaded.size();
    {
        std::lock_guard const lock(_mutex);
        _byKey = std::move(loaded);
    }
    LOG_INFO("server.world", "Loaded {} teleport point(s) from game_tele", count);
    return true;
}

void GameTeleMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(ReloadTarget), [this](std::vector<std::string>& errors) { return Load(errors); });
}

void GameTeleMgr::Clear()
{
    std::lock_guard const lock(_mutex);
    _byKey.clear();
}

std::optional<GameTele> GameTeleMgr::Find(std::string_view name) const
{
    std::lock_guard const lock(_mutex);
    auto const found = _byKey.find(Key(name));
    if (found == _byKey.end())
        return std::nullopt;
    return found->second;
}

std::size_t GameTeleMgr::Count() const
{
    std::lock_guard const lock(_mutex);
    return _byKey.size();
}

bool GameTeleMgr::Add(GameTele tele, std::string const& who, std::string& error)
{
    tele.Name = std::string(Ambrose::Trim(tele.Name));
    std::string const key = Key(tele.Name);
    if (key.empty() || tele.Name.size() > MaxNameLength)
    {
        error = fmt::format("a teleport point's name holds 1 to {} characters", MaxNameLength);
        return false;
    }
    if (Find(key))
    {
        error = fmt::format("a teleport point named {} already exists; delete it first", tele.Name);
        return false;
    }
    std::string const statement = fmt::format("INSERT INTO `game_tele` (`name`, `zone`, `x`, `y`, `z`, `yaw`) VALUES ({}, {}, {}, {}, {}, {})",
        WorldSqlScript::Literal(tele.Name), WorldSqlScript::Literal(tele.Zone), WorldSqlScript::Literal(double{ tele.X }), WorldSqlScript::Literal(double{ tele.Y }),
        WorldSqlScript::Literal(double{ tele.Z }), WorldSqlScript::Literal(double{ tele.Yaw }));
    if (!WorldEdits::Apply(who, "command", statement, error))
        return false;
    std::lock_guard const lock(_mutex);
    _byKey[key] = std::move(tele);
    return true;
}

bool GameTeleMgr::Remove(std::string_view name, std::string const& who, std::string& error)
{
    std::optional<GameTele> const found = Find(name);
    if (!found)
    {
        error = fmt::format("no teleport point is named {}", name);
        return false;
    }
    if (!WorldEdits::Apply(who, "command", fmt::format("DELETE FROM `game_tele` WHERE `name` = {}", WorldSqlScript::Literal(found->Name)), error))
        return false;
    std::lock_guard const lock(_mutex);
    _byKey.erase(Key(name));
    return true;
}
