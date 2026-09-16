/*
 * Project Ambrose by Imjustchico
 * With AMBROSE_TEST_DB set, tests the worker pool: async queries from many threads, exclusive sync leases, draining and deadline cancels on close, query holders, refused statements, TryQuery telling empty results from failures, keepalive past wait_timeout, and refusing work when closed.
 */

#include "DatabaseWorkerPool.h"
#include "Environment.h"
#include "MySQLConnection.h"
#include "QueryHolder.h"
#include "QueryResult.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <atomic>
#include <mutex>
#include <set>
#include <thread>

namespace
{
    class PoolTestConnection : public MySQLConnection
    {
    public:
        enum Statements : uint32
        {
            POOL_SEL_ECHO,
            POOL_SEL_CONNECTION_ID,
            POOL_SEL_ASYNC_ONLY,
            MAX_POOL_STATEMENTS
        };

        using MySQLConnection::MySQLConnection;

    protected:
        void DoPrepareStatements() override
        {
            PrepareStatement(POOL_SEL_ECHO, "POOL_SEL_ECHO", "SELECT CAST(? AS UNSIGNED)", ConnectionFlags::Both);
            PrepareStatement(POOL_SEL_CONNECTION_ID, "POOL_SEL_CONNECTION_ID", "SELECT CONNECTION_ID()", ConnectionFlags::Both);
            PrepareStatement(POOL_SEL_ASYNC_ONLY, "POOL_SEL_ASYNC_ONLY", "SELECT 1", ConnectionFlags::Async);
        }
    };

    using TestPool = DatabaseWorkerPool<PoolTestConnection>;

    std::optional<std::string> TestDatabase()
    {
        std::optional<std::string> text = Ambrose::GetEnv("AMBROSE_TEST_DB");
        if (!text || text->empty())
            return std::nullopt;
        return text;
    }

    bool PumpUntilDone(std::vector<QueryCallback>& callbacks, std::chrono::seconds timeout)
    {
        auto const deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            bool done = true;
            for (QueryCallback& callback : callbacks)
                done = callback.InvokeIfReady() && done;
            if (done)
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return false;
    }
}

TEST(DatabaseWorkerPoolTest, ClosedPoolRefusesWorkWithoutBlocking)
{
    TestPool pool("closed");
    EXPECT_FALSE(pool.IsOpen());
    EXPECT_FALSE(pool.DirectExecute("SELECT 1"));
    EXPECT_FALSE(pool.Query("SELECT 1"));
    QueryResult refused;
    EXPECT_FALSE(pool.TryQuery("SELECT 1", refused));
    QueryCallback callback = pool.AsyncQuery("SELECT 1");
    bool ran = false;
    callback.WithCallback([&ran](QueryResult result) { ran = !result; });
    EXPECT_TRUE(callback.InvokeIfReady());
    EXPECT_TRUE(ran);
    EXPECT_FALSE(pool.SetConnectionInfo("not;enough", 1, 1));
    EXPECT_NE(pool.Open(), 0u);
}

TEST(DatabaseWorkerPoolTest, ThousandAsyncQueriesFromEightThreadsComplete)
{
    std::optional<std::string> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    TestPool pool("async");
    ASSERT_TRUE(pool.SetConnectionInfo(*info, 4, 1));
    ASSERT_EQ(pool.Open(), 0u);
    EXPECT_EQ(pool.GetAsyncConnectionCount(), 4u);

    constexpr std::size_t Threads = 8;
    constexpr std::size_t PerThread = 125;
    std::vector<uint64> results(Threads * PerThread, 0);
    std::mutex callbacksMutex;
    std::vector<QueryCallback> callbacks;
    callbacks.reserve(Threads * PerThread);
    std::vector<std::thread> producers;
    for (std::size_t thread = 0; thread < Threads; ++thread)
    {
        producers.emplace_back([&, thread]
        {
            for (std::size_t i = 0; i < PerThread; ++i)
            {
                std::size_t const slot = thread * PerThread + i;
                uint64 const value = 1000000 + slot;
                QueryCallback callback = (slot % 2 == 0)
                    ? [&] { auto statement = pool.GetPreparedStatement(PoolTestConnection::POOL_SEL_ECHO); if (statement) statement->SetData(0, value); return pool.AsyncQuery(std::move(statement)).WithPreparedCallback([&results, slot](PreparedQueryResult result) { results[slot] = result ? (*result)[0].Get<uint64>() : 0; }); }()
                    : pool.AsyncQuery(fmt::format("SELECT {}", value)).WithCallback([&results, slot](QueryResult result) { results[slot] = result ? (*result)[0].Get<uint64>() : 0; });
                std::lock_guard<std::mutex> lock(callbacksMutex);
                callbacks.push_back(std::move(callback));
            }
        });
    }
    for (std::thread& producer : producers)
        producer.join();
    ASSERT_TRUE(PumpUntilDone(callbacks, std::chrono::seconds(60)));
    for (std::size_t slot = 0; slot < results.size(); ++slot)
        ASSERT_EQ(results[slot], 1000000 + slot) << slot;
    EXPECT_EQ(pool.GetConcurrentUseCount(), 0u);
    pool.Close();
}

