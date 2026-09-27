/*
 * Project Ambrose by Imjustchico
 * What has been done to this app and by whom, for GET /api/activity: the commands its own record holds and the secret settings revealed to a caller, newest first, each with who did it, from where, at what level, whether it ran or was refused and why, and the one place a row is appended to that record. A refused attempt is kept and shown like any other, because what somebody tried and was not allowed to do is the half of a record that matters most. The record is a file of one JSON object per line, so a line that cannot be read is counted and skipped rather than hiding every line after it.
 */

#ifndef AMBROSE_ADMINACTIVITYVIEW_H
#define AMBROSE_ADMINACTIVITYVIEW_H

#include "Types.h"

#include <filesystem>
#include <string>

class AdminRouter;

class AdminActivityView
{
public:
    static constexpr int SchemaVersion = 1;
    static constexpr std::size_t DefaultLimit = 200;
    static constexpr std::size_t MaxLimit = 1000;

    AdminActivityView() = delete;

    static std::string ActivityJson(std::filesystem::path const& file, std::size_t limit);
    static bool Append(std::filesystem::path const& file, std::string const& row);
    static void Register(AdminRouter& router, std::filesystem::path auditFile);
};

#endif
