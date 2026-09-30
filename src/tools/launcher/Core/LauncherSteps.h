/*
 * Project Ambrose by Imjustchico
 * The first run the launcher's window shows as it happens: finding the install, readying the folder the client runs from, and waiting for the login server to answer. Each time the window asks, at most one step moves on, so the window draws each as it goes rather than one answer at the end, and every step says what it found in its own numbers. A step that fails names the cause and what to do about it and stops the steps after it; asking again from the start is how the window's Look again tries once more. The server is asked by a function handed in, so a test decides whether anything answers.
 */

#ifndef AMBROSE_LAUNCHERSTEPS_H
#define AMBROSE_LAUNCHERSTEPS_H

#include "Launcher.h"

#include <chrono>
#include <functional>
#include <string>
#include <vector>

struct LauncherStep
{
    std::string Id;
    std::string Label;
    std::string State;
    std::string Word;
};

class LauncherSteps
{
public:
    using Clock = std::chrono::steady_clock;
    using Reach = std::function<bool(std::string const& host, uint16 port, std::string& error)>;

    static constexpr int ServerTries = 30;
    static constexpr std::chrono::milliseconds ServerGap{ 1000 };
    static constexpr std::chrono::milliseconds ReachTimeout{ 700 };

    explicit LauncherSteps(Reach reach, std::function<Clock::time_point()> now = &Clock::now);

    std::vector<LauncherStep> const& Advance(Launcher const& launcher, LauncherRequest const& request, SetupMode mode, SetupPrompt& prompt);
    std::vector<LauncherStep> const& Steps() const { return _steps; }
    void Restart();

    static std::string Describe(std::vector<LauncherStep> const& steps);
    static bool ReachByTcp(std::string const& host, uint16 port, std::string& error);

private:
    Reach _reach;
    std::function<Clock::time_point()> _now;
    std::vector<LauncherStep> _steps;
    std::optional<LauncherPlan> _plan;
    int _tries = 0;
    Clock::time_point _lastTry{};
};

#endif
