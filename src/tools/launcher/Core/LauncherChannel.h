/*
 * Project Ambrose by Imjustchico
 * The only thing the launcher's window may say to the launcher, and the only thing it hears back. A request names the same values the console options name and nothing else, and it is turned into the very LauncherRequest the console builds, so the window and the terminal reach one Prepare and can never disagree about what would be run. What comes back describes the plan in the words the window shows, or names why there is no plan, because a window that says only that something failed sends its user to a log file they do not have. No decision is made here: this reads a message, hands it to the launcher and writes down the answer.
 */

#ifndef AMBROSE_LAUNCHERCHANNEL_H
#define AMBROSE_LAUNCHERCHANNEL_H

#include "Launcher.h"

#include <string>

class LauncherChannel
{
public:
    static constexpr int SchemaVersion = 1;

    LauncherChannel() = delete;

    static bool ReadRequest(std::string const& json, LauncherRequest& request, std::string& error);
    static std::string DescribePlan(LauncherPlan const& plan);
    static std::string DescribeRefusal(std::string const& reason);
};

#endif