TEST(DatabaseWorkerPoolTest, SyncQueriesNeverShareAConnection)
{
    std::optional<std::string> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    TestPool pool("sync");
    ASSERT_TRUE(pool.SetConnectionInfo(*info, 0, 2));
    ASSERT_EQ(pool.Open(), 0u);

    std::mutex idsMutex;
    std::set<uint64> ids;
    std::atomic<int> failures{ 0 };
    std::vector<std::thread> threads;
    for (int thread = 0; thread < 4; ++thread)
    {
        threads.emplace_back([&, thread]
        {
            for (int i = 0; i < 50; ++i)
            {
                uint64 id = 0;
                if ((thread + i) % 2 == 0)
                {
                    QueryResult const result = pool.Query("SELECT CONNECTION_ID(), SLEEP(0.001)");
                    id = result ? (*result)[0].Get<uint64>() : 0;
                }
                else
                {
                    auto const statement = pool.GetPreparedStatement(PoolTestConnection::POOL_SEL_CONNECTION_ID);
                    PreparedQueryResult const result = statement ? pool.Query(*statement) : nullptr;
                    id = result ? (*result)[0].Get<uint64>() : 0;
                }
                if (id == 0)
                    failures.fetch_add(1);
                std::lock_guard<std::mutex> lock(idsMutex);
                ids.insert(id);
            }
        });
    }
    for (std::thread& thread : threads)
        thread.join();
    EXPECT_EQ(failures.load(), 0);
    EXPECT_EQ(ids.size(), 2u);
    EXPECT_EQ(pool.GetConcurrentUseCount(), 0u);
}

TEST(DatabaseWorkerPoolTest, CloseDrainsQueuedWorkAndSettlesEveryCallback)
{
    std::optional<std::string> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    TestPool pool("drain");
    ASSERT_TRUE(pool.SetConnectionInfo(*info, 2, 1));
    ASSERT_EQ(pool.Open(), 0u);

    std::atomic<int> answered{ 0 };
    std::vector<QueryCallback> callbacks;
    for (int i = 0; i < 500; ++i)
        callbacks.push_back(pool.AsyncQuery(fmt::format("SELECT {}", i)).WithCallback([&answered](QueryResult result) { if (result) answered.fetch_add(1); }));
    pool.Close();
    EXPECT_FALSE(pool.IsOpen());
    for (QueryCallback& callback : callbacks)
        EXPECT_TRUE(callback.IsReady());
    ASSERT_TRUE(PumpUntilDone(callbacks, std::chrono::seconds(5)));
    EXPECT_EQ(answered.load(), 500);

    QueryCallback late = pool.AsyncQuery("SELECT 1");
    EXPECT_TRUE(late.IsReady());
}

