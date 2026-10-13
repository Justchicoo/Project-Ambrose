/*
 * Project Ambrose by Imjustchico
 * The panel's network routes: the allocations per node with their bind address, port, protocol, public alias, app, role and notes, adding one or a range of at most 1000 ports with every port proven free by binding it, assigning one to an app by writing its listen setting through the app's settings API, refusing to delete one an app uses on every path, the per-node port pool that supplies ports for new realms, and the network page per node and per app, every change audited in the same transaction that makes it.
 */

#include "Panel.h"
#include "AdminRouter.h"
#include "IpAddress.h"
#include "PanelUsers.h"
#include "PortAllocations.h"
#include "PortProbe.h"
#include "StringUtil.h"

#include <nlohmann/json.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <chrono>

namespace
{
    nlohmann::json AllocationJson(PortAllocation const& allocation)
    {
        return {
            { "id", allocation.Id },
            { "node", allocation.Node },
            { "bind_address", allocation.BindAddress },
            { "port", allocation.Port },
            { "protocol", allocation.Protocol },
            { "public_alias", allocation.PublicAlias },
            { "app", allocation.App },
            { "role", allocation.Role },
            { "notes", allocation.Notes },
            { "assigned", !allocation.App.empty() },
        };
    }

    nlohmann::json PoolJson(PortPool const& pool)
    {
        return {
            { "node", pool.Node },
            { "start_port", pool.StartPort },
            { "end_port", pool.EndPort },
        };
    }

    nlohmann::json ParseBody(AdminRequest const& request)
    {
        return request.Body.empty() ? nlohmann::json() : nlohmann::json::parse(request.Body, nullptr, false);
    }

    std::string TextOf(nlohmann::json const& body, std::string_view name)
    {
        auto const found = body.find(name);
        if (found == body.end() || !found->is_string())
            return {};
        return std::string(Ambrose::Trim(found->get_ref<std::string const&>()));
    }

    std::optional<uint16> PortOf(nlohmann::json const& body, std::string_view name)
    {
        auto const found = body.find(name);
        if (found == body.end() || !found->is_number_unsigned())
            return std::nullopt;
        unsigned long long const value = found->get<unsigned long long>();
        if (value == 0 || value > 65535)
            return std::nullopt;
        return static_cast<uint16>(value);
    }

    std::optional<std::size_t> CountOf(nlohmann::json const& body, std::string_view name)
    {
        auto const found = body.find(name);
        if (found == body.end() || !found->is_number_unsigned())
            return std::nullopt;
        return found->get<std::size_t>();
    }

    AdminResponse Invalid(std::string message, std::vector<std::pair<std::string, std::string>> fields)
    {
        return AdminResponse::Invalid(std::move(message), std::move(fields));
    }

    AdminResponse Refused(std::string_view code, std::string const& message)
    {
        return AdminResponse::Problem(409, std::string(code), message);
    }

    AdminResponse StoreDown(std::string const& error)
    {
        return AdminResponse::Problem(503, "store_unavailable", error);
    }
}

std::optional<std::pair<std::string, uint16>> Panel::AppListenerOf(std::string_view app)
{
    if (!_appCall)
        return std::nullopt;
    MaintenanceAppCallResult const apps = _appCall(app, "GET", "/api/apps", "");
    if (!apps.Ok || apps.Status != 200)
        return std::nullopt;
    nlohmann::json const body = nlohmann::json::parse(apps.Body, nullptr, false);
    if (!body.is_array() || body.empty() || !body[0].is_object())
        return std::nullopt;
    std::string const liveAddress = body[0].value("address", std::string());
    uint16 const livePort = static_cast<uint16>(body[0].value("port", 0));
    return std::make_pair(liveAddress, livePort);
}

bool Panel::AppListensOn(std::string_view app, std::string_view address, uint16 port)
{
    std::optional<std::pair<std::string, uint16>> const listener = AppListenerOf(app);
    return listener && listener->first == address && listener->second == port;
}

