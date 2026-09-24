/*
 * Project Ambrose by Imjustchico
 * dbimport entry point: creates and updates the login, characters and world databases selected by Updates.EnableDatabases, then exits 0 on success or 1 on any failure.
 */

#include "ConfigMgr.h"
#include "DatabaseEnv.h"
#include "DatabaseLoader.h"
#include "Environment.h"
#include "Log.h"
#include "ServerApp.h"

#include <iostream>
#include <string>
#include <vector>

namespace
{
    class DbImportApp : public ServerApp
    {
    public:
        DbImportApp() : ServerApp({ "dbimport", "dbimport.conf" }, sConfigMgr, sLog, std::cout, std::cerr)
        {
        }

    protected:
        bool OnStart() override
        {
            DatabaseLoader loader(Config());
            loader.AddDatabase(LoginDatabase, "Login", DatabaseLoader::DATABASE_LOGIN)
                .AddDatabase(CharacterDatabase, "Character", DatabaseLoader::DATABASE_CHARACTER)
                .AddDatabase(WorldDatabase, "World", DatabaseLoader::DATABASE_WORLD);
            if (!loader.Load())
            {
                LOG_ERROR("server.dbimport", "Database import failed");
                return false;
            }
            loader.Close();
            LOG_INFO("server.dbimport", "Every enabled database is created and up to date");
            RequestStop();
            return true;
        }
    };
}

int main(int argc, char** argv)
{
    DbImportApp app;
    return app.Run(Ambrose::GetArguments(argc, argv));
}