TEST(DatabaseWorkerPoolTest, QueryHolderRunsEverySlotOnOneConnection)
{
    std::optional<std::string> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    TestPool pool("holder");
    ASSERT_TRUE(pool.SetConnectionInfo(*info, 2, 1));
    ASSERT_EQ(pool.Open(), 0u);

    auto holder = std::make_shared<SQLQueryHolder<PoolTestConnection>>(4);
    for (std::size_t slot = 0; slot < 2; ++slot)
    {
        auto statement = pool.GetPreparedStatement(PoolTestConnection::POOL_SEL_ECHO);
        ASSERT_TRUE(statement);
        statement->SetData(0, uint64{ 40 + slot });
        EXPECT_TRUE(holder->SetPreparedQuery(slot, std::move(statement)));
    }
    EXPECT_TRUE(holder->SetPreparedQuery(2, pool.GetPreparedStatement(PoolTestConnection::POOL_SEL_CONNECTION_ID)));
    EXPECT_TRUE(holder->SetPreparedQuery(3, pool.GetPreparedStatement(PoolTestConnection::POOL_SEL_CONNECTION_ID)));
    EXPECT_FALSE(holder->SetPreparedQuery(4, pool.GetPreparedStatement(PoolTestConnection::POOL_SEL_ECHO)));
    SQLQueryHolderCallback done = pool.DelayQueryHolder(holder);
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    bool finished = false;
    while (!(finished = done.InvokeIfReady()) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    ASSERT_TRUE(finished);
    for (std::size_t slot = 0; slot < 4; ++slot)
        ASSERT_TRUE(holder->GetPreparedResult(slot)) << slot;
    EXPECT_EQ((*holder->GetPreparedResult(0))[0].Get<uint64>(), 40u);
    EXPECT_EQ((*holder->GetPreparedResult(1))[0].Get<uint64>(), 41u);
    EXPECT_EQ((*holder->GetPreparedResult(2))[0].Get<uint64>(), (*holder->GetPreparedResult(3))[0].Get<uint64>());

    SQLQueryHolderCallback empty = pool.DelayQueryHolder(nullptr);
    EXPECT_TRUE(empty.IsReady());
}

TEST(DatabaseWorkerPoolTest, InvalidStatementsAreRefusedAtTheCallSite)
{
    std::optional<std::string> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    TestPool pool("refuse");
    ASSERT_TRUE(pool.SetConnectionInfo(*info, 1, 1));
    EXPECT_FALSE(pool.GetPreparedStatement(PoolTestConnection::POOL_SEL_ECHO));
    ASSERT_EQ(pool.Open(), 0u);

    EXPECT_FALSE(pool.GetPreparedStatement(PoolTestConnection::MAX_POOL_STATEMENTS));
    pool.Execute(std::unique_ptr<TestPool::Statement>());
    QueryCallback empty = pool.AsyncQuery(std::unique_ptr<TestPool::Statement>());
    EXPECT_TRUE(empty.IsReady());

    auto asyncOnly = pool.GetPreparedStatement(PoolTestConnection::POOL_SEL_ASYNC_ONLY);
    ASSERT_TRUE(asyncOnly);
    EXPECT_FALSE(pool.Query(*asyncOnly));
    QueryCallback allowed = pool.AsyncQuery(std::move(asyncOnly));
    bool answered = false;
    allowed.WithPreparedCallback([&answered](PreparedQueryResult result) { answered = result && (*result)[0].Get<int32>() == 1; });
    std::vector<QueryCallback> callbacks;
    callbacks.push_back(std::move(allowed));
    ASSERT_TRUE(PumpUntilDone(callbacks, std::chrono::seconds(30)));
    EXPECT_TRUE(answered);

    pool.Close();
    EXPECT_TRUE(pool.GetPreparedStatement(PoolTestConnection::POOL_SEL_ECHO));
}

TEST(DatabaseWorkerPoolTest, TryQuerySeparatesEmptyResultsFromFailures)
{
    std::optional<std::string> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    TestPool pool("try");
    ASSERT_TRUE(pool.SetConnectionInfo(*info, 1, 1));
    ASSERT_EQ(pool.Open(), 0u);

    QueryResult text;
    EXPECT_TRUE(pool.TryQuery("SELECT 1 FROM DUAL WHERE 1 = 0", text));
    EXPECT_FALSE(text);
    EXPECT_TRUE(pool.TryQuery("SELECT 7", text));
    ASSERT_TRUE(text);
    EXPECT_EQ((*text)[0].Get<uint32>(), 7u);
    EXPECT_FALSE(pool.TryQuery("SELECT * FROM `ambrose_try_query_missing_table`", text));
    EXPECT_FALSE(text);

    auto statement = pool.GetPreparedStatement(PoolTestConnection::POOL_SEL_ECHO);
    ASSERT_TRUE(statement);
    statement->SetData(0, uint32{ 9 });
    PreparedQueryResult prepared;
    EXPECT_TRUE(pool.TryQuery(*statement, prepared));
    ASSERT_TRUE(prepared);
    EXPECT_EQ((*prepared)[0].Get<uint32>(), 9u);
    auto asyncOnly = pool.GetPreparedStatement(PoolTestConnection::POOL_SEL_ASYNC_ONLY);
    ASSERT_TRUE(asyncOnly);
    EXPECT_FALSE(pool.TryQuery(*asyncOnly, prepared));
    EXPECT_FALSE(prepared);

    pool.Close();
    EXPECT_FALSE(pool.TryQuery(*statement, prepared));
}

TEST(DatabaseWorkerPoolTest, CloseCancelsWorkLeftAfterTheDrainDeadline)
{
    std::optional<std::string> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    TestPool pool("deadline");
    ASSERT_TRUE(pool.SetConnectionInfo(*info, 1, 1));
    ASSERT_EQ(pool.Open(), 0u);

    std::vector<QueryCallback> callbacks;
    std::atomic<int> answered{ 0 };
    std::atomic<int> cancelled{ 0 };
    for (int i = 0; i < 100; ++i)
        callbacks.push_back(pool.AsyncQuery("SELECT SLEEP(0.05)").WithCallback([&](QueryResult result) { (result ? answered : cancelled).fetch_add(1); }));
    auto const start = std::chrono::steady_clock::now();
    pool.Close(std::chrono::milliseconds(200));
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(3));
    ASSERT_TRUE(PumpUntilDone(callbacks, std::chrono::seconds(5)));
    EXPECT_EQ(answered.load() + cancelled.load(), 100);
    EXPECT_GT(cancelled.load(), 0);
}

