/*
 * Project Ambrose by Imjustchico
 * The status, capabilities, apps and errors routes: the snapshot an app fills at request time, the JSON each route answers with, and the field lists a schema test holds so a field is only ever added and never renamed or removed.
 */

#ifndef AMBROSE_ADMINSTATUS_H
#define AMBROSE_ADMINSTATUS_H

#include "AdminCapabilities.h"
#include "ProcessInfo.h"
#include "StatsRegistry.h"
#include "Types.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class AdminRouter;

struct AdminAppIdentity
{
    std::string Name;
    std::string Role;
    std::string Realm;
    std::string Address;
    uint16 Port = 0;
    std::string Revision;
};

struct AdminTickWindow
{
    double AverageMs = 0.0;
    double MaxMs = 0.0;
    uint32 Samples = 0;
    uint32 WindowSeconds = 60;
};

struct AdminStatusSnapshot
{
    AdminAppIdentity App;
    std::string State;
    uint64 UptimeSeconds = 0;
    std::optional<Ambrose::ProcessSnapshot> Process;
    std::optional<uint64> Sessions;
    std::optional<AdminTickWindow> Tick;
    std::vector<std::pair<std::string, Ambrose::StatValue>> Stats;
    std::vector<AdminProblem> Problems;
};

class AdminStatus
{
public:
    static constexpr uint32 SchemaVersion = 1;

    using Source = std::function<AdminStatusSnapshot()>;

    AdminStatus() = delete;

    static void Register(AdminRouter& router, Source source);
    static std::string StatusJson(AdminStatusSnapshot const& snapshot);
    static std::string AppsJson(AdminStatusSnapshot const& snapshot);
    static std::string CapabilitiesJson();
    static std::string ErrorsJson(std::string const& appName);
    static std::vector<std::string> const& StatusFields();
    static std::vector<std::string> const& ErrorFields();
    static std::vector<std::string> const& AppFields();
    static std::vector<std::string> const& CapabilityFields();
};

#endif
