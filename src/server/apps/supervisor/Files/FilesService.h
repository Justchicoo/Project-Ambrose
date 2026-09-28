/*
 * Project Ambrose by Imjustchico
 * The file roots over HTTP, the same on the supervisor's admin API and on the panel's listener: GET /api/files lists every root with its policy, volume figures and protected paths, GET /api/files/{root}/list pages a folder, GET /api/files/{root}/content reads a window of a file, GET and PUT /api/files/{root}/rules read and replace an owner's protected patterns, with the path always in the query; every one of them, and every later writer, goes through Authorize, the one gate that checks the caller's permission, the path as parsed, the rules on it, the jail, the rules on what was reached, the secret files and the root's policy, in that order, answering a refusal with no host path in it and recording it with the path it would have reached.
 */

#ifndef AMBROSE_FILESSERVICE_H
#define AMBROSE_FILESSERVICE_H

#include "AdminRouter.h"
#include "FileJail.h"
#include "FilePolicy.h"
#include "FileRoots.h"
#include "PanelAudit.h"
#include "PathRules.h"
#include "SpaceGuard.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct FilesHooks
{
    std::function<bool(AuditEvent const& event, std::string& error)> Record = {};
    std::function<std::string(AdminRequest const& request)> NameOf = {};
    std::function<bool(AdminRequest const& request, AuditEvent const& event, std::string const& root, std::vector<std::string> const& patterns, std::string& error)> SaveRules = {};
    std::function<bool(std::vector<std::string>& errors)> Rebuild = {};
    std::function<void(std::string const& line)> Log = {};
};

struct FileDecision
{
    bool Allowed = false;
    int Status = 0;
    std::string Code = {};
    std::string Message = {};
    std::string Rule = {};
    std::string RootId = {};
    FileRoots::Snapshot Set = {};
    FileRoot const* Root = nullptr;
    Ambrose::JailPath Path = {};
    std::optional<Ambrose::JailEntry> Entry = {};
    std::optional<Ambrose::RuleHit> Hit = {};

    AdminResponse Answer() const;
};

class FilesService
{
public:
    static constexpr int SchemaVersion = 1;
    static constexpr std::size_t MaxLoggedPathBytes = 1024;
    static constexpr std::size_t MaxRootIdBytes = 64;
    static constexpr std::size_t MaxReasonBytes = 255;
    static constexpr uint32 ReadCost = 1;

    FilesService(FileRoots& roots, Ambrose::SpaceGuard& space, FilesHooks hooks = {});

    FilesService(FilesService const&) = delete;
    FilesService& operator=(FilesService const&) = delete;

    void Register(AdminRouter& router);

    FileDecision Authorize(AdminRequest const& request, AdminRouter const& router, std::string_view root, std::string_view path, Ambrose::FileOperation operation, bool encoded = true);
    void Tune();

    static std::string_view PermissionFor(Ambrose::FileOperation operation) noexcept;
    static bool IsConf(std::string_view name) noexcept;

private:
    AdminResponse Roots();
    AdminResponse Answer(AdminRequest const& request, AdminRouter const& router);
    AdminResponse List(AdminRequest const& request, AdminRouter const& router, std::string_view root);
    AdminResponse Content(AdminRequest const& request, AdminRouter const& router, std::string_view root);
    AdminResponse Rules(std::string_view root);
    AdminResponse ReplaceRules(AdminRequest const& request);
    void Refuse(FileDecision& decision, AdminRequest const& request, int status, std::string code, std::string message, std::string_view requested, std::string const& resolved,
        Ambrose::FileOperation operation, std::string rule = {});
    AuditEvent Event(AdminRequest const& request, std::string name) const;
    void Record(AuditEvent const& event);

    FileRoots& _roots;
    Ambrose::SpaceGuard& _space;
    FilesHooks _hooks;
};

#endif
