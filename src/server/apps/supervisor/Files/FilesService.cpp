/*
 * Project Ambrose by Imjustchico
 * Answers the file routes. The gate asks for the operation's permission as a caller beyond its route is asked, holding a relayed token to what was forwarded, then refuses in a fixed order and records every refusal of a path, whether the parse, a protected pattern, the jail, a secret file, the policy or a second hard link refused it, as file:path.refused with the host path it would have reached, while the answer names only the code, the reason, the root and the rule; a listing hides protected entries and secret files before it counts, filters, sorts and pages, and gives the policy at that folder and the rule on each row; a read hands back a window no larger than Files.ReadMaxBytes cut on a character boundary, text only when the bytes are UTF-8 with no NUL, an ETag of the content's SHA-256 and the file's identity when the whole file fits, and a configuration file, or a copy of one such as a .conf.bak, whole with every secret value masked unless the caller asked to see them and may, which is recorded with the keys shown and never a value; an owner's protected patterns are checked line by line, saved with their audit row and the roots rebuilt.
 */

#include "FilesService.h"
#include "ConfigMgr.h"
#include "FolderPage.h"
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
    router.AddGuarded("GET", "/api/files", "files.list", [this](AdminRequest const&) { return Roots(); });
    router.AddGuardedPrefix("GET", std::string(Prefix), "files.list", [this, routes](AdminRequest const& request) { return Answer(request, *routes); });
    router.AddGuardedPrefix("PUT", std::string(Prefix), "files.roots", [this](AdminRequest const& request) { return ReplaceRules(request); });
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
    std::size_t const slash = rest.find('/');
    if (slash != std::string_view::npos)
    {
        std::string_view const root = rest.substr(0, slash);
        std::string_view const action = rest.substr(slash + 1);
        if (action == "list")
            return List(request, router, root);
        if (action == "content")
            return Content(request, router, root);
        if (action == "rules")
            return Rules(root);
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
