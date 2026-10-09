/*
 * Project Ambrose by Imjustchico
 * Answers the file routes. The gate asks for the operation's permission as a caller beyond its route is asked, holding a relayed token to what was forwarded, then refuses in a fixed order and records every refusal of a path, whether the parse, a protected pattern, the jail, a secret file, the policy or a second hard link refused it, as file:path.refused with the host path it would have reached, while the answer names only the code, the reason, the root and the rule; a listing hides protected entries and secret files before it counts, filters, sorts and pages, and gives the policy at that folder and the rule on each row; a read hands back a window no larger than Files.ReadMaxBytes cut on a character boundary, text only when the bytes are UTF-8 with no NUL, an ETag of the content's SHA-256 and the file's identity when the whole file fits, and a configuration file, or a copy of one such as a .conf.bak, whole with every secret value masked unless the caller asked to see them and may, which is recorded with the keys shown and never a value; an owner's protected patterns are checked line by line, saved with their audit row and the roots rebuilt; an upload link HMAC-signs its root, destination, replacement choice and expiry, is bound to one pending nonce and is consumed once; each action answers only its own method, so no GET writes; a download is capped at Files.DownloadMaxBytes; and a batch refuses a file an earlier entry moves away, renames without reading the file into memory, hashing it in windows instead, and copies or moves a file out of its root only when that root would let it be downloaded.
 */

#include "FilesService.h"
#include "Base64.h"
#include "ConfigMgr.h"
#include "CryptoRandom.h"
#include "FolderPage.h"
#include "Hmac.h"
#include "LogRedaction.h"
#include "PanelFileRules.h"
#include "SHA256.h"
#include "Settings.h"
#include "StringUtil.h"
#include "Utf.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>

namespace
{
    using Ambrose::FileOperation;
    using Ambrose::FilePolicy;
    using Json = nlohmann::json;

    constexpr std::string_view Prefix = "/api/files/";

    std::string Dump(Json const& body)
    {
        return body.dump(-1, ' ', false, Json::error_handler_t::replace);
    }

    bool IsRootId(std::string_view id) noexcept
    {
        return !id.empty() && id.size() <= FilesService::MaxRootIdBytes
            && std::all_of(id.begin(), id.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; });
    }

    Json NullOr(std::string const& text)
    {
        return text.empty() ? Json(nullptr) : Json(text);
    }

    AdminResponse Problem(int status, std::string_view code, std::string const& message, std::string_view root = {})
    {
        Json body;
        body["error"] = std::string(code);
        body["message"] = message;
        if (!root.empty())
            body["root"] = std::string(root);
        return AdminResponse::Json(status, Dump(body));
    }

    std::string_view OriginName(Ambrose::RuleOrigin origin) noexcept
    {
        return origin == Ambrose::RuleOrigin::BuiltIn ? "built_in" : "operator";
    }

    Json PolicyJson(FilePolicy const& policy, std::optional<Ambrose::RuleHit> const& hit)
    {
        Json body;
        body["summary"] = policy.Summary();
        body["client_derived"] = policy.IsClientDerived() || (hit && hit->Effect == Ambrose::RuleEffect::ClientDerived);
        Json operations = Json::object();
        bool readOnly = true;
        for (FileOperation const operation : FilePolicy::Operations())
        {
            Ambrose::OperationVerdict const verdict = policy.DecideAt(hit, operation);
            Json one;
            one["allowed"] = verdict.Allowed;
            if (!verdict.Allowed)
            {
                one["code"] = verdict.Code;
                one["reason"] = verdict.Reason;
                one["rule"] = NullOr(verdict.Rule);
            }
            else if (FilePolicy::Changes(operation))
                readOnly = false;
            operations[std::string(FilePolicy::NameOf(operation))] = std::move(one);
        }
        body["read_only"] = readOnly;
        body["operations"] = std::move(operations);
        return body;
    }

    Json HitJson(std::optional<Ambrose::RuleHit> const& hit)
    {
        if (!hit)
            return Json(nullptr);
        Json body;
        body["effect"] = std::string(Ambrose::PathRules::EffectName(hit->Effect));
        body["pattern"] = hit->Pattern;
        body["origin"] = std::string(OriginName(hit->Origin));
        body["why"] = hit->Why;
        return body;
    }

    Json RulesJson(FileRoot const& root)
    {
        Json body;
        body["schema"] = FilesService::SchemaVersion;
        body["root"] = root.Id;
        Json builtIn = Json::array();
        for (Ambrose::PathRule const& rule : root.Rules.Rules())
        {
            if (rule.Origin != Ambrose::RuleOrigin::BuiltIn)
                continue;
            Json one;
            one["pattern"] = rule.Pattern;
            one["effect"] = std::string(Ambrose::PathRules::EffectName(rule.Effect));
            one["why"] = rule.Why;
            builtIn.push_back(std::move(one));
        }
        body["built_in"] = std::move(builtIn);
        body["patterns"] = root.Rules.OperatorPatterns();
        return body;
    }

    std::string EntityTag(std::string_view bytes, Ambrose::FileIdentity const& identity)
    {
        SHA256::Digest const digest = SHA256::GetDigestOf(bytes);
        std::string hex;
        hex.reserve(digest.size() * 2);
        for (uint8 const byte : digest)
            hex += fmt::format("{:02x}", byte);
        return fmt::format("\"{}-{:x}-{:016x}{:016x}\"", hex, identity.Volume, identity.High, identity.Low);
    }

    std::string Hex(SHA256::Digest const& digest)
    {
        std::string hex;
        hex.reserve(digest.size() * 2);
        for (uint8 const byte : digest)
            hex += fmt::format("{:02x}", byte);
        return hex;
    }

    std::string Hash(std::string_view bytes)
    {
        return Hex(SHA256::GetDigestOf(bytes));
    }

    std::size_t CompleteCharacters(std::string_view bytes) noexcept
    {
        std::size_t const size = bytes.size();
        for (std::size_t back = 1; back <= 4 && back <= size; ++back)
        {
            unsigned char const byte = static_cast<unsigned char>(bytes[size - back]);
            if ((byte & 0xC0) == 0x80)
                continue;
            std::size_t const needs = byte >= 0xF0 ? 4 : byte >= 0xE0 ? 3 : byte >= 0xC0 ? 2 : 1;
            return needs > back ? size - back : size;
        }
        return size;
    }

    AdminResponse Failed(Ambrose::JailError const& failure, std::string_view root)
    {
        switch (failure.Failure)
        {
            case Ambrose::JailFailure::Missing:
                return Problem(404, "not_found", failure.Message, root);
            case Ambrose::JailFailure::Conflict:
                return Problem(409, failure.Code, failure.Message, root);
            case Ambrose::JailFailure::Refused:
                return Problem(409, failure.Code, failure.Message, root);
            case Ambrose::JailFailure::NoSpace:
                return Problem(507, failure.Code, failure.Message, root);
            case Ambrose::JailFailure::None:
            case Ambrose::JailFailure::Failed:
                break;
        }
        return Problem(failure.Code == "denied" ? 403 : 500, failure.Code.empty() ? std::string_view("io") : std::string_view(failure.Code), failure.Message, root);
    }

    bool HashOf(Ambrose::JailEntry const& entry, std::string& hex, Ambrose::JailError& error)
    {
        constexpr std::size_t Window = 1u << 20;
        SHA256 hash;
        uint64 offset = 0;
        std::string bytes;
        while (offset < entry.Stat.Size)
        {
            if (!Ambrose::FileJail::Read(entry, offset, Window, bytes, error))
                return false;
            if (bytes.empty())
                break;
            hash.Update(std::string_view(bytes));
            offset += bytes.size();
        }
        hex = Hex(hash.Finalize());
        return true;
    }

    std::string SafeAttachmentName(std::string_view name)
    {
        std::string safe;
        safe.reserve(name.size());
        for (unsigned char character : name)
            safe.push_back(character >= 0x20 && character < 0x7f && character != '"' && character != '\\' && character != ';' ? static_cast<char>(character) : '_');
        return safe.empty() ? "download" : safe;
    }
}