void Panel::RegisterNetwork()
{
    AdminRouter& routes = _listener.Routes();
    routes.AddGuardedPrefix("GET", "/api/panel/network/node/", "network.read", [this](AdminRequest const& request) { return NetworkNode(request); });
    routes.AddGuardedPrefix("GET", "/api/panel/network/app/", "network.read", [this](AdminRequest const& request) { return NetworkApp(request); });
    routes.AddGuarded("POST", "/api/panel/network/allocations", "network.edit", [this](AdminRequest const& request) { return NetworkAllocationAdd(request); });
    routes.AddGuarded("POST", "/api/panel/network/allocations/range", "network.edit", [this](AdminRequest const& request) { return NetworkAllocationAddRange(request); });
    routes.AddGuarded("POST", "/api/panel/network/allocations/bulk-delete", "network.edit", [this](AdminRequest const& request) { return NetworkAllocationsBulkDelete(request); });
    routes.AddGuardedPrefix("POST", "/api/panel/network/allocations/", "network.edit", [this](AdminRequest const& request) { return NetworkAllocationPost(request); });
    routes.AddGuardedPrefix("DELETE", "/api/panel/network/allocations/", "network.edit", [this](AdminRequest const& request) { return NetworkAllocationDelete(request); });
    routes.AddGuarded("PUT", "/api/panel/network/pool", "network.edit", [this](AdminRequest const& request) { return NetworkPoolSet(request); });
    routes.AddGuarded("POST", "/api/panel/network/pool/take", "network.edit", [this](AdminRequest const& request) { return NetworkPoolTake(request); });
}

AdminResponse Panel::NetworkNode(AdminRequest const& request)
{
    constexpr std::string_view RoutePrefix = "/api/panel/network/node/";
    std::string const node = std::string(std::string_view(request.Path).substr(RoutePrefix.size()));
    if (!PortAllocations::IsKnownNode(node))
        return AdminResponse::Problem(404, "unknown_node", fmt::format("the panel knows no node '{}'", node));
    nlohmann::json answer;
    answer["schema"] = 1;
    answer["node"] = node;
    std::string error;
    {
        std::lock_guard const lock(_storeMutex);
        if (!_store.IsOpen())
            return StoreDown("the panel store is not open");
        std::vector<PortAllocation> allocations;
        PortPool pool;
        if (!PortAllocations::List(_store, node, allocations, error) || !PortAllocations::PoolGet(_store, node, pool, error))
            return StoreDown(error);
        nlohmann::json listed = nlohmann::json::array();
        for (PortAllocation const& allocation : allocations)
            listed.push_back(AllocationJson(allocation));
        answer["allocations"] = std::move(listed);
        answer["pool"] = pool.StartPort == 0 ? nlohmann::json(nullptr) : PoolJson(pool);
        nlohmann::json apps = nlohmann::json::array();
        std::vector<std::string> const names = _launchAppSource ? _launchAppSource() : std::vector<std::string>();
        for (std::string const& name : names)
        {
            nlohmann::json held = nlohmann::json::array();
            for (PortAllocation const& allocation : allocations)
                if (allocation.App == name)
                    held.push_back(AllocationJson(allocation));
            apps.push_back({ { "app", name }, { "allocations", std::move(held) } });
        }
        answer["apps"] = std::move(apps);
    }
    return AdminResponse::Json(200, answer.dump());
}

AdminResponse Panel::NetworkApp(AdminRequest const& request)
{
    constexpr std::string_view RoutePrefix = "/api/panel/network/app/";
    std::string const app = std::string(std::string_view(request.Path).substr(RoutePrefix.size()));
    if (!PortAllocations::IsKnownApp(app))
        return AdminResponse::Problem(404, "unknown_app", fmt::format("the panel knows no app '{}'", app));
    nlohmann::json answer;
    answer["schema"] = 1;
    answer["app"] = app;
    std::string error;
    {
        std::lock_guard const lock(_storeMutex);
        if (!_store.IsOpen())
            return StoreDown("the panel store is not open");
        std::vector<PortAllocation> allocations;
        if (!PortAllocations::List(_store, PortAllocations::LocalNode, allocations, error))
            return StoreDown(error);
        nlohmann::json held = nlohmann::json::array();
        for (PortAllocation const& allocation : allocations)
            if (allocation.App == app)
                held.push_back(AllocationJson(allocation));
        answer["allocations"] = std::move(held);
    }
    if (_appCall)
    {
        if (std::optional<std::pair<std::string, uint16>> const listener = AppListenerOf(app))
        {
            nlohmann::json live;
            live["bind_address"] = listener->first;
            live["port"] = listener->second;
            answer["live_listener"] = std::move(live);
        }
    }
    return AdminResponse::Json(200, answer.dump());
}

