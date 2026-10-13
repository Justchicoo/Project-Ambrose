/*
 * Project Ambrose by Imjustchico
 * Tests the panel's port allocations: the shipped defaults seed the store on first run, adding proves the port free by binding it and refuses a held port naming it, a duplicate of node, address, port and protocol is refused, a range of 1000 ports is accepted and one of 1001 refused with every added port proven free, an IPv6 bind address is saved, ports below 1025 warn, deleting an allocation an app uses is refused on every path including a bulk delete that deletes nothing and names the assigned one, and the per-node pool supplies free ports for new realms.
 */

#include "LogTestDirectory.h"
#include "PanelStore.h"
#include "PortAllocations.h"
#include "SourceFolder.h"

#include <gtest/gtest.h>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

namespace
{
    bool OpenStore(LogTestDirectory const& directory, PanelStore& store, std::string& error)
    {
        std::vector<std::string> warnings;
        return store.Open(directory.Path() / "panel.sqlite3", Ambrose::FindSourceFolder(), warnings, error);
    }

    bool Transact(PanelStore& store, std::function<bool(std::string&)> work, std::string& error)
    {
        if (!store.Begin(error))
            return false;
        if (!work(error) || !store.Commit(error))
        {
            store.Rollback();
            return false;
        }
        return true;
    }

    uint16 FreePort()
    {
        asio::io_context context;
        asio::ip::tcp::acceptor holder(context);
        std::error_code code;
        holder.open(asio::ip::tcp::v4(), code);
        EXPECT_FALSE(code) << code.message();
        holder.bind(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0), code);
        EXPECT_FALSE(code) << code.message();
        uint16 const port = holder.local_endpoint(code).port();
        EXPECT_FALSE(code) << code.message();
        holder.close(code);
        return port;
    }

    PortAllocationDraft Draft(uint16 port, std::string address = "127.0.0.1")
    {
        PortAllocationDraft draft;
        draft.BindAddress = std::move(address);
        draft.Port = port;
        return draft;
    }

    bool AddOne(PanelStore& store, PortAllocationDraft const& draft, int64& id, std::vector<std::string>& warnings, std::string& refusal, std::string& error)
    {
        return Transact(store, [&](std::string& inner)
        {
            return PortAllocations::Add(store, draft, warnings, id, refusal, inner);
        }, error) && refusal.empty();
    }

    std::vector<PortAllocation> Listed(PanelStore& store, std::string& error)
    {
        std::vector<PortAllocation> allocations;
        EXPECT_TRUE(PortAllocations::List(store, "local", allocations, error)) << error;
        return allocations;
    }

    bool CanBindPrivileged(uint16& port)
    {
        asio::io_context context;
        asio::ip::tcp::acceptor probe(context);
        std::error_code code;
        probe.open(asio::ip::tcp::v4(), code);
        if (code)
            return false;
        probe.bind(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0), code);
        if (code)
            return false;
        uint16 bound = probe.local_endpoint(code).port();
        probe.close(code);
        if (bound >= 1024)
        {
            probe.open(asio::ip::tcp::v4(), code);
            if (code)
                return false;
            probe.bind(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 831), code);
            if (code)
                return false;
            bound = 831;
            probe.close(code);
        }
        port = bound;
        return bound < 1024;
    }
}

TEST(PortAllocationTest, ShippedDefaultsSeedTheStoreOnFirstRun)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    std::vector<PortAllocation> const allocations = Listed(store, error);
    auto const found = [&](std::string_view app, std::string_view role, uint16 port)
    {
        return std::any_of(allocations.begin(), allocations.end(), [&](PortAllocation const& allocation)
        {
            return allocation.App == app && allocation.Role == role && allocation.Port == port && allocation.Protocol == "tcp";
        });
    };
    EXPECT_TRUE(found("loginserver", "login listener", 12000));
    EXPECT_TRUE(found("gameserver", "game listener", 12333));
    EXPECT_TRUE(found("patchserver", "patch HTTP", 12500));
    EXPECT_TRUE(found("supervisor", "panel", 12080));
    EXPECT_TRUE(found("loginserver", "admin API", 12010));
    EXPECT_TRUE(found("gameserver", "admin API", 12343));
    EXPECT_TRUE(found("patchserver", "admin API", 12510));
    EXPECT_TRUE(found("supervisor", "admin API", 12020));

    PortPool pool;
    ASSERT_TRUE(PortAllocations::PoolGet(store, "local", pool, error)) << error;
    EXPECT_GT(pool.EndPort, pool.StartPort);
}