AdminResponse FileDecision::Answer() const
{
    Json body;
    body["error"] = Code;
    body["message"] = Message;
    if (!RootId.empty())
        body["root"] = RootId;
    if (!Rule.empty())
        body["rule"] = Rule;
    return AdminResponse::Json(Status, Dump(body));
}

FilesService::FilesService(FileRoots& roots, Ambrose::SpaceGuard& space, FilesHooks hooks) : _roots(roots), _space(space), _hooks(std::move(hooks))
{
    _uploadSigningKey = Ambrose::Crypto::GetRandomArray<32>();
}

std::string_view FilesService::PermissionFor(FileOperation operation) noexcept
{
    switch (operation)
    {
        case FileOperation::List: return "files.list";
        case FileOperation::Read:
        case FileOperation::Preview: return "files.read";
        case FileOperation::Download:
        case FileOperation::Share: return "files.download";
        case FileOperation::Archive:
        case FileOperation::Extract: return "files.archive";
        case FileOperation::Write:
        case FileOperation::Create:
        case FileOperation::Rename:
        case FileOperation::Move:
        case FileOperation::Copy:
        case FileOperation::Truncate: return "files.write";
        case FileOperation::Upload: return "files.upload";
        case FileOperation::Delete: return "files.delete";
        case FileOperation::Permissions: return "files.permissions";
        case FileOperation::Sftp: return "files.sftp";
        case FileOperation::Pull: break;
    }
    return "files.pull";
}

bool FilesService::IsConf(std::string_view name) noexcept
{
    constexpr std::string_view Marker = ".conf";
    for (std::size_t at = 1; at + Marker.size() <= name.size(); ++at)
    {
        if (!Ambrose::EqualsIgnoreCase(name.substr(at, Marker.size()), Marker))
            continue;
        std::size_t const after = at + Marker.size();
        if (after == name.size() || !std::isalnum(static_cast<unsigned char>(name[after])))
            return true;
    }
    return false;
}

void FilesService::Register(AdminRouter& router)
{
    AdminRouter* const routes = &router;
    router.AddPublic("PUT", "/api/files/upload", [this, routes](AdminRequest const& request) { return UploadWithLink(request, *routes); });
    router.AddGuarded("GET", "/api/files", "files.list", [this](AdminRequest const&) { return Roots(); });
    router.AddDynamicGuardedPrefix("GET", std::string(Prefix), "files.list", [](AdminRequest const& request)
    {
        std::string_view const rest = std::string_view(request.Path).substr(Prefix.size());
        std::size_t const slash = rest.find('/');
        if (slash == std::string_view::npos)
            return std::string("files.list");
        std::string_view const action = rest.substr(slash + 1);
        if (action == "content")
            return std::string("files.read");
        if (action == "download")
            return std::string("files.download");
        return std::string("files.list");
    }, [this, routes](AdminRequest const& request) { return Answer(request, *routes); });
    router.AddDynamicGuardedPrefix("PUT", std::string(Prefix), "files.roots", [](AdminRequest const& request)
    {
        return std::string_view(request.Path).ends_with("/upload") ? std::string("files.upload") : std::string("files.roots");
    }, [this, routes](AdminRequest const& request)
    {
        if (std::string_view(request.Path).ends_with("/upload"))
            return Answer(request, *routes);
        return ReplaceRules(request);
    });
    router.AddDynamicGuardedPrefix("POST", std::string(Prefix), "files.write", [](AdminRequest const& request)
    {
        return std::string_view(request.Path).ends_with("/upload-link") ? std::string("files.upload") : std::string("files.write");
    }, [this, routes](AdminRequest const& request) { return Answer(request, *routes); });
}

AuditEvent FilesService::Event(AdminRequest const& request, std::string name) const
{
    AuditEvent event;
    event.Name = std::move(name);
    event.Actor = request.Principal == "token" ? AuditActor::Token : AuditActor::User;
    event.ActorId = request.Principal;
    event.ActorName = _hooks.NameOf ? _hooks.NameOf(request) : request.Principal;
    event.Address = request.RemoteAddress;
    event.UserAgent = request.UserAgent;
    return event;
}

void FilesService::Record(AuditEvent const& event)
{
    std::string error = "no panel store is open to record it in";
    if (_hooks.Record && _hooks.Record(event, error))
        return;
    if (_hooks.Log)
        _hooks.Log(fmt::format("{} by {} from {} was not recorded ({}): {}", event.Name, event.ActorName.empty() ? event.ActorId : event.ActorName, event.Address, error, event.Properties));
}

void FilesService::Tune()
{
    uint64 const bytes = sSettings.Get<uint64>("Files.MinFreeBytes");
    uint64 const percent = sSettings.Get<uint64>("Files.MinFreePercent");
    _space.SetMinimum(bytes, static_cast<uint32>(std::min<uint64>(percent, 100)));
}

void FilesService::Refuse(FileDecision& decision, AdminRequest const& request, int status, std::string code, std::string message, std::string_view requested, std::string const& resolved,
    FileOperation operation, std::string rule)
{
    decision.Allowed = false;
    decision.Status = status;
    decision.Code = std::move(code);
    decision.Message = std::move(message);
    decision.Rule = std::move(rule);
    AuditEvent event = Event(request, "file:path.refused");
    event.Result = AuditResult::Refused;
    event.Reason = decision.Message;
    Json properties;
    properties["root"] = decision.RootId;
    properties["operation"] = std::string(FilePolicy::NameOf(operation));
    properties["requested"] = Ambrose::ForLog(requested, MaxLoggedPathBytes);
    properties["resolved"] = resolved;
    properties["reason"] = decision.Code;
    properties["rule"] = NullOr(decision.Rule);
    properties["request"] = request.Id;
    event.Properties = Dump(properties);
    event.On("file", fmt::format("{}:{}", decision.RootId, Ambrose::ForLog(requested, MaxLoggedPathBytes)));
    Record(event);
}

FileDecision FilesService::Authorize(AdminRequest const& request, AdminRouter const& router, std::string_view rootId, std::string_view path, FileOperation operation, bool encoded)
{
    FileDecision decision;
    decision.Set = _roots.Get();
    std::string_view const permission = PermissionFor(operation);
    PermissionVerdict verdict = PermissionVerdict::Allowed;
    if (!router.Permits(request, permission))
    {
        verdict = router.MayI(request, permission);
        if (verdict == PermissionVerdict::Allowed)
            verdict = PermissionVerdict::Forbidden;
    }
    switch (verdict)
    {
        case PermissionVerdict::Allowed:
            break;
        case PermissionVerdict::OutOfScope:
            decision.Status = 404;
            decision.Code = "not_found";
            decision.Message = fmt::format("The supervisor has nothing at {}", request.Path);
            return decision;
        case PermissionVerdict::Forbidden:
            decision.Status = 403;
            decision.Code = "forbidden";
            decision.Message = fmt::format("This account is not allowed to {}", permission);
            return decision;
    }
    if (IsRootId(rootId))
    {
        decision.RootId = std::string(rootId);
        decision.Root = decision.Set->Find(rootId);
    }
    if (!decision.Root)
    {
        decision.Status = 404;
        decision.Code = "unknown_root";
        decision.Message = "The supervisor has no file root by that name";
        return decision;
    }
    FileRoot const& root = *decision.Root;
    if (!root.Present())
    {
        decision.Status = 404;
        decision.Code = "root_absent";
        decision.Message = root.Problem;
        return decision;
    }

    Ambrose::JailPathResult const parsed = encoded ? Ambrose::JailPaths::ParseQuery(path) : Ambrose::JailPaths::ParseText(path);
    if (!parsed.Ok())
    {
        Refuse(decision, request, 403, std::string(Ambrose::JailPaths::RefusalCode(parsed.Refusal)), parsed.Reason, path,
            Ambrose::FileJail::HostText(Ambrose::JailPaths::HostPathFor(root.HostPath(), parsed.Decoded)), operation);
        return decision;
    }
    decision.Path = parsed.Path;

    std::optional<Ambrose::RuleHit> const asked = root.Rules.Match(parsed.Path.Components, true);
    Ambrose::OperationVerdict const before = root.Policy.DecideAt(asked, operation);
    if (!before.Allowed)
    {
        Refuse(decision, request, 403, before.Code, before.Reason, path, Ambrose::FileJail::HostText(Ambrose::JailPaths::HostPathFor(root.HostPath(), parsed.Path.Text())), operation,
            before.Rule);
        return decision;
    }

    Ambrose::JailError failure;
    std::optional<Ambrose::JailEntry> entry = Ambrose::FileJail::Resolve(*root.Jail, parsed.Path, root.Links, failure);
    if (!entry)
    {
        if (failure.Failure == Ambrose::JailFailure::Refused)
        {
            Refuse(decision, request, 403, failure.Code, failure.Message, path, failure.Resolved, operation);
            return decision;
        }
        AdminResponse const answer = Failed(failure, decision.RootId);
        decision.Status = answer.Status;
        decision.Code = failure.Code.empty() ? std::string("io") : failure.Code;
        decision.Message = failure.Message;
        return decision;
    }

    std::string const resolved = Ambrose::FileJail::HostText(entry->HostPath);
    if (decision.Set->IsSecret(entry->Stat.Identity, entry->HostPath))
    {
        Refuse(decision, request, 403, "secret", "This is one of the secret files the supervisor keeps, which lie outside every root", path, resolved, operation);
        return decision;
    }
    std::optional<Ambrose::RuleHit> hit = root.Rules.Match(entry->Components, entry->Stat.Kind == Ambrose::EntryKind::Folder);
    Ambrose::OperationVerdict const after = root.Policy.DecideAt(hit, operation);
    if (!after.Allowed)
    {
        Refuse(decision, request, 403, after.Code, after.Reason, path, resolved, operation, after.Rule);
        return decision;
    }
    if (FilePolicy::Changes(operation) && operation != FileOperation::Create && entry->Stat.Kind == Ambrose::EntryKind::File && entry->Stat.Links > 1)
    {
        Refuse(decision, request, 403, "hard_link", "This file has another name elsewhere on the disk, so the panel neither changes nor deletes it", path, resolved, operation);
        return decision;
    }
    decision.Allowed = true;
    decision.Status = 200;
    decision.Entry = std::move(entry);
    decision.Hit = std::move(hit);
    return decision;
}

