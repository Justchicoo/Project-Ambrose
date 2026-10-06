/*
 * Project Ambrose by Imjustchico
 * Reads the login server's entry from the supervisor's app list, ready only when it runs and has a ready time, and reads the private database's SHA256SUMS list, hashing each file it names in turn and refusing on the first one that is missing, outside the folder or different.
 */

#include "HostPlay.h"

#include "ConfigMgr.h"
#include "Hex.h"
#include "SHA256.h"
#include "StringUtil.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

PlayDecision HostPlay::Decide(std::string const& appsJson, std::string const& host, uint16 loginPort)
{
    PlayDecision decision;
    nlohmann::json const listed = nlohmann::json::parse(appsJson, nullptr, false);
    nlohmann::json const* login = nullptr;
    if (listed.is_object() && listed.contains("apps") && listed["apps"].is_array())
        for (nlohmann::json const& app : listed["apps"])
            if (app.is_object() && app.value("name", std::string()) == LoginApp)
                login = &app;
    if (login == nullptr)
    {
        decision.Waiting = "the supervisor runs no login server, so there is nothing for the client to join yet";
        return decision;
    }
    std::string const state = login->value("state", std::string("offline"));
    bool const ready = state == "running" && login->contains("ready_epoch_ms") && !(*login)["ready_epoch_ms"].is_null();
    if (!ready)
    {
        std::string stage;
        if (login->contains("start") && (*login)["start"].is_object())
            stage = (*login)["start"].value("stage", std::string());
        decision.Waiting = stage.empty() ? fmt::format("Play waits for the login server, which is {}", state)
                                         : fmt::format("Play waits for the login server, which is {}: {}", state, stage);
        return decision;
    }
    decision.Ready = true;
    decision.LauncherArguments = { "--host", host, "--port", std::to_string(loginPort) };
    return decision;
}

bool HostPlay::VerifyDatabase(std::filesystem::path const& folder, std::string& error)
{
    std::ifstream list(folder / ChecksumFile, std::ios::binary);
    if (!list)
    {
        error = fmt::format("the private database in {} has no {} to check it against, so none of it runs", ConfigMgr::PathToUtf8(folder), ChecksumFile);
        return false;
    }
    std::string line;
    std::size_t checked = 0;
    while (std::getline(list, line))
    {
        line = std::string(Ambrose::Trim(line));
        if (line.empty())
            continue;
        std::size_t const space = line.find(' ');
        if (space == std::string::npos)
        {
            error = fmt::format("{} holds a line that is not a checksum and a file", ChecksumFile);
            return false;
        }
        std::string const expected = Ambrose::ToLower(line.substr(0, space));
        std::string name(Ambrose::Trim(std::string_view(line).substr(space + 1)));
        if (name.starts_with('*'))
            name.erase(0, 1);
        if (name.find("..") != std::string::npos || name.starts_with('/') || name.find(':') != std::string::npos)
        {
            error = fmt::format("{} names {}, which is outside the private database's folder", ChecksumFile, name);
            return false;
        }
        std::ifstream file(folder / ConfigMgr::PathFromUtf8(name), std::ios::binary);
        if (!file)
        {
            error = fmt::format("the private database's {} is missing, so none of it runs", name);
            return false;
        }
        std::ostringstream bytes;
        bytes << file.rdbuf();
        std::string const actual = Hex::Encode(SHA256::GetDigestOf(std::string_view(bytes.str())), Hex::Case::Lower);
        if (actual != expected)
        {
            error = fmt::format("the private database's {} does not match its published checksum, so none of it runs", name);
            return false;
        }
        ++checked;
    }
    if (checked == 0)
    {
        error = fmt::format("{} names no file, so the private database cannot be checked and none of it runs", ChecksumFile);
        return false;
    }
    return true;
}