bool Panel::NetworkDraftOf(nlohmann::json const& body, PortAllocationDraft& draft, std::string& refusal)
{
    draft = PortAllocationDraft();
    std::string const node = TextOf(body, "node");
    draft.Node = node.empty() ? std::string(PortAllocations::LocalNode) : node;
    draft.BindAddress = TextOf(body, "bind_address");
    std::optional<uint16> const port = PortOf(body, "port");
    if (draft.BindAddress.empty())
    {
        refusal = "a bind address is needed";
        return false;
    }
    if (!port)
    {
        refusal = "a port from 1 to 65535 is needed";
        return false;
    }
    draft.Port = *port;
    std::string const protocol = TextOf(body, "protocol");
    draft.Protocol = protocol.empty() ? "tcp" : protocol;
    draft.PublicAlias = TextOf(body, "public_alias");
    draft.App = TextOf(body, "app");
    draft.Role = TextOf(body, "role");
    draft.Notes = TextOf(body, "notes");
    return true;
}

AdminResponse Panel::NetworkAudited(AdminRequest const& request, std::string_view name, std::function<AdminResponse(std::string& refusal, std::string& error)> operation)
{
    std::optional<PanelUser> const user = UserOf(request);
    AuditEvent event;
    event.Name = std::string(name);
    event.Actor = AuditActor::User;
    if (user)
    {
        event.ActorId = std::to_string(user->Id);
        event.ActorName = user->Username;
        event.On("panel_user", std::to_string(user->Id), user->Username);
    }
    event.Address = request.RemoteAddress;
    event.UserAgent = request.UserAgent;
    event.On("node", std::string(PortAllocations::LocalNode), std::string(PortAllocations::LocalNode));
    std::string failure;
    AdminResponse answer = AdminResponse::Problem(503, "store_unavailable", "the panel store did not answer");
    bool const recorded = Record(event, [&](AuditEvent& entry, std::string& error) -> bool
    {
        std::string refusal;
        answer = operation(refusal, error);
        if (!refusal.empty())
        {
            entry.Result = AuditResult::Refused;
            entry.Reason = refusal;
        }
        return error.empty();
    }, failure);
    if (!recorded)
        return StoreDown(failure);
    return answer;
}

AdminResponse Panel::NetworkAllocationAdd(AdminRequest const& request)
{
    nlohmann::json const body = ParseBody(request);
    if (!body.is_object())
        return Invalid("Adding an allocation takes a JSON object", { { "bind_address", "Give the address to bind" }, { "port", "Give the port to hold" } });
    PortAllocationDraft draft;
    std::string draftRefusal;
    if (!NetworkDraftOf(body, draft, draftRefusal))
        return Invalid(draftRefusal, { { "bind_address", "Give the address to bind" }, { "port", "Give the port to hold" } });
    return NetworkAudited(request, "network:allocation.created", [&](std::string& refusal, std::string& error) -> AdminResponse
    {
        std::vector<std::string> warnings;
        int64 id = 0;
        if (!PortAllocations::Add(_store, draft, warnings, id, refusal, error))
            return StoreDown(error);
        if (!refusal.empty())
            return Refused("allocation_refused", refusal);
        nlohmann::json answer = AllocationJson(PortAllocation{ id, draft.Node, draft.BindAddress, draft.Port, draft.Protocol, draft.PublicAlias, draft.App, draft.Role, draft.Notes });
        answer["warnings"] = warnings;
        return AdminResponse::Json(201, nlohmann::json({ { "schema", 1 }, { "allocation", std::move(answer) } }).dump());
    });
}

