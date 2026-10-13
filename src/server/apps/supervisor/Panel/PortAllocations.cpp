/*
 * Project Ambrose by Imjustchico
 * Reads and writes the port_allocation and port_pool tables through the panel's store: every add proves its ports free by binding them before anything is stored, a range is refused past 1000 ports, an allocation carrying an app and role is never deleted, and taking from the pool skips the ports the node already holds and proves the rest free.
 */

#include "PortAllocations.h"
#include "IpAddress.h"
#include "PanelStore.h"
#include "PortProbe.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>

namespace
{
    PortAllocation ReadRow(PanelStore::Statement& rows)
    {
        PortAllocation allocation;
        allocation.Id = rows.Int64(0);
        allocation.Node = rows.Text(1);
        allocation.BindAddress = rows.Text(2);
        allocation.Port = static_cast<uint16>(rows.Int64(3));
        allocation.Protocol = rows.Text(4);
        allocation.PublicAlias = rows.Text(5);
        allocation.App = rows.Text(6);
        allocation.Role = rows.Text(7);
        allocation.Notes = rows.Text(8);
        return allocation;
    }

    bool Insert(PanelStore& store, PortAllocationDraft const& draft, int64& id, std::string& error)
    {
        std::optional<PanelStore::Statement> insert = store.Prepare(
            "INSERT INTO port_allocation (node, bind_address, port, protocol, public_alias, app, role, notes, created_epoch_ms)"
            " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
            error);
        if (!insert)
            return false;
        insert->Bind(1, draft.Node);
        insert->Bind(2, draft.BindAddress);
        insert->Bind(3, int64{ draft.Port });
        insert->Bind(4, draft.Protocol);
        insert->Bind(5, draft.PublicAlias);
        insert->Bind(6, draft.App);
        insert->Bind(7, draft.Role);
        insert->Bind(8, draft.Notes);
        insert->Bind(9, PanelStore::NowEpochMs());
        if (!insert->Run(error))
            return false;
        id = store.LastInsertId();
        return true;
    }

    bool OpenSavepoint(PanelStore& store, std::string& error)
    {
        return store.Execute("SAVEPOINT port_allocations", error);
    }

    void CanonicalizeAddress(PortAllocationDraft& draft)
    {
        if (std::optional<asio::ip::address> const parsed = Ambrose::Asio::MakeAddress(draft.BindAddress))
            draft.BindAddress = parsed->to_string();
    }

    void CloseSavepoint(PanelStore& store, bool keep)
    {
        std::string ignored;
        if (!keep)
            store.Execute("ROLLBACK TO SAVEPOINT port_allocations", ignored);
        store.Execute("RELEASE SAVEPOINT port_allocations", ignored);
    }
}

bool PortAllocations::IsKnownNode(std::string_view node) noexcept
{
    return node == LocalNode;
}

bool PortAllocations::IsKnownRole(std::string_view role) noexcept
{
    for (std::string const& known : Roles())
        if (known == role)
            return true;
    return false;
}

bool PortAllocations::IsKnownApp(std::string_view app) noexcept
{
    for (std::string const& known : Apps())
        if (known == app)
            return true;
    return false;
}

std::string PortAllocations::ListenSetting(std::string_view app, std::string_view role)
{
    if (app == "loginserver" && role == "login listener")
        return "LoginServerPort";
    if (app == "gameserver" && role == "game listener")
        return "WorldServerPort";
    if (app == "patchserver" && role == "patch HTTP")
        return "PatchServerPort";
    return {};
}

std::vector<std::string> PortAllocations::Roles() noexcept
{
    return { "login listener", "game listener", "patch HTTP", "admin API", "panel", "metrics" };
}

std::vector<std::string> PortAllocations::Apps() noexcept
{
    return { "loginserver", "gameserver", "patchserver", "supervisor" };
}

bool PortAllocations::List(PanelStore& store, std::string_view node, std::vector<PortAllocation>& allocations, std::string& error)
{
    allocations.clear();
    std::optional<PanelStore::Statement> rows = store.Prepare(
        "SELECT id, node, bind_address, port, protocol, public_alias, app, role, notes FROM port_allocation WHERE node = ? ORDER BY port, bind_address", error);
    if (!rows)
        return false;
    rows->Bind(1, node);
    while (rows->Step(error))
        allocations.push_back(ReadRow(*rows));
    return error.empty();
}

bool PortAllocations::Find(PanelStore& store, int64 id, PortAllocation& allocation, std::string& error)
{
    std::optional<PanelStore::Statement> rows = store.Prepare(
        "SELECT id, node, bind_address, port, protocol, public_alias, app, role, notes FROM port_allocation WHERE id = ?", error);
    if (!rows)
        return false;
    rows->Bind(1, id);
    if (!rows->Step(error))
        return error.empty();
    allocation = ReadRow(*rows);
    return true;
}

