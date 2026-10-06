/*
 * Project Ambrose by Imjustchico
 * The server home the panel program hosts a game from on this computer: the supervisor it finds beside the program as a build tree or the server component lays it out, or the one Host.Supervisor names, and a folder in the Ambrose data folder holding the configuration, conf.d and the logs. Each start copies the running build's .conf.dist files in and makes each .conf from its .conf.dist only when it is missing, never over an edited one, and writes one file of the program's own in conf.d that turns the panel on at 127.0.0.1 on a port it remembers, 12080 when free and otherwise the next free one. With no server programs to be found the program runs connect-only, and says which package carries them.
 */

#ifndef AMBROSE_HOSTHOME_H
#define AMBROSE_HOSTHOME_H

#include "Types.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

struct HostPrepared
{
    std::filesystem::path Home = {};
    std::filesystem::path Config = {};
    uint16 PanelPort = 0;
    std::vector<std::string> Made = {};
};

class HostHome
{
public:
    using PortFree = std::function<bool(uint16 port)>;

    static constexpr char const* FolderName = "Server";
    static constexpr char const* OwnFile = "panel-program.conf";
    static constexpr uint16 FirstPanelPort = 12080;
    static constexpr uint16 PortsTried = 100;
    static constexpr std::string_view ConnectOnly = "No Ambrose server programs are beside this program and Host.Supervisor names none, so it opens panels on other machines only; the server component of the Ambrose package carries them";

    HostHome() = delete;

    static std::optional<std::filesystem::path> FindSupervisor(std::filesystem::path const& programFolder, std::filesystem::path const& configured);
    static std::optional<HostPrepared> Prepare(std::filesystem::path const& dataFolder, std::filesystem::path const& supervisor, PortFree free, std::string& error);
    static std::optional<uint16> RememberedPort(std::filesystem::path const& ownFile);
};

#endif
