/*
 * Project Ambrose by Imjustchico
 * The supervisor's command line for sign-in links: --panel-link [username] and --panel-pair <username> --address <host[:port]> read the configuration and the admin token the way the running supervisor does, without ever writing a token, ask that supervisor's admin API for the link, and print the link or the pairing line alone on standard output, with a note or the refusal on standard error, so a program can read the one line it needs.
 */

#ifndef AMBROSE_PANELLINKCLIENT_H
#define AMBROSE_PANELLINKCLIENT_H

#include "PanelLinks.h"
#include "Types.h"

#include <chrono>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

class ConfigMgr;

struct PanelLinkCommand
{
    bool Asked = false;
    PanelLinkKind Kind = PanelLinkKind::Local;
    std::string Username = {};
    std::string Address = {};
    std::vector<std::string> Rest = {};
    std::string Error = {};
};

class PanelLinkClient
{
public:
    static constexpr std::string_view LinkOption = "--panel-link";
    static constexpr std::string_view PairOption = "--panel-pair";
    static constexpr std::string_view AddressOption = "--address";
    static constexpr std::chrono::seconds Timeout{ 60 };

    PanelLinkClient() = delete;

    static bool IsAsked(std::vector<std::string> const& arguments);
    static PanelLinkCommand Parse(std::vector<std::string> const& arguments);
    static std::string Usage();
    static int Run(std::vector<std::string> const& arguments, ConfigMgr& config, std::filesystem::path const& dataFolder, uint16 defaultAdminPort, std::ostream& out, std::ostream& err);
};

#endif