TEST(PortAllocationTest, AddingProvesThePortFreeByBindingIt)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    int64 id = 0;
    std::vector<std::string> warnings;
    std::string refusal;
    ASSERT_TRUE(AddOne(store, Draft(FreePort()), id, warnings, refusal, error)) << error << refusal;
    EXPECT_GT(id, 0);
    EXPECT_TRUE(warnings.empty());
}

TEST(PortAllocationTest, AddingAPortAnotherProgramHoldsIsRefusedWithThePortNamed)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    asio::io_context context;
    asio::ip::tcp::acceptor holder(context);
    std::error_code code;
    holder.open(asio::ip::tcp::v4(), code);
    ASSERT_FALSE(code) << code.message();
    holder.bind(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0), code);
    ASSERT_FALSE(code) << code.message();
    holder.listen(asio::socket_base::max_listen_connections, code);
    ASSERT_FALSE(code) << code.message();
    uint16 const held = holder.local_endpoint(code).port();
    ASSERT_FALSE(code) << code.message();

    int64 id = 0;
    std::vector<std::string> warnings;
    std::string refusal;
    std::string const heldText = std::to_string(held);
    bool const added = Transact(store, [&](std::string& inner)
    {
        return PortAllocations::Add(store, Draft(held), warnings, id, refusal, inner);
    }, error);
    EXPECT_TRUE(added) << error;
    EXPECT_EQ(id, 0);
    EXPECT_NE(refusal.find(heldText), std::string::npos) << "the refusal names the port: " << refusal;
}

TEST(PortAllocationTest, DuplicateNodeAddressPortAndProtocolIsRefused)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    uint16 const port = FreePort();
    int64 id = 0;
    std::vector<std::string> warnings;
    std::string refusal;
    ASSERT_TRUE(AddOne(store, Draft(port), id, warnings, refusal, error)) << error << refusal;

    int64 second = 0;
    bool const added = Transact(store, [&](std::string& inner)
    {
        return PortAllocations::Add(store, Draft(port), warnings, second, refusal, inner);
    }, error);
    EXPECT_TRUE(added) << error;
    EXPECT_EQ(second, 0);
    EXPECT_FALSE(refusal.empty());
}

TEST(PortAllocationTest, RangeOf1000PortsIsAcceptedAnd1001IsRefused)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    PortAllocationDraft draft;
    draft.BindAddress = "127.0.0.1";
    draft.Port = 20000;
    std::vector<std::string> warnings;
    std::size_t added = 0;
    std::string refusal;
    bool const accepted = Transact(store, [&](std::string& inner)
    {
        return PortAllocations::AddRange(store, draft, 20999, warnings, added, refusal, inner);
    }, error);
    EXPECT_TRUE(accepted) << error << refusal;
    EXPECT_EQ(added, 1000u) << refusal;

    std::size_t refused = 0;
    bool const answered = Transact(store, [&](std::string& inner)
    {
        return PortAllocations::AddRange(store, draft, 21000, warnings, refused, refusal, inner);
    }, error);
    EXPECT_TRUE(answered) << error;
    EXPECT_EQ(refused, 0u);
    EXPECT_FALSE(refusal.empty()) << "a range of 1001 ports is refused";
    EXPECT_NE(refusal.find("1000"), std::string::npos) << refusal;
}