bool PortAllocations::CheckDraft(PortAllocationDraft const& draft, std::string& refusal)
{
    if (!IsKnownNode(draft.Node))
    {
        refusal = fmt::format("unknown node '{}'; the panel knows '{}'", draft.Node, LocalNode);
        return false;
    }
    if (!Ambrose::Asio::MakeAddress(draft.BindAddress))
    {
        refusal = fmt::format("'{}' is not an IP address", draft.BindAddress);
        return false;
    }
    if (draft.Port == 0)
    {
        refusal = "port 0 asks the operating system to choose, so it cannot be allocated";
        return false;
    }
    if (!PortProbe::Parse(draft.Protocol))
    {
        refusal = fmt::format("unknown protocol '{}'; use tcp, udp or http", draft.Protocol);
        return false;
    }
    if (!draft.App.empty() && !IsKnownApp(draft.App))
    {
        refusal = fmt::format("unknown app '{}'", draft.App);
        return false;
    }
    if (!draft.Role.empty() && !IsKnownRole(draft.Role))
    {
        refusal = fmt::format("unknown role '{}'", draft.Role);
        return false;
    }
    if (draft.App.empty() != draft.Role.empty())
    {
        refusal = "an allocation names both the app and the role using it, or neither";
        return false;
    }
    return true;
}

bool PortAllocations::ProbeFree(std::string_view protocol, std::string_view address, uint16 port, std::string& refusal)
{
    std::optional<PortProbe::Protocol> const parsed = PortProbe::Parse(protocol);
    std::string error;
    if (parsed && PortProbe::IsFree(*parsed, address, port, error))
        return true;
    refusal = fmt::format("port {} is already in use on {}{}", port, address, error.empty() ? "" : fmt::format(": {}", error));
    return false;
}

void PortAllocations::PrivilegeWarning(uint16 port, std::vector<std::string>& warnings)
{
    if (port <= PrivilegedPortMax)
        warnings.push_back(fmt::format("port {} is below 1025, so binding it needs privileges the app may not have", port));
}

bool PortAllocations::Exists(PanelStore& store, std::string_view node, std::string_view address, uint16 port, std::string_view protocol, std::string& error)
{
    std::optional<PanelStore::Statement> rows = store.Prepare(
        "SELECT COUNT(*) FROM port_allocation WHERE node = ? AND bind_address = ? AND port = ? AND protocol = ?", error);
    if (!rows)
        return false;
    rows->Bind(1, node);
    rows->Bind(2, address);
    rows->Bind(3, int64{ port });
    rows->Bind(4, protocol);
    if (!rows->Step(error))
        return false;
    return rows->Int64(0) > 0;
}

bool PortAllocations::Add(PanelStore& store, PortAllocationDraft const& draft, std::vector<std::string>& warnings, int64& id, std::string& refusal, std::string& error)
{
    warnings.clear();
    id = 0;
    PortAllocationDraft checked = draft;
    if (!CheckDraft(checked, refusal))
        return true;
    CanonicalizeAddress(checked);
    std::string storeError;
    if (Exists(store, checked.Node, checked.BindAddress, checked.Port, checked.Protocol, storeError))
    {
        if (!storeError.empty())
        {
            error = storeError;
            return false;
        }
        refusal = fmt::format("port {} on {} is already allocated on node '{}' for {}", checked.Port, checked.BindAddress, checked.Node, checked.Protocol);
        return true;
    }
    if (!ProbeFree(checked.Protocol, checked.BindAddress, checked.Port, refusal))
        return true;
    PrivilegeWarning(checked.Port, warnings);
    return Insert(store, checked, id, error);
}

bool PortAllocations::AddRange(PanelStore& store, PortAllocationDraft const& draft, uint16 endPort, std::vector<std::string>& warnings, std::size_t& added,
    std::string& refusal, std::string& error)
{
    warnings.clear();
    added = 0;
    if (endPort < draft.Port)
    {
        refusal = fmt::format("the range ends at {} but starts at {}", endPort, draft.Port);
        return true;
    }
    uint32 const count = uint32{ endPort } - uint32{ draft.Port } + 1;
    if (count > MaxRangePorts)
    {
        refusal = fmt::format("a range holds at most {} ports, but {}-{} asks for {}", MaxRangePorts, draft.Port, endPort, count);
        return true;
    }
    PortAllocationDraft checked = draft;
    if (!CheckDraft(checked, refusal))
        return true;
    CanonicalizeAddress(checked);
    std::string storeError;
    for (uint32 port = checked.Port; port <= endPort; ++port)
    {
        if (Exists(store, checked.Node, checked.BindAddress, static_cast<uint16>(port), checked.Protocol, storeError))
        {
            if (!storeError.empty())
            {
                error = storeError;
                return false;
            }
            refusal = fmt::format("port {} on {} is already allocated on node '{}' for {}, so the range adds nothing", port, checked.BindAddress, checked.Node, checked.Protocol);
            return true;
        }
        if (!ProbeFree(checked.Protocol, checked.BindAddress, static_cast<uint16>(port), refusal))
            return true;
    }
    if (!OpenSavepoint(store, error))
        return false;
    bool ok = true;
    for (uint32 port = checked.Port; port <= endPort && ok; ++port)
    {
        PortAllocationDraft single = checked;
        single.Port = static_cast<uint16>(port);
        int64 id = 0;
        ok = Insert(store, single, id, error);
        if (ok)
            ++added;
    }
    CloseSavepoint(store, ok);
    if (!ok)
    {
        added = 0;
        return false;
    }
    PrivilegeWarning(draft.Port, warnings);
    PrivilegeWarning(endPort, warnings);
    return true;
}