AdminResponse FilesService::Roots()
{
    Tune();
    FileRoots::Snapshot const set = _roots.Get();
    Json roots = Json::array();
    for (FileRoot const& root : set->Roots)
    {
        Json body;
        body["id"] = root.Id;
        body["label"] = root.Label;
        body["kind"] = std::string(FileRootSet::KindName(root.Kind));
        body["apps"] = root.Apps;
        body["present"] = root.Present();
        body["problem"] = NullOr(root.Problem);
        body["client_derived"] = root.Policy.IsClientDerived();
        body["read_only"] = root.Policy.IsReadOnly();
        body["policy"] = PolicyJson(root.Policy, std::nullopt);
        body["volume"] = nullptr;
        if (root.Present())
        {
            std::string error;
            if (std::optional<Ambrose::VolumeFigures> const figures = _space.Figures(root.HostPath(), error))
            {
                Json volume;
                volume["name"] = figures->Space.Name;
                volume["free"] = figures->Space.Free;
                volume["total"] = figures->Space.Total;
                volume["minimum"] = figures->Minimum;
                volume["reserved"] = figures->Reserved;
                body["volume"] = std::move(volume);
            }
        }
        Json rules = Json::array();
        for (Ambrose::PathRule const& rule : root.Rules.Rules())
        {
            Json one;
            one["pattern"] = rule.Pattern;
            one["effect"] = std::string(Ambrose::PathRules::EffectName(rule.Effect));
            one["origin"] = std::string(OriginName(rule.Origin));
            one["why"] = rule.Why;
            rules.push_back(std::move(one));
        }
        body["rules"] = std::move(rules);
        roots.push_back(std::move(body));
    }
    Json apps = Json::array();
    apps.push_back({ { "name", "supervisor" }, { "program", "supervisor" } });
    for (FileRootApp const& app : set->Apps)
        apps.push_back({ { "name", app.Name }, { "program", app.Program } });
    Json answer;
    answer["schema"] = SchemaVersion;
    answer["generation"] = _roots.GetGeneration();
    answer["roots"] = std::move(roots);
    answer["apps"] = std::move(apps);
    answer["notes"] = set->Notes;
    return AdminResponse::Json(200, Dump(answer));
}

AdminResponse FilesService::Answer(AdminRequest const& request, AdminRouter const& router)
{
    std::string_view const rest = std::string_view(request.Path).substr(Prefix.size());
    std::string const method = Ambrose::ToUpper(request.Method);
    auto const only = [&request](std::string_view allowed)
    {
        AdminResponse response = AdminResponse::Problem(405, "method_not_allowed", fmt::format("{} answers {}", Ambrose::ForLog(request.Path, 256), allowed));
        response.Headers.emplace_back("Allow", std::string(allowed));
        return response;
    };
    if (rest == "batch")
        return method == "POST" ? Batch(request, router) : only("POST");
    std::size_t const slash = rest.find('/');
    if (slash != std::string_view::npos)
    {
        std::string_view const root = rest.substr(0, slash);
        std::string_view const action = rest.substr(slash + 1);
        if (action == "list")
            return method == "GET" ? List(request, router, root) : only("GET");
        if (action == "content")
            return method == "GET" ? Content(request, router, root) : only("GET");
        if (action == "download")
            return method == "GET" ? Download(request, router, root) : only("GET");
        if (action == "upload")
            return method == "PUT" ? Upload(request, router, root) : only("PUT");
        if (action == "upload-link")
            return method == "POST" ? UploadLink(request, router, root) : only("POST");
        if (action == "rules")
            return method == "GET" ? Rules(root) : only("GET, PUT");
    }
    return Problem(404, "not_found", fmt::format("The supervisor has nothing at {}", Ambrose::ForLog(request.Path, 256)));
}