TEST(PortAllocationTest, RangeAddsNothingWhenOnePortIsHeld)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    asio::io_context context;
    asio::ip::tcp::acceptor holder(context);
    std::error_code code;
    holder.open(asio::ip::tcp::v4(), code);
    ASSERT_FALSE(code) << code.message();
    holder.bind(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0), code);
    ASSERT_FALSE(code) << code.message();
    holder.listen(asio::socket_base::max_listen_connections, code);
    ASSERT_FALSE(code) << code.message();
    uint16 const held = holder.local_endpoint(code).port();
    ASSERT_FALSE(code) << code.message();

    uint16 const start = static_cast<uint16>(held > 65526 ? 65526 : held);
    PortAllocationDraft draft;
    draft.BindAddress = "127.0.0.1";
    draft.Port = start;
    std::vector<std::string> warnings;
    std::size_t added = 0;
    std::string refusal;
    bool const answered = Transact(store, [&](std::string& inner)
    {
        return PortAllocations::AddRange(store, draft, static_cast<uint16>(start + 9), warnings, added, refusal, inner);
    }, error);
    EXPECT_TRUE(answered) << error;
    EXPECT_EQ(added, 0u);
    EXPECT_NE(refusal.find(std::to_string(held)), std::string::npos) << "the refusal names the held port: " << refusal;
    std::vector<PortAllocation> const after = Listed(store, error);
    EXPECT_TRUE(after.end() == std::find_if(after.begin(), after.end(),
        [&](PortAllocation const& allocation) { return allocation.Port >= start && allocation.Port <= start + 9 && allocation.BindAddress == "127.0.0.1"; }))
        << "no port of the refused range was stored";
}

TEST(PortAllocationTest, Ipv6BindAddressIsSaved)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    asio::io_context context;
    asio::ip::tcp::acceptor probe(context);
    std::error_code code;
    probe.open(asio::ip::tcp::v6(), code);
    if (code)
        GTEST_SKIP() << "no IPv6 here: " << code.message();
    probe.bind(asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), 0), code);
    if (code)
        GTEST_SKIP() << "no IPv6 loopback here: " << code.message();
    uint16 const port = probe.local_endpoint(code).port();
    probe.close(code);

    int64 id = 0;
    std::vector<std::string> warnings;
    std::string refusal;
    ASSERT_TRUE(AddOne(store, Draft(port, "::1"), id, warnings, refusal, error)) << error << refusal;
    PortAllocation saved;
    ASSERT_TRUE(PortAllocations::Find(store, id, saved, error)) << error;
    EXPECT_EQ(saved.BindAddress, "::1");
    EXPECT_EQ(saved.Port, port);
}

TEST(PortAllocationTest, PrivilegedPortWarnsWhenItCanBeBound)
{
    uint16 privileged = 0;
    if (!CanBindPrivileged(privileged))
        GTEST_SKIP() << "binding a port below 1025 needs privileges this run does not have";
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    int64 id = 0;
    std::vector<std::string> warnings;
    std::string refusal;
    ASSERT_TRUE(AddOne(store, Draft(privileged), id, warnings, refusal, error)) << error << refusal;
    EXPECT_GT(id, 0);
    EXPECT_FALSE(warnings.empty()) << "a port below 1025 warns";
}

TEST(PortAllocationTest, DeletingAnAssignedAllocationIsRefused)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    std::vector<PortAllocation> const allocations = Listed(store, error);
    auto const seeded = std::find_if(allocations.begin(), allocations.end(),
        [](PortAllocation const& allocation) { return allocation.App == "gameserver" && allocation.Role == "game listener"; });
    ASSERT_NE(seeded, allocations.end());

    std::string refusal;
    bool const answered = Transact(store, [&](std::string& inner)
    {
        return PortAllocations::Remove(store, seeded->Id, refusal, inner);
    }, error);
    EXPECT_TRUE(answered) << error;
    EXPECT_FALSE(refusal.empty());
    EXPECT_NE(refusal.find("gameserver"), std::string::npos) << refusal;
    PortAllocation kept;
    ASSERT_TRUE(PortAllocations::Find(store, seeded->Id, kept, error)) << error;
    EXPECT_EQ(kept.Id, seeded->Id) << "the assigned allocation is still there";
}

