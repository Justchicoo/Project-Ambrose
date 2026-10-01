/*
 * Project Ambrose by Imjustchico
 * Reads the window's message into a launcher request and writes the launcher's answer back. A field the message does not carry is left unset rather than defaulted here, because the defaults belong to the launcher and its configuration, and filling one in twice is how a window and a terminal start disagreeing. A field that is not a string is refused by name instead of being coerced, since a window that sent a number where a folder belongs has a bug worth seeing. A password is never written out at all: the window is handed the command with the secret replaced, so a screen anybody can see, or photograph, never carries one. The plan is written out as what a person reads on the screen, the install, written with forward slashes as the console names it, and its revision, where the client will run, which server it will join, the window size the client is started at and the whole command, so the window shows what will happen rather than a shape only this program understands. A plan and a refusal each carry the launch state they were answered in, playing or retrying unless the caller names another. The log folder is answered as opened with its path, or as not there yet with the reason, and its file URL keeps letters, digits, slashes and the few marks a path may carry plain and escapes every other byte of its UTF-8 as a percent sign and two hex digits, so a space or a hash in a folder name cannot change what the system opens.
 */

#include "LauncherChannel.h"

#include "ClientLocator.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

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

namespace
{
    std::vector<std::string> WithoutSecrets(std::vector<std::string> const& arguments)
    {
        std::vector<std::string> shown = arguments;
        for (std::size_t at = 0; at + 2 < shown.size(); ++at)
        {
            if (shown[at] == "-U" && !shown[at + 2].empty())
                shown[at + 2] = LauncherChannel::Hidden;
        }
        return shown;
    }

    std::string Rebuild(LauncherPlan const& plan, std::vector<std::string> const& arguments)
    {
        std::string line = Launcher::Quote(ConfigMgr::PathToUtf8(plan.Program));
        for (std::string const& argument : arguments)
        {
            line += ' ';
            line += Launcher::Quote(argument);
        }
        return line;
    }
}

std::string LauncherChannel::DescribePlan(LauncherPlan const& plan, std::string_view state)
{
    std::vector<std::string> const shown = WithoutSecrets(plan.Arguments);
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["ready"] = true;
    body["state"] = std::string(state);
    body["install"] = ClientLocator::PathText(plan.Install.Root);
    body["revision"] = plan.Install.Revision;
    body["program"] = ConfigMgr::PathToUtf8(plan.Program);
    body["run_folder"] = ConfigMgr::PathToUtf8(plan.RunFolder);
    body["log_file"] = ConfigMgr::PathToUtf8(plan.LogFile);
    body["host"] = plan.Host;
    body["port"] = plan.Port;
    body["locale"] = plan.Locale;
    body["window"] = std::to_string(plan.Width) + "x" + std::to_string(plan.Height);
    body["arguments"] = shown;
    body["command"] = Rebuild(plan, shown);
    return body.dump();
}

std::string LauncherChannel::DescribeRefusal(std::string const& reason, std::string_view state)
{
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["ready"] = false;
    body["state"] = std::string(state);
    body["reason"] = reason;
    return body.dump();
}

std::string LauncherChannel::DescribeFolder(std::optional<std::filesystem::path> const& folder)
{
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["opened"] = folder.has_value();
    if (folder)
        body["folder"] = ClientLocator::PathText(*folder);
    else
        body["reason"] = "the folder the game writes its log to is made once the installation is found, and it has not been yet";
    return body.dump();
}

std::string LauncherChannel::FolderAddress(std::filesystem::path const& folder)
{
    std::string const text = ClientLocator::PathText(folder);
    std::string address = text.starts_with('/') ? "file://" : "file:///";
    constexpr char const* Hex = "0123456789ABCDEF";
    for (char const c : text)
    {
        unsigned char const byte = static_cast<unsigned char>(c);
        bool const plain = (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9') || byte == '/' || byte == ':'
            || byte == '-' || byte == '_' || byte == '.' || byte == '~';
        if (plain)
        {
            address += c;
            continue;
        }
        address += '%';
        address += Hex[byte >> 4];
        address += Hex[byte & 0x0F];
    }
    return address;
}