AdminResponse FilesService::List(AdminRequest const& request, AdminRouter const& router, std::string_view rootId)
{
    if (std::optional<AdminResponse> held = router.Charge(request, ReadCost))
        return std::move(*held);
    std::vector<std::pair<std::string, std::string>> fields;
    Ambrose::FolderQuery query;
    if (std::optional<std::string> filter = Ambrose::JailPaths::PercentDecode(request.RawQueryValue("q").value_or(std::string_view())))
        query.Filter = std::move(*filter);
    else
        fields.emplace_back("q", "The filter holds a percent sign that is not followed by two hex digits");
    if (std::optional<Ambrose::FolderSort> const sort = Ambrose::FolderPage::ParseSort(request.Query("sort")))
        query.Sort = *sort;
    else
        fields.emplace_back("sort", "Sort by name, size, modified or kind");
    std::string_view const order = request.Query("order");
    if (order == "desc")
        query.Descending = true;
    else if (!order.empty() && order != "asc")
        fields.emplace_back("order", "Give the order as asc or desc");
    if (std::string_view const offset = request.Query("offset"); !offset.empty())
    {
        if (std::optional<uint64> const parsed = Ambrose::StringTo<uint64>(offset))
            query.Offset = static_cast<std::size_t>(std::min<uint64>(*parsed, static_cast<uint64>(SIZE_MAX)));
        else
            fields.emplace_back("offset", "Give the offset as a whole number");
    }
    if (std::string_view const limit = request.Query("limit"); !limit.empty())
    {
        std::optional<uint64> const parsed = Ambrose::StringTo<uint64>(limit);
        if (parsed && *parsed >= 1 && *parsed <= Ambrose::FolderPage::MaxLimit)
            query.Limit = static_cast<std::size_t>(*parsed);
        else
            fields.emplace_back("limit", fmt::format("Give the limit as a whole number from 1 to {}", Ambrose::FolderPage::MaxLimit));
    }
    if (!fields.empty())
        return AdminResponse::Invalid("The listing request has problems", std::move(fields));

    FileDecision decision = Authorize(request, router, rootId, request.RawQueryValue("path").value_or(std::string_view()), FileOperation::List);
    if (!decision.Allowed)
        return decision.Answer();
    Ambrose::JailEntry const& folder = *decision.Entry;
    if (folder.Stat.Kind != Ambrose::EntryKind::Folder)
        return Problem(409, "not_a_folder", "This is a file; read it rather than list it", decision.RootId);

    uint64 const most = std::max<uint64>(sSettings.Get<uint64>("Files.ListMaxEntries"), 1);
    std::vector<Ambrose::JailListed> listed;
    bool truncated = false;
    Ambrose::JailError failure;
    if (!Ambrose::FileJail::List(folder, static_cast<std::size_t>(std::min<uint64>(most, static_cast<uint64>(SIZE_MAX))), listed, truncated, failure))
        return Failed(failure, decision.RootId);

    FileRoot const& root = *decision.Root;
    std::vector<std::string> components = folder.Components;
    components.emplace_back();
    std::vector<Ambrose::JailListed> visible;
    visible.reserve(listed.size());
    for (Ambrose::JailListed& item : listed)
    {
        components.back() = item.Name;
        std::optional<Ambrose::RuleHit> const hit = root.Rules.Match(components, item.Kind == Ambrose::EntryKind::Folder);
        if (hit && hit->Effect == Ambrose::RuleEffect::Hide)
            continue;
        if (decision.Set->IsSecret(item.Identity, folder.HostPath / ConfigMgr::PathFromUtf8(item.Name)))
            continue;
        visible.push_back(std::move(item));
    }
    Ambrose::FolderPageResult const page = Ambrose::FolderPage::Build(std::move(visible), query, truncated);

    Json entries = Json::array();
    for (Ambrose::JailListed const& item : page.Entries)
    {
        components.back() = item.Name;
        Json row;
        row["name"] = item.Name;
        row["kind"] = std::string(Ambrose::FileJail::KindName(item.Kind));
        row["size"] = item.Size;
        row["modified_ms"] = item.ModifiedEpochMs;
        row["openable"] = item.Openable;
        row["problem"] = NullOr(item.Problem);
        row["rule"] = HitJson(root.Rules.Match(components, item.Kind == Ambrose::EntryKind::Folder));
        entries.push_back(std::move(row));
    }
    Json body;
    body["schema"] = SchemaVersion;
    body["root"] = root.Id;
    body["path"] = folder.Relative();
    body["modified_ms"] = folder.Stat.ModifiedEpochMs;
    body["policy"] = PolicyJson(root.Policy, decision.Hit);
    body["rule"] = HitJson(decision.Hit);
    body["entries"] = std::move(entries);
    body["total"] = page.Total;
    body["offset"] = page.Offset;
    body["limit"] = page.Limit;
    body["truncated"] = page.Truncated;
    body["sort"] = std::string(Ambrose::FolderPage::SortName(query.Sort));
    body["order"] = query.Descending ? "desc" : "asc";
    body["filter"] = query.Filter;
    return AdminResponse::Json(200, Dump(body));
}

AdminResponse FilesService::Content(AdminRequest const& request, AdminRouter const& router, std::string_view rootId)
{
    if (std::optional<AdminResponse> held = router.Charge(request, ReadCost))
        return std::move(*held);
    uint64 offset = 0;
    if (std::string_view const text = request.Query("offset"); !text.empty())
    {
        std::optional<uint64> const parsed = Ambrose::StringTo<uint64>(text);
        if (!parsed)
            return AdminResponse::Invalid("The read request has problems", { { "offset", "Give the offset as a whole number of bytes" } });
        offset = *parsed;
    }

    FileDecision decision = Authorize(request, router, rootId, request.RawQueryValue("path").value_or(std::string_view()), FileOperation::Read);
    if (!decision.Allowed)
        return decision.Answer();
    Ambrose::JailEntry const& entry = *decision.Entry;
    if (entry.Stat.Kind != Ambrose::EntryKind::File)
        return Problem(409, "not_a_file", "This is a folder; list it rather than read it", decision.RootId);

    uint64 const most = std::max<uint64>(sSettings.Get<uint64>("Files.ReadMaxBytes"), 1);
    std::string const name = entry.Components.empty() ? entry.Leaf : entry.Components.back();
    bool const conf = IsConf(name);
    uint64 const size = entry.Stat.Size;
    if (conf && size > most)
        return Problem(413, "too_large",
            fmt::format("A configuration file larger than Files.ReadMaxBytes, {} bytes, is not shown, because its secret values could not all be masked in one read", most), decision.RootId);
    if (conf && offset != 0)
        return AdminResponse::Invalid("The read request has problems", { { "offset", "A configuration file is read whole, from offset 0" } });
    if (offset > size)
        return AdminResponse::Invalid("The read request has problems", { { "offset", fmt::format("The file holds {} bytes, so an offset past that reads nothing", size) } });

    std::size_t const length = static_cast<std::size_t>(std::min<uint64>(most, size - offset));
    std::string bytes;
    Ambrose::JailError failure;
    if (!Ambrose::FileJail::Read(entry, offset, length, bytes, failure))
        return Failed(failure, decision.RootId);

    bool const ended = bytes.size() < length || offset + bytes.size() >= size;
    std::size_t kept = ended ? bytes.size() : CompleteCharacters(bytes);
    if (kept == 0)
        kept = bytes.size();
    std::string_view const window(bytes.data(), kept);
    bool const whole = offset == 0 && size <= most && bytes.size() == size;
    std::string const tag = whole ? EntityTag(bytes, entry.Stat.Identity) : std::string();
    bool const binary = window.find('\0') != std::string_view::npos || !Utf::IsValidUtf8(window);
    bool const finished = ended && kept == bytes.size();

    Json body;
    body["schema"] = SchemaVersion;
    body["root"] = decision.Root->Id;
    body["path"] = entry.Relative();
    body["name"] = name;
    body["size"] = size;
    body["modified_ms"] = entry.Stat.ModifiedEpochMs;
    body["etag"] = NullOr(tag);
    body["offset"] = offset;
    body["length"] = kept;
    body["next_offset"] = finished ? Json(nullptr) : Json(offset + kept);
    body["eof"] = finished;
    body["binary"] = binary;
    body["bom"] = !binary && offset == 0 && window.starts_with("\xEF\xBB\xBF");

    std::vector<std::string> keys;
    bool revealed = false;
    if (binary)
        body["text"] = nullptr;
    else if (conf)
    {
        std::string masked = LogRedaction::RedactConf(window, &keys);
        if (!keys.empty() && request.Query("reveal") == "1" && router.Permits(request, "settings.secrets.read"))
        {
            revealed = true;
            body["text"] = std::string(window);
            AuditEvent event = Event(request, "settings:secret.revealed");
            Json properties;
            properties["root"] = decision.Root->Id;
            properties["path"] = entry.Relative();
            properties["keys"] = keys;
            properties["request"] = request.Id;
            event.Properties = Dump(properties);
            event.Node = "supervisor";
            event.On("file", fmt::format("{}:{}", decision.Root->Id, entry.Relative()));
            for (std::string const& key : keys)
                event.On("setting", key, key);
            Record(event);
        }
        else
            body["text"] = std::move(masked);
    }
    else
        body["text"] = std::string(window);
    body["redacted"] = !revealed && !keys.empty();
    body["redacted_keys"] = revealed ? std::vector<std::string>() : keys;
    body["revealed"] = revealed;
    body["revealed_keys"] = revealed ? keys : std::vector<std::string>();

    AdminResponse response = AdminResponse::Json(200, Dump(body));
    if (!tag.empty())
        response.Headers.emplace_back("ETag", tag);
    return response;
}

