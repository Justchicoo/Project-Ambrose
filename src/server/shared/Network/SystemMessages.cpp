/*
 * Project Ambrose by Imjustchico
 * Formats the UTC time a forced disconnect carries as year-month-day hour:minute:second.
 */

#include "SystemMessages.h"

#include <fmt/format.h>

std::string SystemMessages::FormatTimeStamp(std::chrono::system_clock::time_point time)
{
    std::chrono::sys_seconds const seconds = std::chrono::floor<std::chrono::seconds>(time);
    std::chrono::sys_days const day = std::chrono::floor<std::chrono::days>(seconds);
    std::chrono::year_month_day const date(day);
    std::chrono::hh_mm_ss<std::chrono::seconds> const clock(seconds - day);
    return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", static_cast<int>(date.year()), static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()),
        clock.hours().count(), clock.minutes().count(), clock.seconds().count());
}