TEST(DatabaseWorkerPoolTest, KeepAliveHoldsConnectionsPastWaitTimeout)
{
    std::optional<std::string> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    TestPool pool("keepalive");
    ASSERT_TRUE(pool.SetConnectionInfo(*info, 1, 1));
    pool.SetKeepAliveInterval(std::chrono::milliseconds(1000));
    ASSERT_EQ(pool.Open(), 0u);

    ASSERT_TRUE(pool.DirectExecute("SET SESSION wait_timeout = 5"));
    QueryResult const first = pool.Query("SELECT CONNECTION_ID()");
    ASSERT_TRUE(first);
    uint64 const syncId = (*first)[0].Get<uint64>();

    std::vector<QueryCallback> setup;
    uint64 asyncId = 0;
    setup.push_back(pool.AsyncQuery("SET SESSION wait_timeout = 5"));
    setup.push_back(pool.AsyncQuery("SELECT CONNECTION_ID()").WithCallback([&asyncId](QueryResult result) { asyncId = result ? (*result)[0].Get<uint64>() : 0; }));
    ASSERT_TRUE(PumpUntilDone(setup, std::chrono::seconds(30)));
    ASSERT_NE(asyncId, 0u);

    std::this_thread::sleep_for(std::chrono::seconds(7));

    QueryResult const second = pool.Query("SELECT CONNECTION_ID(), @@SESSION.wait_timeout");
    ASSERT_TRUE(second);
    EXPECT_EQ((*second)[0].Get<uint64>(), syncId);
    EXPECT_EQ((*second)[1].Get<uint32>(), 5u);
    uint64 asyncAfter = 0;
    std::vector<QueryCallback> check;
    check.push_back(pool.AsyncQuery("SELECT CONNECTION_ID()").WithCallback([&asyncAfter](QueryResult result) { asyncAfter = result ? (*result)[0].Get<uint64>() : 0; }));
    ASSERT_TRUE(PumpUntilDone(check, std::chrono::seconds(30)));
    EXPECT_EQ(asyncAfter, asyncId);
    EXPECT_EQ(pool.GetReconnectCount(), 0u);
}
