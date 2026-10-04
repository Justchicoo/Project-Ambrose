/*
 * Project Ambrose by Imjustchico
 * Applies the pending friends schema to an isolated character database used by integration tests.
 */

#ifndef AMBROSE_PENDINGCHARACTERUPDATES_H
#define AMBROSE_PENDINGCHARACTERUPDATES_H

#include "DBUpdater.h"

#include <fstream>
#include <iterator>
#include <string>

namespace AmbroseTestDatabaseUpdates
{
    inline bool ApplyPendingFriendsUpdate(MySQLConnectionInfo const& info, std::string& failure)
    {
        std::filesystem::path const update = DBUpdater::GetBuiltInSourceDirectory() / "data" / "sql" / "updates" / "pending_db_characters" /
            "rev_1790786233_friends.sql";
        std::ifstream sqlFile(update, std::ios::binary);
        if (!sqlFile)
        {
            failure = "Could not read " + update.string();
            return false;
        }
        std::string const sql{ std::istreambuf_iterator<char>(sqlFile), std::istreambuf_iterator<char>() };
        return DBUpdater::ApplyScript(info, {}, update.filename().string(), sql, &failure);
    }
}

#endif