AdminResponse Panel::NetworkAllocationAddRange(AdminRequest const& request)
{
    nlohmann::json const body = ParseBody(request);
    if (!body.is_object())
        return Invalid("Adding a range takes a JSON object", { { "bind_address", "Give the address to bind" }, { "port", "Give the first port" }, { "end_port", "Give the last port" } });
    PortAllocationDraft draft;
    std::string draftRefusal;
    if (!NetworkDraftOf(body, draft, draftRefusal))
        return Invalid(draftRefusal, { { "bind_address", "Give the address to bind" }, { "port", "Give the first port" } });
    std::optional<uint16> const endPort = PortOf(body, "end_port");
    if (!endPort)
        return Invalid("a last port from 1 to 65535 is needed", { { "end_port", "Give the last port of the range" } });
    return NetworkAudited(request, "network:allocation.range.created", [&](std::string& refusal, std::string& error) -> AdminResponse
    {
        std::vector<std::string> warnings;
        std::size_t added = 0;
        if (!PortAllocations::AddRange(_store, draft, *endPort, warnings, added, refusal, error))
            return StoreDown(error);
        if (!refusal.empty())
            return Refused("allocation_refused", refusal);
        nlohmann::json answer;
        answer["schema"] = 1;
        answer["added"] = added;
        answer["warnings"] = warnings;
        return AdminResponse::Json(201, answer.dump());
    });
}

AdminResponse Panel::NetworkAllocationPost(AdminRequest const& request)
{
    constexpr std::string_view RoutePrefix = "/api/panel/network/allocations/";
    std::string_view rest = std::string_view(request.Path).substr(RoutePrefix.size());
    std::size_t const slash = rest.find('/');
    std::string const idText = std::string(slash == std::string_view::npos ? rest : rest.substr(0, slash));
    std::string_view const action = slash == std::string_view::npos ? std::string_view() : rest.substr(slash + 1);
    int64 id = 0;
    try
    {
        id = std::stoll(idText);
    }
    catch (std::exception const&)
    {
        return AdminResponse::Problem(404, "not_found", "that allocation does not exist");
    }
    if (action == "assign")
        return NetworkAllocationAssign(request, id);
    if (action == "unassign")
        return NetworkAllocationUnassign(request, id);
    return AdminResponse::Problem(404, "not_found", "that allocation action does not exist");
}

AdminResponse Panel::NetworkAllocationDelete(AdminRequest const& request)
{
    constexpr std::string_view RoutePrefix = "/api/panel/network/allocations/";
    std::string const idText = std::string(std::string_view(request.Path).substr(RoutePrefix.size()));
    int64 id = 0;
    try
    {
        id = std::stoll(idText);
    }
    catch (std::exception const&)
    {
        return AdminResponse::Problem(404, "not_found", "that allocation does not exist");
    }
    return NetworkAudited(request, "network:allocation.deleted", [&](std::string& refusal, std::string& error) -> AdminResponse
    {
        if (!PortAllocations::Remove(_store, id, refusal, error))
            return StoreDown(error);
        if (!refusal.empty())
            return Refused("allocation_in_use", refusal);
        return AdminResponse::Json(200, nlohmann::json({ { "schema", 1 }, { "deleted", id } }).dump());
    });
}

AdminResponse Panel::NetworkAllocationsBulkDelete(AdminRequest const& request)
{
    nlohmann::json const body = ParseBody(request);
    if (!body.is_object() || !body.contains("ids") || !body["ids"].is_array())
        return Invalid("Bulk delete takes a JSON object with an ids array", { { "ids", "List the allocation ids to delete" } });
    std::vector<int64> ids;
    for (nlohmann::json const& entry : body["ids"])
    {
        if (!entry.is_number_integer())
            return Invalid("every id in ids must be an integer", { { "ids", "List the allocation ids to delete" } });
        ids.push_back(entry.get<int64>());
    }
    if (ids.empty())
        return Invalid("ids names no allocation", { { "ids", "List the allocation ids to delete" } });
    return NetworkAudited(request, "network:allocation.bulk_deleted", [&](std::string& refusal, std::string& error) -> AdminResponse
    {
        if (!PortAllocations::RemoveMany(_store, ids, refusal, error))
            return StoreDown(error);
        if (!refusal.empty())
            return Refused("allocation_in_use", refusal);
        return AdminResponse::Json(200, nlohmann::json({ { "schema", 1 }, { "deleted", ids.size() } }).dump());
    });
}