TEST(PortAllocationTest, BulkDeleteWithAnAssignedAllocationDeletesNothingAndNamesIt)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    int64 first = 0;
    int64 second = 0;
    std::vector<std::string> warnings;
    std::string refusal;
    ASSERT_TRUE(AddOne(store, Draft(FreePort()), first, warnings, refusal, error)) << error << refusal;
    ASSERT_TRUE(AddOne(store, Draft(FreePort()), second, warnings, refusal, error)) << error << refusal;

    std::vector<PortAllocation> const allocations = Listed(store, error);
    auto const seeded = std::find_if(allocations.begin(), allocations.end(),
        [](PortAllocation const& allocation) { return allocation.App == "gameserver" && allocation.Role == "game listener"; });
    ASSERT_NE(seeded, allocations.end());

    refusal.clear();
    bool const answered = Transact(store, [&](std::string& inner)
    {
        return PortAllocations::RemoveMany(store, { first, seeded->Id, second }, refusal, inner);
    }, error);
    EXPECT_TRUE(answered) << error;
    EXPECT_NE(refusal.find(std::to_string(seeded->Id)), std::string::npos) << "the refusal names the assigned allocation: " << refusal;
    PortAllocation kept;
    ASSERT_TRUE(PortAllocations::Find(store, first, kept, error)) << error;
    EXPECT_EQ(kept.Id, first) << "the bulk delete removed nothing";
    ASSERT_TRUE(PortAllocations::Find(store, second, kept, error)) << error;
    EXPECT_EQ(kept.Id, second) << "the bulk delete removed nothing";
}

TEST(PortAllocationTest, DeletingAnUnassignedAllocationWorks)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    int64 id = 0;
    std::vector<std::string> warnings;
    std::string refusal;
    ASSERT_TRUE(AddOne(store, Draft(FreePort()), id, warnings, refusal, error)) << error << refusal;

    bool const answered = Transact(store, [&](std::string& inner)
    {
        return PortAllocations::Remove(store, id, refusal, inner);
    }, error);
    EXPECT_TRUE(answered) << error << refusal;
    PortAllocation gone;
    ASSERT_TRUE(PortAllocations::Find(store, id, gone, error)) << error;
    EXPECT_EQ(gone.Id, 0);
}

TEST(PortAllocationTest, AssignRecordsAppAndRoleAndUnassignClearsThem)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    int64 id = 0;
    std::vector<std::string> warnings;
    std::string refusal;
    ASSERT_TRUE(AddOne(store, Draft(FreePort()), id, warnings, refusal, error)) << error << refusal;

    bool const assigned = Transact(store, [&](std::string& inner)
    {
        return PortAllocations::Assign(store, id, "gameserver", "game listener", refusal, inner);
    }, error);
    EXPECT_TRUE(assigned) << error << refusal;
    PortAllocation saved;
    ASSERT_TRUE(PortAllocations::Find(store, id, saved, error)) << error;
    EXPECT_EQ(saved.App, "gameserver");
    EXPECT_EQ(saved.Role, "game listener");

    std::string deleteRefusal;
    bool const deleteAnswered = Transact(store, [&](std::string& inner)
    {
        return PortAllocations::Remove(store, id, deleteRefusal, inner);
    }, error);
    EXPECT_TRUE(deleteAnswered) << error;
    EXPECT_FALSE(deleteRefusal.empty()) << "an assigned allocation cannot be deleted";

    bool const unassigned = Transact(store, [&](std::string& inner)
    {
        return PortAllocations::Unassign(store, id, refusal, inner);
    }, error);
    EXPECT_TRUE(unassigned) << error << refusal;
    ASSERT_TRUE(PortAllocations::Find(store, id, saved, error)) << error;
    EXPECT_TRUE(saved.App.empty());
}

