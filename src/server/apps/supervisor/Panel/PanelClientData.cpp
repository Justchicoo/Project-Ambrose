/*
 * Project Ambrose by Imjustchico
 * The client data page for the panel: which client installs ClientLocator finds with their revision, program presence and archive count, the revision each server uses, the type dump in use with its revision, executable hash, extractor version and build time and whether its fast copy is current, the message definitions, name tables and creation config loaded, the caches built per revision by kind with revision, size and build time as facts never files, the world table sets extracted from the install with row counts and extraction revision marked when it differs from the install's, and the revision following state with newest revision seen, whether data is built and any build in progress with live output; rebuilds and install switches are checked against clientdata.rebuild and clientdata.switch, run as the protected setup state and audited, and no response ever carries a client file's bytes.
 */

#include "PanelClientData.h"

#include <nlohmann/json.hpp>

std::pair<PanelClientDataResult, PanelClientData::Page> PanelClientData::GetPage(Caller const& caller)
{
    Page page;
    if (!caller.CanRead)
        return { PanelClientDataResult::PermissionDenied, std::move(page) };
    page.Installs = _backend.ListInstalls();
    page.ServerRevisions = _backend.ServerRevisions();
    page.TypeDump = _backend.TypeDumpInUse();
    page.Caches = _backend.ListCaches();
    page.WorldTableSets = _backend.ListWorldTableSets();
    page.RevisionState = _backend.RevisionState();
    return { PanelClientDataResult::Ok, std::move(page) };
}

PanelClientDataResult PanelClientData::Rebuild(Caller const& caller, std::string_view revision, std::string& error)
{
    if (!caller.CanRebuild)
        return PanelClientDataResult::PermissionDenied;
    if (_backend.IsRebuildInProgress())
        return PanelClientDataResult::RebuildInProgress;
    return _backend.Rebuild(revision, error);
}

PanelClientDataResult PanelClientData::SwitchInstall(Caller const& caller, std::string_view installId, std::string_view revision, std::string& error)
{
    if (!caller.CanSwitch)
        return PanelClientDataResult::PermissionDenied;
    if (_backend.IsRebuildInProgress())
        return PanelClientDataResult::RebuildInProgress;
    return _backend.SwitchInstall(installId, revision, error);
}

nlohmann::json PanelClientData::PageToJson(Page const& page)
{
    nlohmann::json answer = nlohmann::json::object();
    nlohmann::json installs = nlohmann::json::array();
    for (PanelClientInstallRecord const& install : page.Installs)
    {
        installs.push_back({
            { "id", install.Id },
            { "display_name", install.DisplayName },
            { "revision", install.Revision },
            { "has_program", install.HasProgram },
            { "archives", install.Archives },
            { "in_use", install.InUse },
        });
    }
    answer["installs"] = std::move(installs);
    nlohmann::json servers = nlohmann::json::array();
    for (PanelServerRevisionRecord const& server : page.ServerRevisions)
    {
        servers.push_back({
            { "server", server.Server },
            { "revision", server.Revision },
            { "install_id", server.InstallId },
        });
    }
    answer["server_revisions"] = std::move(servers);
    if (page.TypeDump)
    {
        answer["type_dump"] = {
            { "revision", page.TypeDump->Revision },
            { "executable_hash", page.TypeDump->ExecutableHash },
            { "extractor_version", page.TypeDump->ExtractorVersion },
            { "build_time", page.TypeDump->BuildTime },
            { "fast_copy_current", page.TypeDump->FastCopyCurrent },
            { "in_use", page.TypeDump->InUse },
        };
    }
    else
    {
        answer["type_dump"] = nullptr;
    }
    nlohmann::json caches = nlohmann::json::array();
    for (PanelCacheRecord const& cache : page.Caches)
    {
        caches.push_back({
            { "kind", cache.Kind },
            { "revision", cache.Revision },
            { "size_bytes", cache.SizeBytes },
            { "build_time", cache.BuildTime },
        });
    }
    answer["caches"] = std::move(caches);
    nlohmann::json sets = nlohmann::json::array();
    for (PanelWorldTableSetRecord const& set : page.WorldTableSets)
    {
        sets.push_back({
            { "name", set.Name },
            { "row_count", set.RowCount },
            { "extraction_revision", set.ExtractionRevision },
            { "revision_mismatch", set.RevisionMismatch },
        });
    }
    answer["world_table_sets"] = std::move(sets);
    answer["revision_state"] = {
        { "newest_revision_seen", page.RevisionState.NewestRevisionSeen },
        { "data_built", page.RevisionState.DataBuilt },
        { "build_in_progress", page.RevisionState.BuildInProgress },
        { "build_output", page.RevisionState.BuildOutput },
    };
    return answer;
}

nlohmann::json PanelClientData::ResultToJson(PanelClientDataResult result, std::string_view error)
{
    nlohmann::json answer = nlohmann::json::object();
    switch (result)
    {
    case PanelClientDataResult::Ok:
        answer["ok"] = true;
        break;
    case PanelClientDataResult::PermissionDenied:
        answer["ok"] = false;
        answer["error"] = "Permission denied";
        break;
    case PanelClientDataResult::NotFound:
        answer["ok"] = false;
        answer["error"] = "Not found";
        break;
    case PanelClientDataResult::RebuildInProgress:
        answer["ok"] = false;
        answer["error"] = "A rebuild is already in progress";
        break;
    case PanelClientDataResult::SwitchInProgress:
        answer["ok"] = false;
        answer["error"] = "A switch is already in progress";
        break;
    case PanelClientDataResult::SetupBusy:
        answer["ok"] = false;
        answer["error"] = "The setup state is busy";
        break;
    case PanelClientDataResult::StoreFailed:
        answer["ok"] = false;
        answer["error"] = error.empty() ? "Store failed" : std::string(error);
        break;
    }
    return answer;
}
