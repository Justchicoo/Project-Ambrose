/*
 * Project Ambrose by Imjustchico
 * Patch server entry point: runs the shared app lifecycle until a shutdown signal.
 */

#include "ConfigMgr.h"
#include "Environment.h"
#include "Log.h"
#include "ServerApp.h"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    ServerApp app({ "patchserver", "patchserver.conf" }, sConfigMgr, sLog, std::cout, std::cerr);
    return app.Run(Ambrose::GetArguments(argc, argv));
}