bool PortAllocations::Remove(PanelStore& store, int64 id, std::string& refusal, std::string& error)
{
    PortAllocation allocation;
    if (!Find(store, id, allocation, error))
        return false;
    if (allocation.Id == 0)
    {
        refusal = fmt::format("no allocation has id {}", id);
        return true;
    }
    if (!allocation.App.empty())
    {
        refusal = fmt::format("allocation {} (port {} on {}) is assigned to {} as '{}', so it cannot be deleted", id, allocation.Port, allocation.BindAddress, allocation.App, allocation.Role);
        return true;
    }
    std::optional<PanelStore::Statement> remove = store.Prepare("DELETE FROM port_allocation WHERE id = ?", error);
    if (!remove)
        return false;
    remove->Bind(1, id);
    return remove->Run(error);
}

bool PortAllocations::RemoveMany(PanelStore& store, std::vector<int64> const& ids, std::string& refusal, std::string& error)
{
    std::optional<PanelStore::Statement> find = store.Prepare(
        "SELECT id, node, bind_address, port, protocol, public_alias, app, role, notes FROM port_allocation WHERE id = ?", error);
    std::optional<PanelStore::Statement> remove = store.Prepare("DELETE FROM port_allocation WHERE id = ?", error);
    if (!find || !remove)
        return false;
    if (!OpenSavepoint(store, error))
        return false;
    bool changed = true;
    for (int64 id : ids)
    {
        find->Reset();
        find->Bind(1, id);
        if (!find->Step(error))
        {
            if (error.empty())
                refusal = fmt::format("no allocation has id {}", id);
            changed = false;
            break;
        }
        PortAllocation const allocation = ReadRow(*find);
        if (!allocation.App.empty())
        {
            refusal = fmt::format("allocation {} (port {} on {}) is assigned to {} as '{}', so the selection deletes nothing",
                id, allocation.Port, allocation.BindAddress, allocation.App, allocation.Role);
            changed = false;
            break;
        }
        remove->Reset();
        remove->Bind(1, id);
        if (!remove->Run(error))
        {
            changed = false;
            break;
        }
    }
    CloseSavepoint(store, changed && error.empty());
    return error.empty();
}

bool PortAllocations::Assign(PanelStore& store, int64 id, std::string_view app, std::string_view role, std::string& refusal, std::string& error)
{
    PortAllocation allocation;
    if (!Find(store, id, allocation, error))
        return false;
    if (allocation.Id == 0)
    {
        refusal = fmt::format("no allocation has id {}", id);
        return true;
    }
    if (!IsKnownApp(app))
    {
        refusal = fmt::format("unknown app '{}'", app);
        return true;
    }
    if (!IsKnownRole(role))
    {
        refusal = fmt::format("unknown role '{}'", role);
        return true;
    }
    if (!allocation.App.empty() && (allocation.App != app || allocation.Role != role))
    {
        refusal = fmt::format("allocation {} is already assigned to {} as '{}'; unassign it first", id, allocation.App, allocation.Role);
        return true;
    }
    if (ListenSetting(app, role).empty())
    {
        refusal = fmt::format("the '{}' role has no listen setting the settings API can write", role);
        return true;
    }
    if (!OpenSavepoint(store, error))
        return false;
    std::optional<PanelStore::Statement> clear = store.Prepare("UPDATE port_allocation SET app = '', role = '' WHERE app = ? AND role = ? AND id <> ?", error);
    std::optional<PanelStore::Statement> set = store.Prepare("UPDATE port_allocation SET app = ?, role = ? WHERE id = ?", error);
    bool ok = clear && set;
    if (ok)
    {
        clear->Bind(1, app);
        clear->Bind(2, role);
        clear->Bind(3, id);
        ok = clear->Run(error);
    }
    if (ok)
    {
        set->Bind(1, app);
        set->Bind(2, role);
        set->Bind(3, id);
        ok = set->Run(error);
    }
    CloseSavepoint(store, ok && error.empty());
    return ok && error.empty();
}

