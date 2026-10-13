/*
 * Project Ambrose by Imjustchico
 * The panel's port allocations: one row per socket an app listens on, with its bind address, port, protocol, public alias, the app and role using it and notes, unique by node, address, port and protocol, seeded from the shipped defaults by the panel's SQL updates, plus the per-node port pool that supplies ports for new realms; adding proves every port free by binding it, a range holds at most 1000 ports, and an allocation an app uses cannot be deleted on any path.
 */

#ifndef AMBROSE_PORTALLOCATIONS_H
#define AMBROSE_PORTALLOCATIONS_H

#include "Types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class PanelStore;

struct PortAllocation
{
    int64 Id = 0;
    std::string Node;
    std::string BindAddress;
    uint16 Port = 0;
    std::string Protocol;
    std::string PublicAlias;
    std::string App;
    std::string Role;
    std::string Notes;
};

struct PortAllocationDraft
{
    std::string Node = "local";
    std::string BindAddress;
    uint16 Port = 0;
    std::string Protocol = "tcp";
    std::string PublicAlias;
    std::string App;
    std::string Role;
    std::string Notes;
};

struct PortPool
{
    std::string Node;
    uint16 StartPort = 0;
    uint16 EndPort = 0;
};

class PortAllocations
{
public:
    static constexpr std::string_view LocalNode = "local";
    static constexpr uint32 MaxRangePorts = 1000;
    static constexpr uint16 PrivilegedPortMax = 1024;

    static bool IsKnownNode(std::string_view node) noexcept;
    static bool IsKnownRole(std::string_view role) noexcept;
    static bool IsKnownApp(std::string_view app) noexcept;
    static std::string ListenSetting(std::string_view app, std::string_view role);
    static std::vector<std::string> Roles() noexcept;
    static std::vector<std::string> Apps() noexcept;

    static bool List(PanelStore& store, std::string_view node, std::vector<PortAllocation>& allocations, std::string& error);
    static bool Find(PanelStore& store, int64 id, PortAllocation& allocation, std::string& error);
    static bool Add(PanelStore& store, PortAllocationDraft const& draft, std::vector<std::string>& warnings, int64& id, std::string& refusal, std::string& error);
    static bool AddRange(PanelStore& store, PortAllocationDraft const& draft, uint16 endPort, std::vector<std::string>& warnings, std::size_t& added,
        std::string& refusal, std::string& error);
    static bool Remove(PanelStore& store, int64 id, std::string& refusal, std::string& error);
    static bool RemoveMany(PanelStore& store, std::vector<int64> const& ids, std::string& refusal, std::string& error);
    static bool Assign(PanelStore& store, int64 id, std::string_view app, std::string_view role, std::string& refusal, std::string& error);
    static bool Unassign(PanelStore& store, int64 id, std::string& refusal, std::string& error);
    static bool PoolGet(PanelStore& store, std::string_view node, PortPool& pool, std::string& error);
    static bool PoolSet(PanelStore& store, std::string_view node, uint16 startPort, uint16 endPort, std::string& refusal, std::string& error);
    static bool PoolTake(PanelStore& store, std::string_view node, std::size_t count, std::vector<PortAllocation>& taken, std::string& refusal, std::string& error);

private:
    static bool CheckDraft(PortAllocationDraft const& draft, std::string& refusal);
    static bool ProbeFree(std::string_view protocol, std::string_view address, uint16 port, std::string& refusal);
    static void PrivilegeWarning(uint16 port, std::vector<std::string>& warnings);
    static bool Exists(PanelStore& store, std::string_view node, std::string_view address, uint16 port, std::string_view protocol, std::string& error);
};

#endif
