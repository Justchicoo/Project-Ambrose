/*
 * Project Ambrose by Imjustchico
 * Takes its own options out of the command line and hands the rest to the options every app reads, loads the configuration with its layers, finds the supervisor's admin API by its Admin options, with the default port the supervisor itself uses, reads the admin token from Admin.Token or the token file without writing one, posts one request for a link over TLS when the admin API serves it, and prints only the link or the line on standard output and a note or the refusal on standard error; it never starts the logger, so nothing it sees reaches a log, and it exits 0 with a link, 1 when no link came back and 2 when the command line was wrong.
 */

#include "PanelLinkClient.h"
#include "AdminClient.h"
#include "AdminToken.h"
#include "AppOptions.h"
#include "ConfigMgr.h"
#include "ListenerSettings.h"
#include "Panel.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <ostream>
#include <system_error>

namespace
{
    std::string_view NameOf(std::string_view argument)
    {
        return argument.substr(0, argument.find('='));
    }

    bool TakeValue(std::vector<std::string> const& arguments, std::size_t& index, std::string& value)
    {
        std::string_view const argument = arguments[index];
        if (std::size_t const equals = argument.find('='); equals != std::string_view::npos)
        {
            value = std::string(argument.substr(equals + 1));
            return !value.empty();
        }
        if (index + 1 < arguments.size() && !arguments[index + 1].starts_with('-'))
        {
            value = arguments[++index];
            return true;
        }
        return false;
    }

    std::string Problem(nlohmann::json const& reply, int status)
    {
        std::string message = reply.is_object() && reply.contains("message") && reply["message"].is_string() ? reply["message"].get<std::string>()
                                                                                                           : fmt::format("the supervisor answered {}", status);
        if (reply.is_object() && reply.contains("fields") && reply["fields"].is_object())
            for (auto const& [field, problem] : reply["fields"].items())
                if (problem.is_string() && problem.get_ref<std::string const&>() != message)
                    message += fmt::format("; {}: {}", field, problem.get<std::string>());
        return message;
    }
}

bool PanelLinkClient::IsAsked(std::vector<std::string> const& arguments)
{
    for (std::size_t index = 1; index < arguments.size(); ++index)
    {
        std::string_view const name = NameOf(arguments[index]);
        if (name == LinkOption || name == PairOption)
            return true;
    }
    return false;
}

PanelLinkCommand PanelLinkClient::Parse(std::vector<std::string> const& arguments)
{
    PanelLinkCommand command;
    if (!arguments.empty())
        command.Rest.push_back(arguments.front());
    bool addressGiven = false;
    for (std::size_t index = 1; index < arguments.size() && command.Error.empty(); ++index)
    {
        std::string_view const name = NameOf(arguments[index]);
        if (name == LinkOption || name == PairOption)
        {
            if (command.Asked)
            {
                command.Error = fmt::format("give {} or {} once", LinkOption, PairOption);
                break;
            }
            command.Asked = true;
            command.Kind = name == LinkOption ? PanelLinkKind::Local : PanelLinkKind::Pairing;
            if (!TakeValue(arguments, index, command.Username) && command.Kind == PanelLinkKind::Pairing)
                command.Error = fmt::format("{} takes the name of the operator the link signs in", PairOption);
        }
        else if (name == AddressOption)
        {
            addressGiven = true;
            if (!TakeValue(arguments, index, command.Address))
                command.Error = fmt::format("{} takes the host, or host:port, the program reaches the panel at", AddressOption);
        }
        else
            command.Rest.push_back(arguments[index]);
    }
    if (!command.Error.empty())
        return command;
    if (!command.Asked)
        command.Error = fmt::format("give {} or {}", LinkOption, PairOption);
    else if (command.Kind == PanelLinkKind::Pairing && !addressGiven)
        command.Error = fmt::format("{} takes {} <host[:port]>, the address the program on the other machine reaches the panel at", PairOption, AddressOption);
    else if (command.Kind == PanelLinkKind::Local && addressGiven)
        command.Error = fmt::format("{} goes with {}; a local link is opened on this machine", AddressOption, PairOption);
    return command;
}

std::string PanelLinkClient::Usage()
{
    return fmt::format(
        "Usage: supervisor [-c <file>] [--set <Key=Value>] {} [username]\n"
        "       supervisor [-c <file>] [--set <Key=Value>] {} <username> {} <host[:port]>\n"
        "  {}   print a link that opens the running supervisor's panel signed in, once, from this machine, within 60 seconds\n"
        "  {}   print the line a desktop program on another machine pairs with, within 10 minutes\n",
        LinkOption, PairOption, AddressOption, LinkOption, PairOption);
}