AdminResponse Panel::NetworkAllocationAssign(AdminRequest const& request, int64 id)
{
    nlohmann::json const body = ParseBody(request);
    if (!body.is_object())
        return Invalid("Assigning an allocation takes a JSON object", { { "app", "Name the app that takes the port" }, { "role", "Name the role the port serves" } });
    std::string const app = TextOf(body, "app");
    std::string const role = TextOf(body, "role");
    std::string reason = TextOf(body, "reason");
    if (app.empty() || role.empty())
        return Invalid("an app and a role are needed", { { "app", "Name the app that takes the port" }, { "role", "Name the role the port serves" } });
    if (!PortAllocations::IsKnownApp(app))
        return Invalid(fmt::format("the panel knows no app '{}'", app), { { "app", "Name one of the supervised apps" } });
    if (!PortAllocations::IsKnownRole(role))
        return Invalid(fmt::format("the panel knows no role '{}'", role), { { "role", "Name a listener role" } });
    std::string const portKey = PortAllocations::ListenSetting(app, role);
    if (portKey.empty())
        return Refused("role_not_assignable", fmt::format("the '{}' role has no listen setting the settings API can write", role));
    if (!_appCall)
        return AdminResponse::Problem(503, "apps_unavailable", "the supervisor is not answering for its apps");

    PortAllocation allocation;
    {
        std::lock_guard const lock(_storeMutex);
        std::string error;
        if (!_store.IsOpen())
            return StoreDown("the panel store is not open");
        if (!PortAllocations::Find(_store, id, allocation, error))
            return StoreDown(error);
    }
    if (allocation.Id == 0)
        return AdminResponse::Problem(404, "not_found", fmt::format("no allocation has id {}", id));
    if (allocation.Protocol == "udp")
        return Refused("protocol_mismatch",
            fmt::format("allocation {} holds a UDP port, but the '{}' role listens on TCP", id, role));
    if (!allocation.App.empty() && (allocation.App != app || allocation.Role != role))
        return Refused("already_assigned", fmt::format("allocation {} is already assigned to {} as '{}'; unassign it first", id, allocation.App, allocation.Role));

    bool const appHoldsIt = AppListensOn(app, allocation.BindAddress, allocation.Port);
    if (!appHoldsIt)
    {
        std::optional<PortProbe::Protocol> const protocol = PortProbe::Parse(allocation.Protocol);
        std::string probeError;
        if (!protocol || !PortProbe::IsFree(*protocol, allocation.BindAddress, allocation.Port, probeError))
            return Refused("port_in_use",
                fmt::format("port {} is already in use on {}{}", allocation.Port, allocation.BindAddress, probeError.empty() ? "" : fmt::format(": {}", probeError)));
    }

    nlohmann::json batch;
    batch["entries"] = nlohmann::json::array(
        { nlohmann::json({ { "key", portKey }, { "value", allocation.Port } }), nlohmann::json({ { "key", "BindIP" }, { "value", allocation.BindAddress } }) });
    if (reason.empty())
        reason = fmt::format("Port allocation {} assigned to {} as '{}'", id, app, role);
    batch["reason"] = reason;
    MaintenanceAppCallResult const answered = _appCall(app, "POST", "/api/settings/batch", batch.dump());
    if (!answered.Ok && answered.Error.empty())
        return AdminResponse::Problem(503, "app_unreachable", fmt::format("{} is not running, so its listen setting cannot be written", app));
    if (!answered.Ok)
        return AdminResponse::Problem(502, "app_no_answer", fmt::format("{} did not answer: {}", app, answered.Error));
    nlohmann::json const appBody = nlohmann::json::parse(answered.Body, nullptr, false);
    if (answered.Status < 200 || answered.Status >= 300)
    {
        std::string const message = appBody.is_object() && appBody.contains("message") && appBody["message"].is_string()
            ? appBody["message"].get<std::string>()
            : fmt::format("{} refused the listen setting", app);
        std::string const code = answered.Status == 409 ? "allocation_refused" : "app_refused";
        return NetworkAudited(request, "network:allocation.assign_refused", [&](std::string&, std::string& error) -> AdminResponse
        {
            (void)error;
            return AdminResponse::Problem(answered.Status == 409 ? 409 : 502, code, message);
        });
    }

    return NetworkAudited(request, "network:allocation.assigned", [&](std::string& refusal, std::string& error) -> AdminResponse
    {
        if (!PortAllocations::Assign(_store, id, app, role, refusal, error))
            return StoreDown(error);
        if (!refusal.empty())
            return Refused("allocation_refused", refusal);
        nlohmann::json answer;
        answer["schema"] = 1;
        answer["assigned"] = id;
        answer["app"] = app;
        answer["role"] = role;
        answer["port"] = allocation.Port;
        answer["bind_address"] = allocation.BindAddress;
        return AdminResponse::Json(200, answer.dump());
    });
}

