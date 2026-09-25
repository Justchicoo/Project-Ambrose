/*
 * Project Ambrose by Imjustchico
 * Where the launcher's window was when it was last closed, written down and read back. Only the numbers live here, with no window and no operating system in sight, so the rule that a remembered place has to be a place a window could actually be is one a test can drive: a size below what the screens need to hold their words is not taken, a size larger than any screen is not taken, and anything that cannot be read at all leaves the window to open where it would have opened anyway. A window that comes back somewhere its user cannot reach is worse than one that forgets, so forgetting is what every doubtful case does.
 */

#ifndef AMBROSE_LAUNCHERPLACE_H
#define AMBROSE_LAUNCHERPLACE_H

#include <optional>
#include <string>

struct WindowPlace
{
    int X = 0;
    int Y = 0;
    int Width = 0;
    int Height = 0;
    bool Maximised = false;
};

class LauncherPlace
{
public:
    static constexpr int SchemaVersion = 1;
    static constexpr int MinimumWidth = 820;
    static constexpr int MinimumHeight = 560;
    static constexpr int LargestWidth = 16384;
    static constexpr int LargestHeight = 16384;
    static constexpr int FurthestFromOrigin = 32768;

    LauncherPlace() = delete;

    static std::optional<WindowPlace> Read(std::string const& json);
    static std::string Describe(WindowPlace const& place);
};

#endif
