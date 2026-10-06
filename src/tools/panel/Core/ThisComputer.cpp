/*
 * Project Ambrose by Imjustchico
 * Reads the supervisor's admin token from its default file, takes a supervisor to be there only when its health names the supervisor, and asks it for a local link, reading a panel that is off from the supervisor's own panel_off refusal rather than guessing.
 */

#include "ThisComputer.h"

#include "AdminToken.h"
#include "StringUtil.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

std::optional<std::string> ThisComputer::Token(std::filesystem::path const& dataFolder, std::string& error)
{
    std::filesystem::path const file = AdminToken::DefaultFile("supervisor", dataFolder);
    std::ifstream held(file, std::ios::binary);
    if (!held)
    {
        error = "no supervisor's admin token is in the Ambrose data folder, so no supervisor on this computer is listed";
        return std::nullopt;
    }
    std::ostringstream text;
    text << held.rdbuf();
    std::string const contents = text.str();
    std::string const token(Ambrose::Trim(contents));
    if (std::optional<std::string> const problem = AdminToken::Validate(token))
    {
        error = fmt::format("the supervisor's admin token file holds no usable token: it {}", *problem);
        return std::nullopt;
    }
    return token;
}

std::optional<LocalSupervisor> ThisComputer::Find(AdminAsker& asker, std::string const& token, uint16 port, std::string& error)
{
    AdminAnswer const health = asker.Ask(port, token, "GET", "/api/health", std::string());
    if (!health.Answered)
    {
        error = fmt::format("no supervisor answers on this computer's port {}: {}", port, health.Error);
        return std::nullopt;
    }
    nlohmann::json const body = nlohmann::json::parse(health.Body, nullptr, false);
    if (health.Status != 200 || !body.is_object() || body.value("app", std::string()) != "supervisor")
    {
        error = fmt::format("what answers on this computer's port {} is not a supervisor", port);
        return std::nullopt;
    }
    LocalSupervisor found;
    found.AdminPort = port;
    found.Revision = body.value("revision", std::string());
    return found;
}

std::optional<std::string> ThisComputer::LocalLink(AdminAsker& asker, std::string const& token, uint16 port, std::string& error)
{
    AdminAnswer const answer = asker.Ask(port, token, "POST", "/api/panel-links", R"({"kind":"local"})");
    if (!answer.Answered)
    {
        error = fmt::format("the supervisor on this computer did not answer: {}", answer.Error);
        return std::nullopt;
    }
    nlohmann::json const body = nlohmann::json::parse(answer.Body, nullptr, false);
    if (answer.Status != 200)
    {
        std::string const code = body.is_object() ? body.value("error", std::string()) : std::string();
        error = code == "panel_off" ? std::string(PanelOffAdvice)
                                    : fmt::format("the supervisor on this computer gave no link: {}", body.is_object() ? body.value("message", std::to_string(answer.Status)) : std::to_string(answer.Status));
        return std::nullopt;
    }
    std::string const link = body.is_object() ? body.value("link", std::string()) : std::string();
    if (link.empty())
    {
        error = "the supervisor on this computer answered without a link";
        return std::nullopt;
    }
    return link;
}