AdminResponse Panel::NetworkAllocationUnassign(AdminRequest const& request, int64 id)
{
    return NetworkAudited(request, "network:allocation.unassigned", [&](std::string& refusal, std::string& error) -> AdminResponse
    {
        if (!PortAllocations::Unassign(_store, id, refusal, error))
            return StoreDown(error);
        if (!refusal.empty())
            return Refused("allocation_refused", refusal);
        return AdminResponse::Json(200, nlohmann::json({ { "schema", 1 }, { "unassigned", id } }).dump());
    });
}

AdminResponse Panel::NetworkPoolSet(AdminRequest const& request)
{
    nlohmann::json const body = ParseBody(request);
    if (!body.is_object())
        return Invalid("Setting the pool takes a JSON object", { { "start_port", "Give the first port" }, { "end_port", "Give the last port" } });
    std::string const nodeText = TextOf(body, "node");
    std::string const node = nodeText.empty() ? std::string(PortAllocations::LocalNode) : nodeText;
    std::optional<uint16> const startPort = PortOf(body, "start_port");
    std::optional<uint16> const endPort = PortOf(body, "end_port");
    if (!startPort || !endPort)
        return Invalid("a first and a last port from 1 to 65535 are needed", { { "start_port", "Give the first port" }, { "end_port", "Give the last port" } });
    return NetworkAudited(request, "network:pool.changed", [&](std::string& refusal, std::string& error) -> AdminResponse
    {
        if (!PortAllocations::PoolSet(_store, node, *startPort, *endPort, refusal, error))
            return StoreDown(error);
        if (!refusal.empty())
            return Refused("pool_refused", refusal);
        return AdminResponse::Json(200, nlohmann::json({ { "schema", 1 }, { "pool", PoolJson(PortPool{ node, *startPort, *endPort }) } }).dump());
    });
}

AdminResponse Panel::NetworkPoolTake(AdminRequest const& request)
{
    nlohmann::json const body = ParseBody(request);
    std::string node = std::string(PortAllocations::LocalNode);
    std::size_t count = 1;
    if (body.is_object())
    {
        std::string const nodeText = TextOf(body, "node");
        if (!nodeText.empty())
            node = nodeText;
        if (std::optional<std::size_t> const asked = CountOf(body, "count"))
            count = *asked;
    }
    if (!PortAllocations::IsKnownNode(node))
        return AdminResponse::Problem(404, "unknown_node", fmt::format("the panel knows no node '{}'", node));
    return NetworkAudited(request, "network:pool.taken", [&](std::string& refusal, std::string& error) -> AdminResponse
    {
        std::vector<PortAllocation> taken;
        if (!PortAllocations::PoolTake(_store, node, count, taken, refusal, error))
            return StoreDown(error);
        if (!refusal.empty())
            return Refused("pool_empty", refusal);
        nlohmann::json listed = nlohmann::json::array();
        for (PortAllocation const& allocation : taken)
            listed.push_back(AllocationJson(allocation));
        return AdminResponse::Json(201, nlohmann::json({ { "schema", 1 }, { "taken", std::move(listed) } }).dump());
    });
}
