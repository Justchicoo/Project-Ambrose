/*
 * Project Ambrose by Imjustchico
 * Reads the window's message into a launcher request and writes the launcher's answer back. A field the message does not carry is left unset rather than defaulted here, because the defaults belong to the launcher and its configuration, and filling one in twice is how a window and a terminal start disagreeing. A field that is not a string is refused by name instead of being coerced, since a window that sent a number where a folder belongs has a bug worth seeing. The plan is written out as what a person reads on the screen, the install and its revision, where the client will run, which server it will join and the whole command, so the window shows what will happen rather than a shape only this program understands.
 */

#include "LauncherChannel.h"

#include <nlohmann/json.hpp>

#include <string>

namespace
{
    bool Take(nlohmann::json const& body, char const* name, std::optional<std::string>& into, std::string& error)
    {
        auto const found = body.find(name);
        if (found == body.end() || found->is_null())
            return true;
        if (!found->is_string())
        {
            error = std::string("the field ") + name + " must be text";
            return false;
        }
        into = found->get<std::string>();
        return true;
    }
}

bool LauncherChannel::ReadRequest(std::string const& json, LauncherRequest& request, std::string& error)
{
    nlohmann::json const body = nlohmann::json::parse(json, nullptr, false);
    if (!body.is_object())
    {
        error = "the window sent something that is not a message";
        return false;
    }

    if (!Take(body, "client_dir", request.ClientDir, error) || !Take(body, "host", request.Host, error)
        || !Take(body, "port", request.Port, error) || !Take(body, "locale", request.Locale, error)
        || !Take(body, "window", request.Window, error) || !Take(body, "fullscreen", request.Fullscreen, error)
        || !Take(body, "window_x", request.WindowX, error) || !Take(body, "window_y", request.WindowY, error)
        || !Take(body, "run_dir", request.RunDir, error) || !Take(body, "character", request.Character, error))
        return false;

    auto const user = body.find("user");
    if (user != body.end() && !user->is_null())
    {
        if (!user->is_object())
        {
            error = "the field user must be a message of its own";
            return false;
        }
        ClientLogin login;
        std::optional<std::string> id;
        std::optional<std::string> key;
        std::optional<std::string> name;
        if (!Take(*user, "user_id", id, error) || !Take(*user, "key", key, error) || !Take(*user, "name", name, error))
            return false;
        login.UserId = id.value_or(std::string());
        login.Key = key.value_or(std::string());
        login.Name = name.value_or(std::string());
        request.User = login;
    }
    return true;
}

std::string LauncherChannel::DescribePlan(LauncherPlan const& plan)
{
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["ready"] = true;
    body["install"] = ConfigMgr::PathToUtf8(plan.Install.Root);
    body["revision"] = plan.Install.Revision;
    body["program"] = ConfigMgr::PathToUtf8(plan.Program);
    body["run_folder"] = ConfigMgr::PathToUtf8(plan.RunFolder);
    body["log_file"] = ConfigMgr::PathToUtf8(plan.LogFile);
    body["host"] = plan.Host;
    body["port"] = plan.Port;
    body["locale"] = plan.Locale;
    body["arguments"] = plan.Arguments;
    body["command"] = plan.Command();
    return body.dump();
}

std::string LauncherChannel::DescribeRefusal(std::string const& reason)
{
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["ready"] = false;
    body["reason"] = reason;
    return body.dump();
}