TEST(PortAllocationTest, PoolTakeSuppliesFreePortsForNewRealms)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    std::vector<PortAllocation> taken;
    std::string refusal;
    bool const answered = Transact(store, [&](std::string& inner)
    {
        return PortAllocations::PoolTake(store, "local", 2, taken, refusal, inner);
    }, error);
    EXPECT_TRUE(answered) << error << refusal;
    ASSERT_EQ(taken.size(), 2u);
    EXPECT_NE(taken[0].Port, taken[1].Port);
    for (PortAllocation const& allocation : taken)
    {
        EXPECT_EQ(allocation.BindAddress, "0.0.0.0");
        EXPECT_EQ(allocation.Protocol, "tcp");
        EXPECT_TRUE(allocation.App.empty());
    }
    PortPool pool;
    ASSERT_TRUE(PortAllocations::PoolGet(store, "local", pool, error)) << error;
    for (PortAllocation const& allocation : taken)
    {
        EXPECT_GE(allocation.Port, pool.StartPort);
        EXPECT_LE(allocation.Port, pool.EndPort);
    }
}

TEST(PortAllocationTest, PoolTakeSkipsPortsTheNodeAlreadyHolds)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    PortPool pool;
    ASSERT_TRUE(PortAllocations::PoolGet(store, "local", pool, error)) << error;
    PortAllocationDraft draft;
    draft.BindAddress = "0.0.0.0";
    draft.Port = pool.StartPort;
    int64 id = 0;
    std::vector<std::string> warnings;
    std::string refusal;
    ASSERT_TRUE(AddOne(store, draft, id, warnings, refusal, error)) << error << refusal;

    std::vector<PortAllocation> taken;
    bool const answered = Transact(store, [&](std::string& inner)
    {
        return PortAllocations::PoolTake(store, "local", 1, taken, refusal, inner);
    }, error);
    EXPECT_TRUE(answered) << error << refusal;
    ASSERT_EQ(taken.size(), 1u);
    EXPECT_NE(taken[0].Port, pool.StartPort) << "a port the node already holds is skipped";
}

TEST(PortAllocationTest, ListenSettingMapsAppAndRole)
{
    EXPECT_EQ(PortAllocations::ListenSetting("loginserver", "login listener"), "LoginServerPort");
    EXPECT_EQ(PortAllocations::ListenSetting("gameserver", "game listener"), "WorldServerPort");
    EXPECT_EQ(PortAllocations::ListenSetting("patchserver", "patch HTTP"), "PatchServerPort");
    EXPECT_TRUE(PortAllocations::ListenSetting("supervisor", "panel").empty());
    EXPECT_TRUE(PortAllocations::ListenSetting("gameserver", "metrics").empty());
}

TEST(PortAllocationTest, BindAddressesAreStoredInCanonicalForm)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;

    asio::io_context context;
    asio::ip::tcp::acceptor probe(context);
    std::error_code code;
    probe.open(asio::ip::tcp::v6(), code);
    if (code)
        GTEST_SKIP() << "no IPv6 here: " << code.message();
    probe.bind(asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), 0), code);
    if (code)
        GTEST_SKIP() << "no IPv6 loopback here: " << code.message();
    uint16 const port = probe.local_endpoint(code).port();
    probe.close(code);

    int64 id = 0;
    std::vector<std::string> warnings;
    std::string refusal;
    ASSERT_TRUE(AddOne(store, Draft(port, "0:0:0:0:0:0:0:1"), id, warnings, refusal, error)) << error << refusal;
    PortAllocation saved;
    ASSERT_TRUE(PortAllocations::Find(store, id, saved, error)) << error;
    EXPECT_EQ(saved.BindAddress, "::1") << "the long form is stored canonically";

    int64 second = 0;
    refusal.clear();
    bool const added = Transact(store, [&](std::string& inner)
    {
        return PortAllocations::Add(store, Draft(port, "::1"), warnings, second, refusal, inner);
    }, error);
    EXPECT_TRUE(added) << error;
    EXPECT_EQ(second, 0);
    EXPECT_FALSE(refusal.empty()) << "the same address in another spelling is the same allocation";
}
