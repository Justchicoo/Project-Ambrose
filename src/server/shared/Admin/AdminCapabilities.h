/*
 * Project Ambrose by Imjustchico
 * The registries a running build describes itself from, reload targets, schedule actions, announcement channels and problem codes, so the panel offers only what this build can do and a status response names problems by codes the build registered.
 */

#ifndef AMBROSE_ADMINCAPABILITIES_H
#define AMBROSE_ADMINCAPABILITIES_H

#include <map>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct AdminProblem
{
    std::string Code;
    std::string Message;
    std::string Subject;
};

namespace AdminProblemCodes
{
    inline constexpr std::string_view InstallMissing = "install_missing";
    inline constexpr std::string_view TypeDumpMissing = "type_dump_missing";
    inline constexpr std::string_view TypeDumpStale = "type_dump_stale";
    inline constexpr std::string_view DatabaseUnreachable = "database_unreachable";
    inline constexpr std::string_view SchemaUpdatePending = "schema_update_pending";
    inline constexpr std::string_view RevisionNotAllowed = "revision_not_allowed";
}

class AdminCapabilities
{
public:
    static AdminCapabilities& Instance();

    AdminCapabilities() = default;
    AdminCapabilities(AdminCapabilities const&) = delete;
    AdminCapabilities& operator=(AdminCapabilities const&) = delete;

    void AddReloadTarget(std::string name);
    void AddScheduleAction(std::string name);
    void AddAnnouncementChannel(std::string name);
    void AddProblemCode(std::string code, std::string description);
    void RegisterStandardProblems();
    void Clear();

    std::vector<std::string> ReloadTargets() const;
    std::vector<std::string> ScheduleActions() const;
    std::vector<std::string> AnnouncementChannels() const;
    std::vector<std::pair<std::string, std::string>> ProblemCodes() const;
    bool HasProblemCode(std::string_view code) const;

private:
    mutable std::shared_mutex _mutex;
    std::vector<std::string> _reloadTargets;
    std::vector<std::string> _scheduleActions;
    std::vector<std::string> _announcementChannels;
    std::map<std::string, std::string> _problemCodes;
};

#define sAdminCapabilities AdminCapabilities::Instance()

#endif