bool PortAllocations::Unassign(PanelStore& store, int64 id, std::string& refusal, std::string& error)
{
    PortAllocation allocation;
    if (!Find(store, id, allocation, error))
        return false;
    if (allocation.Id == 0)
    {
        refusal = fmt::format("no allocation has id {}", id);
        return true;
    }
    if (allocation.App.empty())
    {
        refusal = fmt::format("allocation {} is not assigned to any app", id);
        return true;
    }
    std::optional<PanelStore::Statement> clear = store.Prepare("UPDATE port_allocation SET app = '', role = '' WHERE id = ?", error);
    if (!clear)
        return false;
    clear->Bind(1, id);
    return clear->Run(error);
}

bool PortAllocations::PoolGet(PanelStore& store, std::string_view node, PortPool& pool, std::string& error)
{
    pool = PortPool{};
    std::optional<PanelStore::Statement> rows = store.Prepare("SELECT node, start_port, end_port FROM port_pool WHERE node = ?", error);
    if (!rows)
        return false;
    rows->Bind(1, node);
    if (!rows->Step(error))
        return error.empty();
    pool.Node = rows->Text(0);
    pool.StartPort = static_cast<uint16>(rows->Int64(1));
    pool.EndPort = static_cast<uint16>(rows->Int64(2));
    return true;
}

bool PortAllocations::PoolSet(PanelStore& store, std::string_view node, uint16 startPort, uint16 endPort, std::string& refusal, std::string& error)
{
    if (!IsKnownNode(node))
    {
        refusal = fmt::format("unknown node '{}'; the panel knows '{}'", node, LocalNode);
        return true;
    }
    if (startPort == 0 || endPort == 0 || startPort > endPort)
    {
        refusal = fmt::format("the pool range {}-{} is not a range of ports", startPort, endPort);
        return true;
    }
    std::optional<PanelStore::Statement> set = store.Prepare(
        "INSERT INTO port_pool (node, start_port, end_port) VALUES (?, ?, ?)"
        " ON CONFLICT (node) DO UPDATE SET start_port = excluded.start_port, end_port = excluded.end_port",
        error);
    if (!set)
        return false;
    set->Bind(1, node);
    set->Bind(2, int64{ startPort });
    set->Bind(3, int64{ endPort });
    return set->Run(error);
}

bool PortAllocations::PoolTake(PanelStore& store, std::string_view node, std::size_t count, std::vector<PortAllocation>& taken, std::string& refusal, std::string& error)
{
    taken.clear();
    if (count == 0)
    {
        refusal = "taking no ports changes nothing";
        return true;
    }
    PortPool pool;
    if (!PoolGet(store, node, pool, error))
        return false;
    if (pool.StartPort == 0)
    {
        refusal = fmt::format("node '{}' has no port pool; set one first", node);
        return true;
    }
    std::optional<PanelStore::Statement> held = store.Prepare(
        "SELECT port FROM port_allocation WHERE node = ? AND bind_address = ? AND protocol IN ('tcp', 'http')", error);
    if (!held)
        return false;
    held->Bind(1, node);
    held->Bind(2, "0.0.0.0");
    std::vector<uint16> taken_ports;
    while (held->Step(error))
        taken_ports.push_back(static_cast<uint16>(held->Int64(0)));
    if (!error.empty())
        return false;
    std::vector<uint16> free;
    for (uint32 port = pool.StartPort; port <= pool.EndPort && free.size() < count; ++port)
    {
        uint16 const candidate = static_cast<uint16>(port);
        if (std::find(taken_ports.begin(), taken_ports.end(), candidate) != taken_ports.end())
            continue;
        std::string probeRefusal;
        if (!ProbeFree("tcp", "0.0.0.0", candidate, probeRefusal))
            continue;
        free.push_back(candidate);
    }
    if (free.size() < count)
    {
        refusal = fmt::format("the pool on node '{}' has only {} free port{} for {} asked", node, free.size(), free.size() == 1 ? "" : "s", count);
        return true;
    }
    if (!OpenSavepoint(store, error))
        return false;
    bool ok = true;
    for (uint16 port : free)
    {
        PortAllocationDraft draft;
        draft.Node = std::string(node);
        draft.BindAddress = "0.0.0.0";
        draft.Port = port;
        draft.Protocol = "tcp";
        draft.Role = "game listener";
        draft.Notes = "Taken from the port pool for a new realm";
        int64 id = 0;
        ok = Insert(store, draft, id, error);
        if (!ok)
            break;
        PortAllocation allocation;
        if (!Find(store, id, allocation, error))
        {
            ok = false;
            break;
        }
        taken.push_back(allocation);
    }
    CloseSavepoint(store, ok && error.empty());
    if (!ok || !error.empty())
        taken.clear();
    return error.empty();
}
