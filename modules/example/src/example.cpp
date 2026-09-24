/*
 * Project Ambrose by Imjustchico
 * The module that proves a module needs no edit to the core: it lives entirely under modules/, CMake finds its loader the same way it finds a script's, and it says once at startup that it was loaded. Delete the folder and it is gone from the build at the next configure; copy it and rename the loader to start one of your own.
 */

#include "Log.h"
#include "ScriptMgr.h"

namespace
{
    class ExampleModule : public WorldScript
    {
    public:
        ExampleModule() : WorldScript("example_module") {}

        void OnStartup() override
        {
            LOG_INFO("server.modules", "The example module is loaded; it lives entirely under modules/example");
        }
    };
}

void Addmodules_example()
{
    new ExampleModule();
}
