/*
 * Project Ambrose by Imjustchico
 * Finds the supervisor by its file name, lays the server home out, refreshes every .conf.dist from the folder the supervisor was built into, creates a .conf only where none is, and keeps the panel's port in the program's own conf.d file: a remembered port that is still free stays, a taken one moves to the next free one, and the choice is written back.
 */

#include "HostHome.h"

#include "ConfigMgr.h"

#include <fmt/format.h>

#include <fstream>
#include <regex>
#include <sstream>
#include <system_error>

namespace
{
    std::filesystem::path SupervisorName()
    {
#ifdef _WIN32
        return "supervisor.exe";
#else
        return "supervisor";
#endif
    }

    bool IsFile(std::filesystem::path const& file)
    {
        std::error_code code;
        return std::filesystem::is_regular_file(file, code);
    }
}

std::optional<std::filesystem::path> HostHome::FindSupervisor(std::filesystem::path const& programFolder, std::filesystem::path const& configured)
{
    if (!configured.empty())
        return IsFile(configured) ? std::optional<std::filesystem::path>(configured) : std::nullopt;
    std::filesystem::path const beside = programFolder / SupervisorName();
    return IsFile(beside) ? std::optional<std::filesystem::path>(beside) : std::nullopt;
}

std::optional<uint16> HostHome::RememberedPort(std::filesystem::path const& ownFile)
{
    std::ifstream held(ownFile, std::ios::binary);
    if (!held)
        return std::nullopt;
    std::ostringstream text;
    text << held.rdbuf();
    std::smatch found;
    std::string const contents = text.str();
    if (!std::regex_search(contents, found, std::regex(R"(Panel\.Port\s*=\s*(\d+))")))
        return std::nullopt;
    unsigned long const port = std::stoul(found[1].str());
    if (port == 0 || port > 65535)
        return std::nullopt;
    return static_cast<uint16>(port);
}

std::optional<HostPrepared> HostHome::Prepare(std::filesystem::path const& dataFolder, std::filesystem::path const& supervisor, PortFree free, std::string& error)
{
    HostPrepared prepared;
    prepared.Home = dataFolder / FolderName;
    std::filesystem::path const confD = prepared.Home / "conf.d";
    std::error_code code;
    std::filesystem::create_directories(confD, code);
    std::filesystem::create_directories(prepared.Home / "logs", code);
    if (code)
    {
        error = fmt::format("the server home {} could not be made: {}", ConfigMgr::PathToUtf8(prepared.Home), code.message());
        return std::nullopt;
    }

    std::filesystem::path const built = supervisor.parent_path();
    for (std::filesystem::directory_iterator it(built, code), end; !code && it != end; it.increment(code))
    {
        std::string const name = ConfigMgr::PathToUtf8(it->path().filename());
        if (!name.ends_with(".conf.dist") || !it->is_regular_file(code))
            continue;
        std::filesystem::path const dist = prepared.Home / it->path().filename();
        std::filesystem::copy_file(it->path(), dist, std::filesystem::copy_options::overwrite_existing, code);
        if (code)
        {
            error = fmt::format("{} could not be copied into the server home: {}", name, code.message());
            return std::nullopt;
        }
        std::filesystem::path const conf = prepared.Home / name.substr(0, name.size() - std::string_view(".dist").size());
        if (!IsFile(conf))
        {
            std::filesystem::copy_file(dist, conf, code);
            if (code)
            {
                error = fmt::format("{} could not be made: {}", ConfigMgr::PathToUtf8(conf), code.message());
                return std::nullopt;
            }
            prepared.Made.push_back(ConfigMgr::PathToUtf8(conf.filename()));
        }
    }
    prepared.Config = prepared.Home / "supervisor.conf";

    std::filesystem::path const own = confD / OwnFile;
    uint16 port = RememberedPort(own).value_or(FirstPanelPort);
    uint16 tried = 0;
    while (free && !free(port) && tried < PortsTried)
    {
        ++port;
        ++tried;
    }
    if (tried == PortsTried)
    {
        error = fmt::format("none of the {} ports from {} is free for the panel", PortsTried, port - PortsTried);
        return std::nullopt;
    }
    prepared.PanelPort = port;
    std::ofstream writing(own, std::ios::binary | std::ios::trunc);
    writing << "# Project Ambrose by Imjustchico\n"
            << "# Written by the panel program when it hosts on this computer: the panel on loopback, on the port it chose and remembers.\n"
            << "Panel.Enable = 1\nPanel.BindIP = 127.0.0.1\n"
            << fmt::format("Panel.Port = {}\n", port);
    if (!writing)
    {
        error = fmt::format("{} could not be written", ConfigMgr::PathToUtf8(own));
        return std::nullopt;
    }
    return prepared;
}
