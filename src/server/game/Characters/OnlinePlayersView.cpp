/*
 * Project Ambrose by Imjustchico
 * Reads the online rows from the login database and fills each one in from the characters database, which is a second read rather than a join because the two are separate databases whose names the operator chooses. A wizard that cannot be found is still returned with what is known of it, and the age of a session is worked out from the moment it started rather than stored, so a page that sits open does not have to be told the time again.
 */

#include "OnlinePlayersView.h"
#include "AdminRouter.h"
#include "CharacterRepository.h"
#include "CharacterSummary.h"
#include "DatabaseEnv.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <memory>
#include <utility>

namespace
{
    int64 NowSeconds()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
}

std::vector<OnlinePlayer> OnlinePlayersView::Read()
{
    std::vector<OnlinePlayer> players;
    if (!LoginDatabase.IsOpen())
        return players;
    std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> statement = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ONLINE_PLAYERS);
    if (!statement)
        return players;
    PreparedQueryResult result = LoginDatabase.Query(*statement);
    if (!result)
        return players;

    do
    {
        Field const* row = result->Fetch();
        OnlinePlayer player;
        player.CharacterGuid = row[0].Get<uint64>();
        player.AccountId = row[1].Get<uint64>();
        player.RealmId = row[2].Get<uint32>();
        player.SinceEpochSeconds = static_cast<int64>(row[3].Get<uint64>());
        player.Account = row[4].IsNull() ? std::string() : row[4].Get<std::string>();
        player.Realm = row[5].IsNull() ? std::string() : row[5].Get<std::string>();
        players.push_back(std::move(player));
    } while (result->NextRow());

    for (OnlinePlayer& player : players)
    {
        CharacterLoad const loaded = CharacterRepository::Load(player.CharacterGuid);
        if (!loaded.Character)
            continue;
        player.Found = true;
        player.Name = loaded.Character->CustomName.value_or(std::string());
        player.Zone = loaded.Character->ZoneDisplay.empty() ? loaded.Character->Zone : loaded.Character->ZoneDisplay;
        player.Level = static_cast<uint32>(loaded.Character->Level);
        player.SchoolId = loaded.Character->SchoolId;
    }
    return players;
}

std::string OnlinePlayersView::PlayersJson(std::vector<OnlinePlayer> const& players, int64 nowEpochSeconds)
{
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    nlohmann::json rows = nlohmann::json::array();
    for (OnlinePlayer const& player : players)
    {
        nlohmann::json row;
        row["character_guid"] = player.CharacterGuid;
        row["account_id"] = player.AccountId;
        row["realm_id"] = player.RealmId;
        row["account"] = player.Account;
        row["realm"] = player.Realm;
        row["name"] = player.Name;
        row["zone"] = player.Zone;
        row["level"] = player.Level;
        row["school_id"] = player.SchoolId;
        row["found"] = player.Found;
        row["since_epoch_seconds"] = player.SinceEpochSeconds;
        row["seconds"] = player.SinceEpochSeconds > 0 && nowEpochSeconds > player.SinceEpochSeconds
            ? nowEpochSeconds - player.SinceEpochSeconds
            : 0;
        rows.push_back(std::move(row));
    }
    body["players"] = std::move(rows);
    body["counted"] = players.size();
    return body.dump();
}

void OnlinePlayersView::Register(AdminRouter& router)
{
    router.AddGuarded("GET", "/api/players", "players.read", [](AdminRequest const&)
    {
        return AdminResponse::Json(200, PlayersJson(Read(), NowSeconds()));
    });
}
