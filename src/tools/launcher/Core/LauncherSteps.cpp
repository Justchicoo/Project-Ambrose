/*
 * Project Ambrose by Imjustchico
 * Moves the first run on by one step per ask: the install through the same Prepare the console reaches, the run folder written as Play would write it, and the login server asked once a second, up to thirty times, with a TCP connection that gives up after 700 milliseconds so the window never waits on it. The launch state is read off the steps in one place, a failed install first since the cure is to name one, then any other failure, then whatever is still to do, and only once every step is done the client's own state, launching for ten seconds after it was handed off and playing after that; the server is online once it answered, offline once a try went unanswered and unknown before it was asked, and everything the window is sent about the steps, the state and the server is written out together so the three can never be read from different moments.
 */

#include "LauncherSteps.h"

#include "ConfigMgr.h"
#include "LauncherChannel.h"

#include <asio/connect.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace
{
    constexpr char const* Waiting = "waiting";
    constexpr char const* Doing = "doing";
    constexpr char const* Done = "done";
    constexpr char const* Wrong = "wrong";

    std::vector<LauncherStep> Fresh()
    {
        return {
            { "install", "Find your Wizard101 installation", Waiting, "Not looked at yet" },
            { "run-folder", "Ready the folder the game runs from", Waiting, "Waits for the installation" },
            { "server", "Reach the login server", Waiting, "Waits for the run folder" },
        };
    }
}

LauncherSteps::LauncherSteps(Reach reach, std::function<Clock::time_point()> now) : _reach(std::move(reach)), _now(std::move(now)), _steps(Fresh())
{
}

void LauncherSteps::Restart()
{
    _steps = Fresh();
    _plan.reset();
    _tries = 0;
    _lastTry = {};
    _started.reset();
}

void LauncherSteps::Started()
{
    _started = _now();
}

LaunchState LauncherSteps::State() const
{
    if (_steps[0].State == Wrong)
        return LaunchState::Locate;
    for (LauncherStep const& step : _steps)
        if (step.State == Wrong)
            return LaunchState::Retry;
    if (_steps[0].State != Done)
        return LaunchState::Checking;
    for (LauncherStep const& step : _steps)
        if (step.State != Done)
            return LaunchState::SettingUp;
    if (!_started)
        return LaunchState::Play;
    return _now() - *_started < LaunchingFor ? LaunchState::Launching : LaunchState::Playing;
}

std::string LauncherSteps::ServerStatus() const
{
    if (_steps[2].State == Done)
        return "online";
    if (_steps[2].State == Wrong || _tries > 0)
        return "offline";
    return "unknown";
}

std::optional<std::filesystem::path> LauncherSteps::LogFolder() const
{
    if (!_plan || _steps[1].State != Done)
        return std::nullopt;
    return _plan->LogFile.parent_path();
}

char const* LauncherSteps::Name(LaunchState state)
{
    switch (state)
    {
        case LaunchState::Locate:
            return "locate";
        case LaunchState::Checking:
            return "checking";
        case LaunchState::SettingUp:
            return "setting-up";
        case LaunchState::Play:
            return "play";
        case LaunchState::Launching:
            return "launching";
        case LaunchState::Playing:
            return "playing";
        case LaunchState::Retry:
            return "retry";
    }
    return "retry";
}

std::vector<LauncherStep> const& LauncherSteps::Advance(Launcher const& launcher, LauncherRequest const& request, SetupMode mode, SetupPrompt& prompt)
{
    for (LauncherStep const& step : _steps)
        if (step.State == Wrong)
            return _steps;

    LauncherStep& install = _steps[0];
    LauncherStep& folder = _steps[1];
    LauncherStep& server = _steps[2];
    std::string error;

    if (install.State != Done)
    {
        _plan = launcher.Prepare(request, mode, prompt, error);
        if (!_plan)
        {
            install.State = Wrong;
            install.Word = fmt::format("{}. Name the installation with ClientDir in launcher.conf or with --client, then look again", error);
            return _steps;
        }
        install.State = Done;
        install.Word = fmt::format("Wizard101 {} at {}", _plan->Install.Revision, ConfigMgr::PathToUtf8(_plan->Install.Root.lexically_normal()));
        folder.State = Doing;
        folder.Word = "Writing the configuration the game reads";
        return _steps;
    }

    if (folder.State != Done)
    {
        if (!launcher.WriteRunFolder(*_plan, error))
        {
            folder.State = Wrong;
            folder.Word = fmt::format("{}. Choose a folder you can write with RunDir in launcher.conf or with --run-dir, then look again", error);
            return _steps;
        }
        folder.State = Done;
        folder.Word = fmt::format("{} with {} files, its configuration from {}", ConfigMgr::PathToUtf8(_plan->RunFolder.lexically_normal()),
            _plan->Folder.Files.size() + _plan->Folder.Copies.size(), _plan->Folder.ConfigSource);
        server.State = Doing;
        server.Word = fmt::format("Asking {}:{}", _plan->Host, _plan->Port);
        return _steps;
    }

    if (server.State == Done)
        return _steps;
    Clock::time_point const now = _now();
    if (_tries > 0 && now - _lastTry < ServerGap)
        return _steps;
    _lastTry = now;
    ++_tries;
    if (_reach && _reach(_plan->Host, _plan->Port, error))
    {
        server.State = Done;
        server.Word = fmt::format("{}:{} is open, answered on try {}", _plan->Host, _plan->Port, _tries);
        return _steps;
    }
    if (_tries >= ServerTries)
    {
        server.State = Wrong;
        server.Word = fmt::format("Nothing answered at {}:{} after {} tries ({}). Start the Ambrose servers, or name the right one with LoginHost and LoginPort, then look again",
            _plan->Host, _plan->Port, _tries, error.empty() ? "no answer" : error);
        return _steps;
    }
    server.State = Doing;
    server.Word = fmt::format("Waiting for {}:{} to open, try {} of {}", _plan->Host, _plan->Port, _tries, ServerTries);
    return _steps;
}

std::string LauncherSteps::Describe() const
{
    nlohmann::json body;
    body["schema"] = LauncherChannel::SchemaVersion;
    body["state"] = Name(State());
    body["server"] = { { "status", ServerStatus() }, { "address", _plan ? fmt::format("{}:{}", _plan->Host, _plan->Port) : std::string() } };
    body["steps"] = nlohmann::json::array();
    for (LauncherStep const& step : _steps)
        body["steps"].push_back({ { "id", step.Id }, { "label", step.Label }, { "state", step.State }, { "word", step.Word } });
    return body.dump();
}

bool LauncherSteps::ReachByTcp(std::string const& host, uint16 port, std::string& error)
{
    asio::io_context context;
    asio::ip::tcp::resolver resolver(context);
    std::error_code code;
    auto const endpoints = resolver.resolve(host, std::to_string(port), code);
    if (code)
    {
        error = code.message();
        return false;
    }
    asio::ip::tcp::socket socket(context);
    bool connected = false;
    asio::async_connect(socket, endpoints, [&](std::error_code const& result, asio::ip::tcp::endpoint const&)
    {
        connected = !result;
        if (result)
            error = result.message();
    });
    context.run_for(ReachTimeout);
    if (!connected && error.empty())
        error = "no answer within 700 milliseconds";
    std::error_code ignored;
    socket.close(ignored);
    return connected;
}
