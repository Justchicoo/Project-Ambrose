/*
 * Project Ambrose by Imjustchico
 * The supervisor on this machine, listed as This computer without being added: it is there when its admin API answers on loopback with the token from the default file under the Ambrose data folder, its revision comes from its health, and it opens through a 17.180 local link the program asks it for. A supervisor whose panel listener is off is still listed, and asking it for a link names the setting that turns it on, and no token or link is ever written anywhere by the program. The admin API is reached through an interface so a test answers as a supervisor would.
 */

#ifndef AMBROSE_THISCOMPUTER_H
#define AMBROSE_THISCOMPUTER_H

#include "Types.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

struct AdminAnswer
{
    bool Answered = false;
    int Status = 0;
    std::string Body = {};
    std::string Error = {};
};

class AdminAsker
{
public:
    virtual ~AdminAsker() = default;
    virtual AdminAnswer Ask(uint16 port, std::string const& token, std::string_view method, std::string_view path, std::string const& body) = 0;
};

struct LocalSupervisor
{
    uint16 AdminPort = 0;
    std::string Revision = {};
};

class ThisComputer
{
public:
    static constexpr uint16 DefaultAdminPort = 12020;
    static constexpr std::string_view PanelOffAdvice = "Its panel is off: set Panel.Enable = 1 in supervisor.conf and start the supervisor again";

    ThisComputer() = delete;

    static std::optional<std::string> Token(std::filesystem::path const& dataFolder, std::string& error);
    static std::optional<LocalSupervisor> Find(AdminAsker& asker, std::string const& token, uint16 port, std::string& error);
    static std::optional<std::string> LocalLink(AdminAsker& asker, std::string const& token, uint16 port, std::string& error);
};

#endif
