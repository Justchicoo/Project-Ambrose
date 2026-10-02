/*
 * Project Ambrose by Imjustchico
 * The hours when a restart, an update or a migration is refused: Power.ProtectedHours read as daily windows in the zone Power.ProtectedHoursZone names, each written HH:MM-HH:MM and allowed to run past midnight, and asked whether a moment falls inside one, naming the window when it does.
 */

#ifndef AMBROSE_PROTECTEDHOURS_H
#define AMBROSE_PROTECTEDHOURS_H

#include "Types.h"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct ProtectedWindow
{
    uint32 FromMinute = 0;
    uint32 ToMinute = 0;

    std::string Describe() const;
};

class ProtectedHours
{
public:
    static constexpr std::string_view DefaultZone = "UTC";

    static std::optional<ProtectedHours> Parse(std::string_view windows, std::string_view zone, std::string& error);

    bool Empty() const noexcept { return _windows.empty(); }
    std::vector<ProtectedWindow> const& Windows() const noexcept { return _windows; }
    std::string const& Zone() const noexcept { return _zone; }
    std::optional<ProtectedWindow> Covering(std::chrono::system_clock::time_point when) const;
    std::string Describe(ProtectedWindow const& window) const;

private:
    std::vector<ProtectedWindow> _windows;
    std::string _zone{ DefaultZone };
};

#endif
