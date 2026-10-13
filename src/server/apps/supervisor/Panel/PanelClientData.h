/*
 * Project Ambrose by Imjustchico
 * The client data page for the panel: which client installs ClientLocator finds with their revision, program presence and archive count, the revision each server uses, the type dump in use with its revision, executable hash, extractor version and build time and whether its fast copy is current, the message definitions, name tables and creation config loaded, the caches built per revision by kind with revision, size and build time as facts never files, the world table sets extracted from the install with row counts and extraction revision marked when it differs from the install's, and the revision following state with newest revision seen, whether data is built and any build in progress with live output; rebuilds and install switches are checked against clientdata.rebuild and clientdata.switch, run as the protected setup state and audited, and no response ever carries a client file's bytes.
 */

#ifndef AMBROSE_PANELCLIENTDATA_H
#define AMBROSE_PANELCLIENTDATA_H

#include "Types.h"

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class PanelClientDataResult : uint8
{
    Ok,
    NotFound,
    PermissionDenied,
    RebuildInProgress,
    SwitchInProgress,
    SetupBusy,
    StoreFailed
};

struct PanelClientInstallRecord
{
    std::string Id;
    std::string DisplayName;
    std::string Revision;
    bool HasProgram = false;
    uint64 Archives = 0;
    bool InUse = false;
};

struct PanelTypeDumpRecord
{
    std::string Revision;
    std::string ExecutableHash;
    std::string ExtractorVersion;
    int64 BuildTime = 0;
    bool FastCopyCurrent = false;
    bool InUse = false;
};

struct PanelCacheRecord
{
    std::string Kind;
    std::string Revision;
    uint64 SizeBytes = 0;
    int64 BuildTime = 0;
};

struct PanelWorldTableSetRecord
{
    std::string Name;
    uint64 RowCount = 0;
    std::string ExtractionRevision;
    bool RevisionMismatch = false;
};

struct PanelRevisionStateRecord
{
    std::string NewestRevisionSeen;
    bool DataBuilt = false;
    bool BuildInProgress = false;
    std::string BuildOutput;
};

struct PanelServerRevisionRecord
{
    std::string Server;
    std::string Revision;
    std::string InstallId;
};

class ClientDataBackend
{
public:
    virtual ~ClientDataBackend() = default;

    virtual std::vector<PanelClientInstallRecord> ListInstalls() = 0;
    virtual std::vector<PanelServerRevisionRecord> ServerRevisions() = 0;
    virtual std::optional<PanelTypeDumpRecord> TypeDumpInUse() = 0;
    virtual std::vector<PanelCacheRecord> ListCaches() = 0;
    virtual std::vector<PanelWorldTableSetRecord> ListWorldTableSets() = 0;
    virtual PanelRevisionStateRecord RevisionState() = 0;
    virtual PanelClientDataResult Rebuild(std::string_view revision, std::string& error) = 0;
    virtual PanelClientDataResult SwitchInstall(std::string_view installId, std::string_view revision, std::string& error) = 0;
    virtual bool IsRebuildInProgress() = 0;
};

class PanelClientData
{
public:
    explicit PanelClientData(ClientDataBackend& backend) : _backend(backend) {}

    struct Caller
    {
        int64 UserId = 0;
        std::string Username;
        bool CanRead = false;
        bool CanRebuild = false;
        bool CanSwitch = false;
    };

    struct Page
    {
        std::vector<PanelClientInstallRecord> Installs;
        std::vector<PanelServerRevisionRecord> ServerRevisions;
        std::optional<PanelTypeDumpRecord> TypeDump;
        std::vector<PanelCacheRecord> Caches;
        std::vector<PanelWorldTableSetRecord> WorldTableSets;
        PanelRevisionStateRecord RevisionState;
    };

    std::pair<PanelClientDataResult, Page> GetPage(Caller const& caller);
    PanelClientDataResult Rebuild(Caller const& caller, std::string_view revision, std::string& error);
    PanelClientDataResult SwitchInstall(Caller const& caller, std::string_view installId, std::string_view revision, std::string& error);

    static nlohmann::json PageToJson(Page const& page);
    static nlohmann::json ResultToJson(PanelClientDataResult result, std::string_view error);

private:
    ClientDataBackend& _backend;
};

#endif