AdminResponse FilesService::Download(AdminRequest const& request, AdminRouter const& router, std::string_view rootId)
{
    if (std::optional<AdminResponse> held = router.Charge(request, ReadCost))
        return std::move(*held);

    FileDecision decision = Authorize(request, router, rootId, request.RawQueryValue("path").value_or(std::string_view()), FileOperation::Download);
    if (!decision.Allowed)
        return decision.Answer();
    Ambrose::JailEntry const& entry = *decision.Entry;
    if (entry.Stat.Kind != Ambrose::EntryKind::File)
        return Problem(409, "not_a_file", "This is a folder; download a file rather than a folder", decision.RootId);

    uint64 const size = entry.Stat.Size;
    uint64 start = 0;
    uint64 end = size == 0 ? 0 : size - 1;
    bool const ranged = !request.Range.empty();
    if (ranged)
    {
        std::string_view range = request.Range;
        if (!range.starts_with("bytes=") || range.find(',') != std::string_view::npos)
        {
            AdminResponse response = Problem(416, "invalid_range", "Give one byte range in the form bytes=start-end", decision.RootId);
            response.Headers.emplace_back("Content-Range", fmt::format("bytes */{}", size));
            return response;
        }
        range.remove_prefix(6);
        std::size_t const dash = range.find('-');
        if (dash == std::string_view::npos)
        {
            AdminResponse response = Problem(416, "invalid_range", "Give one byte range in the form bytes=start-end", decision.RootId);
            response.Headers.emplace_back("Content-Range", fmt::format("bytes */{}", size));
            return response;
        }
        std::string_view const first = range.substr(0, dash);
        std::string_view const last = range.substr(dash + 1);
        if (size == 0)
        {
            AdminResponse response = Problem(416, "range_not_satisfiable", "No byte range can be read from an empty file", decision.RootId);
            response.Headers.emplace_back("Content-Range", "bytes */0");
            return response;
        }
        if (first.empty())
        {
            std::optional<uint64> const suffix = Ambrose::StringTo<uint64>(last);
            if (!suffix || *suffix == 0)
            {
                AdminResponse response = Problem(416, "invalid_range", "Give a positive suffix length in the byte range", decision.RootId);
                response.Headers.emplace_back("Content-Range", fmt::format("bytes */{}", size));
                return response;
            }
            start = *suffix >= size ? 0 : size - *suffix;
        }
        else
        {
            std::optional<uint64> const parsedStart = Ambrose::StringTo<uint64>(first);
            std::optional<uint64> const parsedEnd = last.empty() ? std::optional<uint64>(size - 1) : Ambrose::StringTo<uint64>(last);
            if (!parsedStart || !parsedEnd || *parsedStart > *parsedEnd || *parsedStart >= size)
            {
                AdminResponse response = Problem(416, "range_not_satisfiable", "The requested byte range does not overlap this file", decision.RootId);
                response.Headers.emplace_back("Content-Range", fmt::format("bytes */{}", size));
                return response;
            }
            start = *parsedStart;
            end = std::min(*parsedEnd, size - 1);
        }
    }

    uint64 const length = size == 0 ? 0 : end - start + 1;
    uint64 const most = std::max<uint64>(sSettings.Get<uint64>("Files.DownloadMaxBytes"), 1);
    if (length > most || length > static_cast<uint64>(std::numeric_limits<std::size_t>::max()))
    {
        AdminResponse response = Problem(413, "too_large", fmt::format("One download hands out at most Files.DownloadMaxBytes, {} bytes; ask for a byte range of this {} byte file", most, size),
            decision.RootId);
        response.Headers.emplace_back("Accept-Ranges", "bytes");
        return response;
    }

    std::string bytes;
    Ambrose::JailError failure;
    if (!Ambrose::FileJail::Read(entry, start, static_cast<std::size_t>(length), bytes, failure))
        return Failed(failure, decision.RootId);
    if (bytes.size() != length)
        return Problem(409, "changed_during_read", "The file changed while it was being downloaded", decision.RootId);

    AuditEvent event = Event(request, "file:downloaded");
    Json properties;
    properties["root"] = decision.Root->Id;
    properties["path"] = entry.Relative();
    properties["offset"] = start;
    properties["length"] = bytes.size();
    properties["sha256"] = Hash(bytes);
    properties["request"] = request.Id;
    event.Properties = Dump(properties);
    event.On("file", fmt::format("{}:{}", decision.Root->Id, entry.Relative()));
    Record(event);

    AdminResponse response;
    response.Status = ranged ? 206 : 200;
    response.ContentType = "application/octet-stream";
    response.Body = std::move(bytes);
    response.Headers.emplace_back("Accept-Ranges", "bytes");
    response.Headers.emplace_back("Content-Disposition", fmt::format("attachment; filename=\"{}\"", SafeAttachmentName(entry.Leaf)));
    if (ranged)
        response.Headers.emplace_back("Content-Range", fmt::format("bytes {}-{}/{}", start, end, size));
    return response;
}

AdminResponse FilesService::Upload(AdminRequest const& request, AdminRouter const& router, std::string_view rootId)
{
    uint64 const maximum = std::max<uint64>(sSettings.Get<uint64>("Files.UploadMaxBytes"), 1);
    std::optional<std::string_view> const rawPath = request.RawQueryValue("path");
    if (!rawPath)
        return AdminResponse::Invalid("The upload request has problems", { { "path", "Give the destination file path" } });
    Ambrose::JailPathResult const parsed = Ambrose::JailPaths::ParseQuery(*rawPath);
    if (!parsed.Ok() || parsed.Path.IsRoot())
    {
        FileDecision refused = Authorize(request, router, rootId, *rawPath, FileOperation::Upload);
        if (!refused.Allowed)
            return refused.Answer();
        return Problem(403, "invalid_path", "The root itself cannot be uploaded to", rootId);
    }
    Ambrose::JailPath const targetPath = parsed.Path;
    std::string const targetText = targetPath.Text();
    FileDecision parent = Authorize(request, router, rootId, targetPath.Parent().Text(), FileOperation::Upload, false);
    if (!parent.Allowed)
        return parent.Answer();
    if (parent.Entry->Stat.Kind != Ambrose::EntryKind::Folder)
        return Problem(409, "not_a_folder", "The upload destination's parent is not a folder", parent.RootId);

    FileRoot const& root = *parent.Root;
    std::optional<Ambrose::RuleHit> const targetHit = root.Rules.Match(targetPath.Components, false);
    Ambrose::OperationVerdict const targetVerdict = root.Policy.DecideAt(targetHit, FileOperation::Upload);
    if (!targetVerdict.Allowed)
    {
        FileDecision refused;
        refused.Set = parent.Set;
        refused.Root = parent.Root;
        refused.RootId = parent.RootId;
        Refuse(refused, request, 403, targetVerdict.Code, targetVerdict.Reason, targetText,
            Ambrose::FileJail::HostText(Ambrose::JailPaths::HostPathFor(root.HostPath(), targetText)), FileOperation::Upload, targetVerdict.Rule);
        return refused.Answer();
    }

    if (request.Body.size() > maximum)
    {
        FileDecision refused;
        refused.Set = parent.Set;
        refused.Root = parent.Root;
        refused.RootId = parent.RootId;
        Refuse(refused, request, 413, "too_large", fmt::format("This upload is larger than Files.UploadMaxBytes, {} bytes", maximum), targetText,
            Ambrose::FileJail::HostText(Ambrose::JailPaths::HostPathFor(root.HostPath(), targetText)), FileOperation::Upload);
        return refused.Answer();
    }

    FileDecision existing = Authorize(request, router, rootId, targetText, FileOperation::Upload, false);
    bool const replacing = existing.Allowed;
    if (!replacing && !(existing.Status == 404 && existing.Code == "not_found"))
        return existing.Answer();
    std::string beforeHash;
    std::string previous;
    if (replacing)
    {
        if (request.Query("replace") != "1")
        {
            FileDecision refused;
            refused.Set = parent.Set;
            refused.Root = parent.Root;
            refused.RootId = parent.RootId;
            Refuse(refused, request, 409, "exists", "A file already has this name; set replace=1 to replace it", targetText,
                Ambrose::FileJail::HostText(existing.Entry->HostPath), FileOperation::Upload);
            return refused.Answer();
        }
        if (existing.Entry->Stat.Kind != Ambrose::EntryKind::File)
            return Problem(409, "not_a_file", "Only an existing file can be replaced by an upload", root.Id);
        if (existing.Entry->Stat.Size > maximum || existing.Entry->Stat.Size > std::numeric_limits<std::size_t>::max())
            return Problem(413, "too_large", "The existing file is too large to replace safely", root.Id);
        Ambrose::JailError readError;
        if (!Ambrose::FileJail::Read(*existing.Entry, 0, static_cast<std::size_t>(existing.Entry->Stat.Size), previous, readError))
            return Failed(readError, root.Id);
        beforeHash = Hash(previous);
    }

    Tune();
    Ambrose::JailError failure;
    bool created = false;
    if (!replacing)
    {
        created = Ambrose::FileJail::Create(*root.Jail, targetPath, request.Body, _space, failure);
    }
    else
    {
        created = Ambrose::FileJail::Replace(*existing.Entry, request.Body, previous, _space, failure);
    }
    if (!created)
        return Failed(failure, root.Id);

    AuditEvent event = Event(request, "file:uploaded");
    Json properties;
    properties["root"] = root.Id;
    properties["path"] = targetText;
    properties["size"] = request.Body.size();
    properties["sha256_before"] = NullOr(beforeHash);
    properties["sha256_after"] = Hash(request.Body);
    properties["request"] = request.Id;
    event.Properties = Dump(properties);
    event.On("file", fmt::format("{}:{}", root.Id, targetText));
    Record(event);
    return AdminResponse::Json(201, Dump(Json{ { "schema", SchemaVersion }, { "root", root.Id }, { "path", targetText }, { "size", request.Body.size() } }));
}

