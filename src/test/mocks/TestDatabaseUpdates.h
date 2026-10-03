/*
 * Project Ambrose by Imjustchico
 * Applies released world updates and opts test databases into pending world updates when the pending folder exists.
 */

#ifndef AMBROSE_TESTDATABASEUPDATES_H
#define AMBROSE_TESTDATABASEUPDATES_H

#include "DBUpdater.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <system_error>

namespace AmbroseTestDatabase
{
    inline ::testing::AssertionResult RunWorldUpdates(MySQLConnectionInfo const& info)
    {
        UpdaterSettings settings;
        if (!DBUpdater::Run(info, "world", settings))
            return ::testing::AssertionFailure() << "released world updates could not be applied";

        std::filesystem::path const pendingFolder = DBUpdater::SourceDirectoryFor(settings) / "data" / "sql" / "updates" / "pending_db_world";
        std::error_code error;
        if (!std::filesystem::is_directory(pendingFolder, error))
        {
            if (error)
                return ::testing::AssertionFailure() << "could not inspect pending world updates: " << error.message();
            return ::testing::AssertionSuccess();
        }

        MySQLConnection setup(info);
        uint32 const openResult = setup.Open();
        if (openResult != 0)
            return ::testing::AssertionFailure() << "could not open world database to enable pending updates: " << openResult;
        bool const registered = setup.Execute("INSERT IGNORE INTO `updates_include` (`path`, `state`) VALUES ('$/data/sql/updates/pending_db_world', 'PENDING')");
        setup.Close();
        if (!registered)
            return ::testing::AssertionFailure() << "could not enable pending world updates";

        settings.AllowPending = true;
        if (!DBUpdater::Run(info, "world", settings))
            return ::testing::AssertionFailure() << "pending world updates could not be applied";
        return ::testing::AssertionSuccess();
    }
}

#endif