int PanelLinkClient::Run(std::vector<std::string> const& arguments, ConfigMgr& config, std::filesystem::path const& dataFolder, uint16 defaultAdminPort, std::ostream& out, std::ostream& err)
{
    PanelLinkCommand const command = Parse(arguments);
    if (!command.Error.empty())
    {
        err << "supervisor: " << command.Error << '\n' << Usage();
        return 2;
    }
    AppOptions const options = AppOptions::Parse(command.Rest, "supervisor.conf");
    if (!options.Error.empty())
    {
        err << "supervisor: " << options.Error << '\n' << Usage();
        return 2;
    }

    std::error_code pathError;
    std::filesystem::path const named = ConfigMgr::PathFromUtf8(options.ConfigFile);
    std::filesystem::path const absolute = std::filesystem::absolute(named, pathError);
    std::filesystem::path const configFile = pathError ? named : absolute;
    ConfigLoadResult const loaded = config.LoadInitial(configFile, command.Rest, options.Overrides);
    if (!loaded.Succeeded())
    {
        err << "supervisor: the configuration could not be read from " << ConfigMgr::PathToUtf8(configFile) << '\n';
        for (ConfigIssue const& issue : loaded.Errors)
            err << "  " << issue.ToString() << '\n';
        return 1;
    }

    ListenerSettings const admin = ListenerSettings::Load(config, "Admin", defaultAdminPort);
    if (!admin.Enable)
    {
        err << "supervisor: Admin.Enable = 0, so the running supervisor has no admin API to ask for a link\n";
        return 1;
    }
    if (admin.Port == 0)
    {
        err << "supervisor: Admin.Port = 0 lets the running supervisor choose its own port, so set the port it listens on to ask it for a link\n";
        return 1;
    }
    AdminTokenResult const token = AdminToken::Read(admin, "supervisor", dataFolder, config.GetFilename().parent_path());
    if (!token.Succeeded())
    {
        err << "supervisor: " << token.Error << '\n';
        return 1;
    }

    bool const pairing = command.Kind == PanelLinkKind::Pairing;
    nlohmann::json body;
    body["kind"] = pairing ? "pairing" : "local";
    if (!command.Username.empty())
        body["username"] = command.Username;
    if (pairing)
        body["address"] = command.Address;
    AdminClient const client(AdminClient::ConnectHost(admin.BindIp), admin.Port, token.Token, admin.HasTls());
    AdminClientRequest request;
    request.Method = "POST";
    request.Path = std::string(Panel::LinksPath);
    request.Body = body.dump();
    AdminClientResponse const answer = client.Send(request, Timeout);
    if (!answer.Answered)
    {
        err << "supervisor: no running supervisor gave a link: " << answer.Error << '\n';
        return 1;
    }
    nlohmann::json const reply = nlohmann::json::parse(answer.Body, nullptr, false);
    if (answer.Status != 200)
    {
        err << "supervisor: " << Problem(reply, answer.Status) << '\n';
        return 1;
    }
    std::string const printed = reply.is_object() ? reply.value(pairing ? "line" : "link", std::string()) : std::string();
    if (printed.empty())
    {
        err << "supervisor: the running supervisor answered without a link\n";
        return 1;
    }

    std::string const username = reply.value("username", std::string());
    std::string const made = reply.value("created_owner", false) ? "; the panel had no operator, so they are its owner now, with a password nobody is told until they set their own" : "";
    int64 const seconds = reply.value("expires_seconds", int64{ 0 });
    out << printed << '\n';
    out.flush();
    if (!pairing)
        err << fmt::format("It signs {} in once, from this machine only, within {} seconds{}", username, seconds, made) << '\n';
    else if (reply.contains("fingerprint") && reply["fingerprint"].is_string())
        err << fmt::format("It pins the certificate the panel serves, SHA-256 {}, and signs {} in once within {} minutes{}", reply["fingerprint"].get<std::string>(), username,
            seconds / 60, made) << '\n';
    else
        err << fmt::format("It pins no certificate, since the panel serves plain HTTP on this machine only; it signs {} in once within {} minutes{}", username, seconds / 60, made) << '\n';
    return 0;
}