AdminResponse FilesService::UploadLink(AdminRequest const& request, AdminRouter const& router, std::string_view rootId)
{
    uint64 const maximum = std::max<uint64>(sSettings.Get<uint64>("Files.UploadMaxBytes"), 1);
    std::optional<std::string_view> const rawPath = request.RawQueryValue("path");
    if (!rawPath)
        return AdminResponse::Invalid("The upload-link request has problems", { { "path", "Give the destination file path" } });
    Ambrose::JailPathResult const parsed = Ambrose::JailPaths::ParseQuery(*rawPath);
    if (!parsed.Ok() || parsed.Path.IsRoot())
    {
        FileDecision refused = Authorize(request, router, rootId, *rawPath, FileOperation::Upload);
        if (!refused.Allowed)
            return refused.Answer();
        return Problem(403, "invalid_path", "The root itself cannot be uploaded to", rootId);
    }

    Ambrose::JailPath const targetPath = parsed.Path;
    std::string const targetText = targetPath.Text();
    FileDecision parent = Authorize(request, router, rootId, targetPath.Parent().Text(), FileOperation::Upload, false);
    if (!parent.Allowed)
        return parent.Answer();
    if (parent.Entry->Stat.Kind != Ambrose::EntryKind::Folder)
        return Problem(409, "not_a_folder", "The upload destination's parent is not a folder", parent.RootId);

    FileRoot const& root = *parent.Root;
    std::optional<Ambrose::RuleHit> const targetHit = root.Rules.Match(targetPath.Components, false);
    Ambrose::OperationVerdict const targetVerdict = root.Policy.DecideAt(targetHit, FileOperation::Upload);
    if (!targetVerdict.Allowed)
    {
        FileDecision refused;
        refused.Set = parent.Set;
        refused.Root = parent.Root;
        refused.RootId = parent.RootId;
        Refuse(refused, request, 403, targetVerdict.Code, targetVerdict.Reason, targetText,
            Ambrose::FileJail::HostText(Ambrose::JailPaths::HostPathFor(root.HostPath(), targetText)), FileOperation::Upload, targetVerdict.Rule);
        return refused.Answer();
    }

    FileDecision existing = Authorize(request, router, rootId, targetText, FileOperation::Upload, false);
    bool const replacing = existing.Allowed;
    if (!replacing && !(existing.Status == 404 && existing.Code == "not_found"))
        return existing.Answer();
    if (replacing)
    {
        if (request.Query("replace") != "1")
        {
            FileDecision refused;
            refused.Set = parent.Set;
            refused.Root = parent.Root;
            refused.RootId = parent.RootId;
            Refuse(refused, request, 409, "exists", "A file already has this name; set replace=1 to replace it", targetText,
                Ambrose::FileJail::HostText(existing.Entry->HostPath), FileOperation::Upload);
            return refused.Answer();
        }
        if (existing.Entry->Stat.Kind != Ambrose::EntryKind::File)
            return Problem(409, "not_a_file", "Only an existing file can be replaced by an upload", root.Id);
        if (existing.Entry->Stat.Size > maximum)
            return Problem(413, "too_large", "The existing file is too large to replace safely", root.Id);
    }

    auto const now = std::chrono::steady_clock::now();
    UploadGrant grant;
    grant.Root = root.Id;
    grant.RawPath = std::string(*rawPath);
    grant.Principal = request.Principal;
    grant.ForwardedActor = request.ForwardedActor;
    grant.ForwardedGrants = request.ForwardedGrants;
    grant.Replace = replacing;
    grant.ExpiresAt = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() + 120;
    grant.Expires = now + std::chrono::seconds(120);

    std::array<uint8, 32> const nonceBytes = Ambrose::Crypto::GetRandomArray<32>();
    std::string const nonce = Base64::Encode(nonceBytes, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
    Json claims{ { "expires", grant.ExpiresAt }, { "nonce", nonce }, { "path", grant.RawPath }, { "replace", grant.Replace }, { "root", grant.Root } };
    std::string const signingText = claims.dump();
    Hmac signer(CryptoHash::Algorithm::Sha256, _uploadSigningKey);
    std::vector<uint8> const signature = signer.Update(signingText).Finalize();
    std::string const token = nonce + "." + Base64::Encode(signature, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
    {
        std::lock_guard const lock(_uploadMutex);
        std::erase_if(_uploadGrants, [now](auto const& entry) { return now >= entry.second.Expires; });
        if (_uploadGrants.size() >= MaxUploadLinks)
            return Problem(429, "too_many_upload_links", "Too many upload links are waiting to be used", root.Id);
        if (!_uploadGrants.emplace(nonce, grant).second)
            return Problem(503, "upload_link_unavailable", "A unique upload link could not be made; try again", root.Id);
    }

    AuditEvent event = Event(request, "file:upload-link-issued");
    event.Properties = Dump(Json{ { "expires_at", grant.ExpiresAt }, { "path", targetText }, { "replace", replacing }, { "root", root.Id } });
    event.On("file", fmt::format("{}:{}", root.Id, targetText));
    Record(event);
    return AdminResponse::Json(201, Dump(Json{ { "schema", SchemaVersion }, { "upload_url", "/api/files/upload?token=" + token }, { "expires_in_seconds", 120 } }));
}

AdminResponse FilesService::UploadWithLink(AdminRequest const& request, AdminRouter const& router)
{
    if (std::optional<AdminResponse> held = router.Charge(request, ReadCost))
        return std::move(*held);

    std::string_view const token = request.Query("token");
    if (token.size() != 87)
        return Problem(404, "upload_link_not_found", "This upload link is invalid, expired or already used");
    std::size_t const separator = token.find('.');
    if (separator == std::string_view::npos || separator == 0 || separator + 1 == token.size())
        return Problem(404, "upload_link_not_found", "This upload link is invalid, expired or already used");
    std::string const nonce(token.substr(0, separator));
    std::optional<std::vector<uint8>> const nonceBytes = Base64::Decode(nonce, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
    std::optional<std::vector<uint8>> const signature = Base64::Decode(token.substr(separator + 1), Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
    if (!nonceBytes || nonceBytes->size() != 32 || !signature || signature->size() != 32)
        return Problem(404, "upload_link_not_found", "This upload link is invalid, expired or already used");

    UploadGrant grant;
    auto const now = std::chrono::steady_clock::now();
    {
        std::lock_guard const lock(_uploadMutex);
        auto const found = _uploadGrants.find(nonce);
        if (found == _uploadGrants.end() || now >= found->second.Expires)
        {
            if (found != _uploadGrants.end())
                _uploadGrants.erase(found);
            return Problem(404, "upload_link_not_found", "This upload link is invalid, expired or already used");
        }
        Json claims{ { "expires", found->second.ExpiresAt }, { "nonce", nonce }, { "path", found->second.RawPath }, { "replace", found->second.Replace }, { "root", found->second.Root } };
        std::string const signingText = claims.dump();
        std::span<uint8 const> const signedBytes(reinterpret_cast<uint8 const*>(signingText.data()), signingText.size());
        if (!Hmac::Verify(CryptoHash::Algorithm::Sha256, _uploadSigningKey, signedBytes, *signature))
            return Problem(404, "upload_link_not_found", "This upload link is invalid, expired or already used");
        grant = std::move(found->second);
        _uploadGrants.erase(found);
    }

    AdminRequest uploaded = request;
    uploaded.Path = fmt::format("{}{}/upload", Prefix, grant.Root);
    uploaded.RawQuery = "path=" + grant.RawPath;
    uploaded.QueryValues.clear();
    if (grant.Replace)
    {
        uploaded.RawQuery += "&replace=1";
        uploaded.QueryValues.emplace("replace", "1");
    }
    uploaded.Principal = std::move(grant.Principal);
    uploaded.ForwardedActor = std::move(grant.ForwardedActor);
    uploaded.ForwardedGrants = std::move(grant.ForwardedGrants);
    return Upload(uploaded, router, grant.Root);
}

AdminResponse FilesService::Batch(AdminRequest const& request, AdminRouter const& router)
{
    Json body = Json::parse(request.Body, nullptr, false);
    if (!body.is_object() || !body.contains("operations") || !body["operations"].is_array() || body["operations"].empty() || body["operations"].size() > 100)
        return Problem(400, "invalid_batch", "Give an operations array holding from 1 to 100 file changes");

    struct Planned
    {
        std::string Action;
        std::string SourcePath;
        std::string TargetPath;
        FileDecision Source;
        FileDecision TargetParent;
        Ambrose::JailPath Target;
        std::string Contents;
        std::string BeforeHash;
        FileOperation Operation = FileOperation::Move;
    };
    std::vector<Planned> plan;
    plan.reserve(body["operations"].size());
    std::vector<std::pair<std::string, Ambrose::JailPath>> destinations;
    std::vector<std::pair<std::string, Ambrose::JailPath>> taken;
    bool const insensitive = Ambrose::PathRules::PlatformCase() == Ambrose::CaseMode::Insensitive;
    auto const samePath = [insensitive](std::vector<std::pair<std::string, Ambrose::JailPath>> const& list, std::string const& root, Ambrose::JailPath const& path)
    {
        for (auto const& [listedRoot, listed] : list)
        {
            bool same = listedRoot == root && listed.Components.size() == path.Components.size();
            for (std::size_t part = 0; same && part < listed.Components.size(); ++part)
                same = insensitive ? Ambrose::EqualsIgnoreCase(listed.Components[part], path.Components[part]) : listed.Components[part] == path.Components[part];
            if (same)
                return true;
        }
        return false;
    };
    uint64 totalBytes = 0;
    uint64 const maximumBytes = std::max<uint64>(sSettings.Get<uint64>("Files.UploadMaxBytes"), 1);
    Json done = Json::array();

    auto failure = [&done](int status, std::string_view code, std::string const& message, std::size_t index, std::string const& path, Json const& operations)
    {
        Json answer;
        answer["error"] = std::string(code);
        answer["message"] = message;
        answer["failed_index"] = index;
        answer["failed_path"] = path;
        answer["done"] = done;
        answer["not_done"] = Json::array();
        for (std::size_t rest = index; rest < operations.size(); ++rest)
            answer["not_done"].push_back(operations[rest]);
        return AdminResponse::Json(status, Dump(answer));
    };

    for (std::size_t index = 0; index < body["operations"].size(); ++index)
    {
        Json const& item = body["operations"][index];
        if (!item.is_object() || !item.contains("action") || !item["action"].is_string()
            || !item.contains("sourceRoot") || !item["sourceRoot"].is_string()
            || !item.contains("sourcePath") || !item["sourcePath"].is_string()
            || !item.contains("targetRoot") || !item["targetRoot"].is_string()
            || !item.contains("targetPath") || !item["targetPath"].is_string())
            return failure(400, "invalid_operation", "Each change needs action, sourceRoot, sourcePath, targetRoot and targetPath", index, {}, body["operations"]);

        Planned operation;
        operation.Action = item["action"].get<std::string>();
        operation.SourcePath = item["sourcePath"].get<std::string>();
        operation.TargetPath = item["targetPath"].get<std::string>();
        std::string const sourceRoot = item["sourceRoot"].get<std::string>();
        std::string const targetRoot = item["targetRoot"].get<std::string>();
        if (operation.Action == "rename")
            operation.Operation = FileOperation::Rename;
        else if (operation.Action == "move")
            operation.Operation = FileOperation::Move;
        else if (operation.Action == "copy")
            operation.Operation = FileOperation::Copy;
        else
            return failure(400, "invalid_action", "Use rename, move or copy for a file change", index, operation.SourcePath, body["operations"]);
        if (operation.Action == "rename" && sourceRoot != targetRoot)
            return failure(400, "invalid_roots", "A rename stays within one root; use move to change roots", index, operation.TargetPath, body["operations"]);

        operation.Source = Authorize(request, router, sourceRoot, operation.SourcePath, operation.Operation, false);
        if (!operation.Source.Allowed)
            return failure(operation.Source.Status, operation.Source.Code, operation.Source.Message, index, operation.SourcePath, body["operations"]);
        if (operation.Source.Entry->Stat.Kind != Ambrose::EntryKind::File)
            return failure(409, "not_a_file", "Only files can be changed in a batch", index, operation.SourcePath, body["operations"]);
        if (samePath(taken, operation.Source.RootId, operation.Source.Path))
            return failure(409, "source_moved", "An earlier entry in this batch moves this file away", index, operation.SourcePath, body["operations"]);
        bool const relinks = operation.Action == "rename" || (operation.Action == "move" && sourceRoot == targetRoot);
        uint64 const size = operation.Source.Entry->Stat.Size;
        if (!relinks && (size > maximumBytes || totalBytes > maximumBytes - size || size > std::numeric_limits<std::size_t>::max()))
            return failure(413, "too_large", fmt::format("The batch exceeds Files.UploadMaxBytes, {} bytes", maximumBytes), index, operation.SourcePath, body["operations"]);

        Ambrose::JailPathResult const targetParsed = Ambrose::JailPaths::ParseText(operation.TargetPath);
        if (!targetParsed.Ok() || targetParsed.Path.IsRoot())
            return failure(400, "invalid_path", targetParsed.Reason.empty() ? "The root itself cannot be a batch destination" : targetParsed.Reason, index, operation.TargetPath, body["operations"]);
        operation.Target = targetParsed.Path;
        operation.TargetPath = operation.Target.Text();
        if (samePath(destinations, targetRoot, operation.Target))
            return failure(409, "duplicate_target", "Two batch entries name the same destination", index, operation.TargetPath, body["operations"]);
        destinations.emplace_back(targetRoot, operation.Target);
        operation.TargetParent = Authorize(request, router, targetRoot, operation.Target.Parent().Text(), operation.Operation, false);
        if (!operation.TargetParent.Allowed)
            return failure(operation.TargetParent.Status, operation.TargetParent.Code, operation.TargetParent.Message, index, operation.TargetPath, body["operations"]);
        if (operation.TargetParent.Entry->Stat.Kind != Ambrose::EntryKind::Folder)
            return failure(409, "not_a_folder", "The destination's parent is not a folder", index, operation.TargetPath, body["operations"]);

        FileRoot const& destination = *operation.TargetParent.Root;
        std::optional<Ambrose::RuleHit> const targetHit = destination.Rules.Match(operation.Target.Components, false);
        Ambrose::OperationVerdict const targetVerdict = destination.Policy.DecideAt(targetHit, operation.Operation);
        if (!targetVerdict.Allowed)
        {
            FileDecision refused;
            refused.Set = operation.TargetParent.Set;
            refused.Root = operation.TargetParent.Root;
            refused.RootId = operation.TargetParent.RootId;
            Refuse(refused, request, 403, targetVerdict.Code, targetVerdict.Reason, operation.TargetPath,
                Ambrose::FileJail::HostText(Ambrose::JailPaths::HostPathFor(destination.HostPath(), operation.TargetPath)), operation.Operation, targetVerdict.Rule);
            return failure(403, targetVerdict.Code, targetVerdict.Reason, index, operation.TargetPath, body["operations"]);
        }

        FileDecision const existing = Authorize(request, router, targetRoot, operation.TargetPath, operation.Operation, false);
        if (existing.Allowed)
            return failure(409, "exists", "The batch destination already exists", index, operation.TargetPath, body["operations"]);
        if (!(existing.Status == 404 && existing.Code == "not_found"))
            return failure(existing.Status, existing.Code, existing.Message, index, operation.TargetPath, body["operations"]);

        if (sourceRoot != targetRoot)
        {
            Ambrose::OperationVerdict const leaving = operation.Source.Root->Policy.DecideAt(operation.Source.Hit, FileOperation::Download);
            if (!leaving.Allowed)
            {
                FileDecision refused;
                refused.Set = operation.Source.Set;
                refused.Root = operation.Source.Root;
                refused.RootId = operation.Source.RootId;
                Refuse(refused, request, 403, leaving.Code, leaving.Reason, operation.SourcePath, Ambrose::FileJail::HostText(operation.Source.Entry->HostPath), operation.Operation,
                    leaving.Rule);
                return failure(403, leaving.Code, leaving.Reason, index, operation.SourcePath, body["operations"]);
            }
        }

        operation.Contents.clear();
        Ambrose::JailError readError;
        if (relinks)
        {
            if (!HashOf(*operation.Source.Entry, operation.BeforeHash, readError))
                return failure(500, readError.Code, readError.Message, index, operation.SourcePath, body["operations"]);
        }
        else
        {
            if (!Ambrose::FileJail::Read(*operation.Source.Entry, 0, static_cast<std::size_t>(size), operation.Contents, readError))
                return failure(500, readError.Code, readError.Message, index, operation.SourcePath, body["operations"]);
            if (operation.Contents.size() != size)
                return failure(409, "changed", "The file changed while the batch was being checked", index, operation.SourcePath, body["operations"]);
            operation.BeforeHash = Hash(operation.Contents);
            totalBytes += size;
        }
        if (operation.Action != "copy")
            taken.emplace_back(operation.Source.RootId, operation.Source.Path);
        plan.push_back(std::move(operation));
    }

    Tune();
    for (std::size_t index = 0; index < plan.size(); ++index)
    {
        Planned& operation = plan[index];
        FileRoot const& sourceRoot = *operation.Source.Root;
        FileRoot const& targetRoot = *operation.TargetParent.Root;
        Ambrose::JailError applyError;
        bool applied = false;
        if (operation.Action == "rename" || (operation.Action == "move" && sourceRoot.Id == targetRoot.Id))
        {
            applied = Ambrose::FileJail::Rename(*operation.Source.Entry, *operation.TargetParent.Entry, operation.Target.Leaf(), false, applyError);
        }
        else
        {
            applied = Ambrose::FileJail::Create(*targetRoot.Jail, operation.Target, operation.Contents, _space, applyError);
            if (applied && operation.Action == "move")
            {
                if (!Ambrose::FileJail::Remove(*operation.Source.Entry, applyError))
                {
                    Ambrose::JailError rollback;
                    std::optional<Ambrose::JailEntry> const created = Ambrose::FileJail::Resolve(*targetRoot.Jail, operation.Target, targetRoot.Links, rollback);
                    if (created && !Ambrose::FileJail::Remove(*created, rollback))
                        return failure(500, "partial_move", fmt::format("{}; rollback failed: {}", applyError.Message, rollback.Message), index, operation.TargetPath, body["operations"]);
                    applied = false;
                }
            }
        }
        if (!applied)
            return failure(applyError.Failure == Ambrose::JailFailure::None ? 500 : 409, applyError.Code, applyError.Message, index, operation.TargetPath, body["operations"]);

        AuditEvent event = Event(request, "file:batch.changed");
        Json properties;
        properties["action"] = operation.Action;
        properties["source_root"] = sourceRoot.Id;
        properties["source_path"] = operation.SourcePath;
        properties["target_root"] = targetRoot.Id;
        properties["target_path"] = operation.TargetPath;
        properties["sha256_before"] = operation.BeforeHash;
        properties["sha256_after"] = operation.BeforeHash;
        properties["request"] = request.Id;
        event.Properties = Dump(properties);
        event.On("file", fmt::format("{}:{}", sourceRoot.Id, operation.SourcePath));
        event.On("file", fmt::format("{}:{}", targetRoot.Id, operation.TargetPath));
        Record(event);
        done.push_back({ { "action", operation.Action }, { "sourceRoot", sourceRoot.Id }, { "sourcePath", operation.SourcePath },
            { "targetRoot", targetRoot.Id }, { "targetPath", operation.TargetPath } });
    }

    return AdminResponse::Json(200, Dump(Json{ { "schema", SchemaVersion }, { "done", std::move(done) }, { "not_done", Json::array() } }));
}

AdminResponse FilesService::Rules(std::string_view rootId)
{
    FileRoots::Snapshot const set = _roots.Get();
    FileRoot const* const root = IsRootId(rootId) ? set->Find(rootId) : nullptr;
    if (!root)
        return Problem(404, "unknown_root", "The supervisor has no file root by that name");
    return AdminResponse::Json(200, Dump(RulesJson(*root)));
}

AdminResponse FilesService::ReplaceRules(AdminRequest const& request)
{
    constexpr std::string_view Tail = "/rules";
    std::string_view const rest = std::string_view(request.Path).substr(Prefix.size());
    if (!rest.ends_with(Tail))
        return Problem(404, "not_found", fmt::format("The supervisor has nothing at {}", Ambrose::ForLog(request.Path, 256)));
    std::string_view const rootId = rest.substr(0, rest.size() - Tail.size());
    FileRoots::Snapshot const set = _roots.Get();
    FileRoot const* const root = IsRootId(rootId) ? set->Find(rootId) : nullptr;
    if (!root)
        return Problem(404, "unknown_root", "The supervisor has no file root by that name");

    Json const body = request.Body.empty() ? Json() : Json::parse(request.Body, nullptr, false);
    if (!body.is_object() || !body.contains("patterns") || !body["patterns"].is_array())
        return AdminResponse::Invalid("Protected patterns are replaced with a patterns list", { { "patterns", "Give every pattern the root should keep, one per entry" } });
    std::vector<std::string> lines;
    for (Json const& line : body["patterns"])
    {
        if (!line.is_string())
            return AdminResponse::Invalid("Protected patterns are replaced with a patterns list", { { "patterns", "Every entry is one pattern, written as text" } });
        lines.push_back(line.get<std::string>());
    }
    std::string reason;
    if (body.contains("reason"))
    {
        if (!body["reason"].is_string() || body["reason"].get_ref<std::string const&>().size() > MaxReasonBytes)
            return AdminResponse::Invalid("The reason is not usable", { { "reason", fmt::format("Give the reason as text of at most {} bytes", MaxReasonBytes) } });
        reason = body["reason"].get<std::string>();
    }
    std::vector<PanelFileRuleProblem> const problems = PanelFileRules::Validate(lines);
    if (!problems.empty())
    {
        std::string listed;
        for (PanelFileRuleProblem const& problem : problems)
            listed += (listed.empty() ? "" : "; ") + (problem.Line == 0 ? problem.Message : fmt::format("Line {}: {}", problem.Line, problem.Message));
        return AdminResponse::Invalid("The protected patterns have problems", { { "patterns", listed } });
    }
    std::vector<std::string> const patterns = PanelFileRules::Normalise(lines);

    AuditEvent event = Event(request, "file:rules.changed");
    event.Reason = reason;
    Json properties;
    properties["root"] = root->Id;
    properties["before"] = root->Rules.OperatorPatterns().size();
    properties["after"] = patterns.size();
    properties["request"] = request.Id;
    event.Properties = Dump(properties);
    event.On("file_root", root->Id, root->Label);
    std::string const id = root->Id;
    if (!_hooks.SaveRules)
        return Problem(503, "store_unavailable", "Protected patterns are kept in the panel store, which this supervisor has not opened; set Panel.Enable = 1", id);
    std::string error;
    if (!_hooks.SaveRules(request, event, id, patterns, error))
        return Problem(503, "store_unavailable", fmt::format("The protected patterns were not saved: {}", error), id);

    std::vector<std::string> errors;
    bool const rebuilt = !_hooks.Rebuild || _hooks.Rebuild(errors);
    FileRoots::Snapshot const now = _roots.Get();
    FileRoot const* const fresh = now->Find(id);
    Json answer = RulesJson(fresh ? *fresh : *root);
    answer["patterns"] = patterns;
    answer["rebuilt"] = rebuilt;
    answer["errors"] = errors;
    return AdminResponse::Json(200, Dump(answer));
}
