/*
 * Project Ambrose by Imjustchico
 * The first run the launcher's window shows as it happens: finding the install, readying the folder the client runs from, and waiting for the login server to answer. Each time the window asks, at most one step moves on, so the window draws each as it goes rather than one answer at the end, and every step says what it found in its own numbers. A step that fails names the cause and what to do about it and stops the steps after it; asking again from the start is how the window's Retry tries once more. The one state the window's primary button shows is decided here from what the steps already know, so the page only draws it: Checking before the install has been looked at, Locate when no install was found, Setting up while the run folder is written and the server is asked, Play once every step is done, Launching for the first seconds after the client was handed off, Playing after that, since a client started detached is not watched and the launcher can say only that it started one, and Retry after any other step failed. The server's status beside Play, online, offline or unknown, comes from the same reach step, and the folder holding the client's log is named only once the run folder has been written, so the window can open that folder and no other. The server is asked and the time is read through functions handed in, so a test decides whether anything answers and when.
 */

#ifndef AMBROSE_LAUNCHERSTEPS_H
#define AMBROSE_LAUNCHERSTEPS_H

#include "Launcher.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

struct LauncherStep
{
    std::string Id;
    std::string Label;
    std::string State;
    std::string Word;
};

enum class LaunchState
{
    Locate,
    Checking,
    SettingUp,
    Play,
    Launching,
    Playing,
    Retry,
};

class LauncherSteps
{
public:
    using Clock = std::chrono::steady_clock;
    using Reach = std::function<bool(std::string const& host, uint16 port, std::string& error)>;

    static constexpr int ServerTries = 30;
    static constexpr std::chrono::milliseconds ServerGap{ 1000 };
    static constexpr std::chrono::milliseconds ReachTimeout{ 700 };
    static constexpr std::chrono::seconds LaunchingFor{ 10 };

    explicit LauncherSteps(Reach reach, std::function<Clock::time_point()> now = &Clock::now);

    std::vector<LauncherStep> const& Advance(Launcher const& launcher, LauncherRequest const& request, SetupMode mode, SetupPrompt& prompt);
    std::vector<LauncherStep> const& Steps() const { return _steps; }
    void Restart();
    void Started();

    LaunchState State() const;
    std::string ServerStatus() const;
    std::optional<std::filesystem::path> LogFolder() const;
    std::string Describe() const;

    static char const* Name(LaunchState state);
    static bool ReachByTcp(std::string const& host, uint16 port, std::string& error);

private:
    Reach _reach;
    std::function<Clock::time_point()> _now;
    std::vector<LauncherStep> _steps;
    std::optional<LauncherPlan> _plan;
    int _tries = 0;
    Clock::time_point _lastTry{};
    std::optional<Clock::time_point> _started;
};

#endif
