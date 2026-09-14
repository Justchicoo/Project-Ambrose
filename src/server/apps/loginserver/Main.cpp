/*
 * Project Ambrose by Imjustchico
 * Login server entry point: runs the shared app lifecycle until a shutdown signal.
 */

#include "ConfigMgr.h"
#include "Log.h"
#include "ServerApp.h"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    ServerApp app({ "loginserver", "loginserver.conf" }, sConfigMgr, sLog, std::cout, std::cerr);
    return app.Run(std::vector<std::string>(argv, argv + argc));
}
