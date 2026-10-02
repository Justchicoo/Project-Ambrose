/*
 * Project Ambrose by Imjustchico
 * The only thing the launcher's window may say to the launcher, and the only thing it hears back. A request names the same values the console options name and nothing else, and it is turned into the very LauncherRequest the console builds, so the window and the terminal reach one Prepare and can never disagree about what would be run. What comes back describes the plan in the words the window shows, or names why there is no plan, because a window that says only that something failed sends its user to a log file they do not have. Every answer also carries the launch state the steps decided, so the window's one primary button follows the launcher rather than a guess of its own. The folder holding the client's log is answered by the launcher from its own plan and never taken from the window, so a page can ask for that folder to be opened and for nothing else, and the address handed to the system to open it is a file URL with every character outside a plain path escaped. No decision is made here: this reads a message, hands it to the launcher and writes down the answer.
 */

#ifndef AMBROSE_LAUNCHERCHANNEL_H
#define AMBROSE_LAUNCHERCHANNEL_H

#include "Launcher.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

class LauncherChannel
{
public:
    static constexpr int SchemaVersion = 1;
    static constexpr char const* Hidden = "********";

    LauncherChannel() = delete;

    static bool ReadRequest(std::string const& json, LauncherRequest& request, std::string& error);
    static std::string DescribePlan(LauncherPlan const& plan, std::string_view state = "play");
    static std::string DescribeRefusal(std::string const& reason, std::string_view state = "retry");
    static std::string DescribeFolder(std::optional<std::filesystem::path> const& folder);
    static std::string FolderAddress(std::filesystem::path const& folder);
};

#endif
