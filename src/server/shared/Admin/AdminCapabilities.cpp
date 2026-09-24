/*
 * Project Ambrose by Imjustchico
 * Keeps each registry sorted and free of duplicates behind a shared mutex, and registers the six problem codes every app can raise, with the description an operator reads beside each.
 */

#include "AdminCapabilities.h"

#include <algorithm>
#include <mutex>

namespace
{
    void AddUnique(std::vector<std::string>& names, std::string name)
    {
        if (std::find(names.begin(), names.end(), name) != names.end())
            return;
        names.push_back(std::move(name));
        std::sort(names.begin(), names.end());
    }
}

AdminCapabilities& AdminCapabilities::Instance()
{
    static AdminCapabilities instance;
    return instance;
}

void AdminCapabilities::AddReloadTarget(std::string name)
{
    std::unique_lock const lock(_mutex);
    AddUnique(_reloadTargets, std::move(name));
}

void AdminCapabilities::AddScheduleAction(std::string name)
{
    std::unique_lock const lock(_mutex);
    AddUnique(_scheduleActions, std::move(name));
}

void AdminCapabilities::AddAnnouncementChannel(std::string name)
{
    std::unique_lock const lock(_mutex);
    AddUnique(_announcementChannels, std::move(name));
}

void AdminCapabilities::AddProblemCode(std::string code, std::string description)
{
    std::unique_lock const lock(_mutex);
    _problemCodes[std::move(code)] = std::move(description);
}

void AdminCapabilities::RegisterStandardProblems()
{
    AddProblemCode(std::string(AdminProblemCodes::InstallMissing), "No client installation was found, so nothing that reads the client's own data can run");
    AddProblemCode(std::string(AdminProblemCodes::TypeDumpMissing), "No type dump is in use, so ObjectProperty data cannot be read or written");
    AddProblemCode(std::string(AdminProblemCodes::TypeDumpStale), "The type dump was built from a different client program than the one installed");
    AddProblemCode(std::string(AdminProblemCodes::DatabaseUnreachable), "A database the app needs cannot be reached");
    AddProblemCode(std::string(AdminProblemCodes::SchemaUpdatePending), "A database update is waiting to be applied");
    AddProblemCode(std::string(AdminProblemCodes::RevisionNotAllowed), "The client's revision is not one Login.AllowedRevision lists");
}

void AdminCapabilities::Clear()
{
    std::unique_lock const lock(_mutex);
    _reloadTargets.clear();
    _scheduleActions.clear();
    _announcementChannels.clear();
    _problemCodes.clear();
}

std::vector<std::string> AdminCapabilities::ReloadTargets() const
{
    std::shared_lock const lock(_mutex);
    return _reloadTargets;
}

std::vector<std::string> AdminCapabilities::ScheduleActions() const
{
    std::shared_lock const lock(_mutex);
    return _scheduleActions;
}

std::vector<std::string> AdminCapabilities::AnnouncementChannels() const
{
    std::shared_lock const lock(_mutex);
    return _announcementChannels;
}

std::vector<std::pair<std::string, std::string>> AdminCapabilities::ProblemCodes() const
{
    std::shared_lock const lock(_mutex);
    return std::vector<std::pair<std::string, std::string>>(_problemCodes.begin(), _problemCodes.end());
}

bool AdminCapabilities::HasProblemCode(std::string_view code) const
{
    std::shared_lock const lock(_mutex);
    return _problemCodes.contains(std::string(code));
}
