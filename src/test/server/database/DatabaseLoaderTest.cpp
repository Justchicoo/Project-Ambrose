/*
 * Project Ambrose by Imjustchico
 * With AMBROSE_TEST_DB set, tests transactions, deadlock retries, chained callbacks on the polling thread, holder callbacks, the loader's open and failure paths, and live pool reconfiguration; and without it, that a database's pending update folder is the one under Updates.SourcePath, or under the folder the build came from when it names none.
 */

#include "AsyncCallbackProcessor.h"
#include "ConfigMgr.h"
#include "DatabaseLoader.h"
#include "DatabaseWorkerPool.h"
#include "Environment.h"
#include "Log.h"
#include "LogTestConfig.h"
#include "LogTestDirectory.h"
#include "MySQLConnection.h"
#include "QueryHolder.h"
#include "QueryResult.h"
#include "ScopeExit.h"
#include "SourceFolder.h"
#include "TestAppender.h"
#include "Transaction.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <fstream>
#include <thread>

namespace
{
    class LoaderTestConnection : public MySQLConnection
    {
    public:
        enum Statements : uint32
        {
            LOADER_SEL_ECHO,
            MAX_LOADER_STATEMENTS
        };

        using MySQLConnection::MySQLConnection;

    protected:
        void DoPrepareStatements() override
        {
            PrepareStatement(LOADER_SEL_ECHO, "LOADER_SEL_ECHO", "SELECT CAST(? AS UNSIGNED)", ConnectionFlags::Both);
        }
    };

    class BrokenLoginConnection : public MySQLConnection
    {
    public:
        enum Statements : uint32
        {
            LOGIN_SEL_BROKEN,
            MAX_BROKEN_STATEMENTS
        };

        using MySQLConnection::MySQLConnection;

    protected:
        void DoPrepareStatements() override
        {
            PrepareStatement(LOGIN_SEL_BROKEN, "LOGIN_SEL_BROKEN", "SELEC nothing", ConnectionFlags::Both);
        }
    };

    using LoaderPool = DatabaseWorkerPool<LoaderTestConnection>;

    std::optional<std::string> TestDatabase()
    {
        std::optional<std::string> text = Ambrose::GetEnv("AMBROSE_TEST_DB");
        if (!text || text->empty())
            return std::nullopt;
        return text;
    }

    template<typename Callback>
    bool Pump(AsyncCallbackProcessor<Callback>& processor, std::chrono::seconds timeout)
    {
        auto const deadline = std::chrono::steady_clock::now() + timeout;
        while (processor.GetPendingCount() > 0)
        {
            if (std::chrono::steady_clock::now() > deadline)
                return false;
            processor.ProcessReadyCallbacks();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return true;
    }

    struct CapturedLog
    {
        CapturedLog() : Store(std::make_shared<TestAppenderStore>())
        {
            sLog.RegisterAppenderType(TestAppender::GetTypeInfo(Store));
            sLog.Apply(LogTestConfig::Settings("Appender.Capture = 200,1,0\nLogger.root = 3,Capture\n"));
        }

        ~CapturedLog()
        {
            sLog.Reset();
        }

        bool Contains(std::string_view text) const
        {
            for (LogMessage const& message : Store->Messages("Capture"))
                if (message.Text.find(text) != std::string::npos)
                    return true;
            return false;
        }

        std::shared_ptr<TestAppenderStore> Store;
    };

    std::unique_ptr<ConfigMgr> LoadConfig(LogTestDirectory const& directory, std::string const& body)
    {
        std::filesystem::path const file = directory.Path() / "loader.conf";
        std::ofstream(file) << body;
        auto config = std::make_unique<ConfigMgr>();
        EXPECT_TRUE(config->LoadInitial(file).Succeeded());
        return config;
    }
}

TEST(DatabaseTransactionTest, UniqueKeyViolationRollsBackEveryRow)
{
    std::optional<std::string> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    LoaderPool pool("transactions");
    ASSERT_TRUE(pool.SetConnectionInfo(*info, 1, 1));
    ASSERT_EQ(pool.Open(), 0u);
    ASSERT_TRUE(pool.DirectExecute("DROP TABLE IF EXISTS ambrose_unique"));
    ASSERT_TRUE(pool.DirectExecute("CREATE TABLE ambrose_unique (id INT PRIMARY KEY) ENGINE=InnoDB"));
    ScopeExit const drop([&pool] { pool.DirectExecute("DROP TABLE IF EXISTS ambrose_unique"); });

    auto failing = pool.BeginTransaction();
    failing->Append("INSERT INTO ambrose_unique VALUES (1)");
    failing->Append("INSERT INTO ambrose_unique VALUES (1)");
    EXPECT_FALSE(pool.DirectCommitTransaction(failing));
    QueryResult const count = pool.Query("SELECT COUNT(*) FROM ambrose_unique");
    ASSERT_TRUE(count);
    EXPECT_EQ((*count)[0].Get<uint64>(), 0u);

    AsyncCallbackProcessor<TransactionCallback> processor;
    bool committed = false;
    auto working = pool.BeginTransaction();
    working->Append("INSERT INTO ambrose_unique VALUES (1)");
    working->Append("INSERT INTO ambrose_unique VALUES (2)");
    processor.AddCallback(pool.AsyncCommitTransaction(working).AfterComplete([&committed](bool success) { committed = success; }));
    ASSERT_TRUE(Pump(processor, std::chrono::seconds(30)));
    EXPECT_TRUE(committed);
    QueryResult const after = pool.Query("SELECT COUNT(*) FROM ambrose_unique");
    ASSERT_TRUE(after);
    EXPECT_EQ((*after)[0].Get<uint64>(), 2u);
}

TEST(DatabaseTransactionTest, DeadlockLoserRetriesAndBothCommit)
{
    std::optional<std::string> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    LoaderPool pool("deadlock");
    ASSERT_TRUE(pool.SetConnectionInfo(*info, 2, 1));
    ASSERT_EQ(pool.Open(), 0u);
    ASSERT_TRUE(pool.DirectExecute("DROP TABLE IF EXISTS ambrose_deadlock"));
    ASSERT_TRUE(pool.DirectExecute("CREATE TABLE ambrose_deadlock (id INT PRIMARY KEY, hits INT NOT NULL) ENGINE=InnoDB"));
    ScopeExit const drop([&pool] { pool.DirectExecute("DROP TABLE IF EXISTS ambrose_deadlock"); });
    ASSERT_TRUE(pool.DirectExecute("INSERT INTO ambrose_deadlock VALUES (1, 0), (2, 0)"));

    CapturedLog log;
    AsyncCallbackProcessor<TransactionCallback> processor;
    std::atomic<int> committed{ 0 };
    for (int first : { 1, 2 })
    {
        int const second = first == 1 ? 2 : 1;
        auto transaction = pool.BeginTransaction();
        transaction->Append(fmt::format("UPDATE ambrose_deadlock SET hits = hits + 1 WHERE id = {}", first));
        transaction->Append("SELECT SLEEP(0.5)");
        transaction->Append(fmt::format("UPDATE ambrose_deadlock SET hits = hits + 1 WHERE id = {}", second));
        processor.AddCallback(pool.AsyncCommitTransaction(transaction).AfterComplete([&committed](bool success) { if (success) committed.fetch_add(1); }));
    }
    ASSERT_TRUE(Pump(processor, std::chrono::seconds(60)));
    EXPECT_EQ(committed.load(), 2);
    EXPECT_TRUE(log.Contains("[1213]") || log.Contains("[1205]"));
    QueryResult const rows = pool.Query("SELECT SUM(hits) FROM ambrose_deadlock");
    ASSERT_TRUE(rows);
    EXPECT_EQ((*rows)[0].Get<uint64>(), 4u);
}

TEST(DatabaseTransactionTest, ChainedCallbacksRunOnThePollingThread)
{
    std::optional<std::string> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    LoaderPool pool("chain");
    ASSERT_TRUE(pool.SetConnectionInfo(*info, 2, 1));
    ASSERT_EQ(pool.Open(), 0u);

    std::thread::id firstThread;
    std::thread::id secondThread;
    uint64 finalValue = 0;
    AsyncCallbackProcessor<QueryCallback> processor;
    processor.AddCallback(pool.AsyncQuery("SELECT 20")
        .WithChainingCallback([&](QueryCallback& callback, QueryResult result)
        {
            firstThread = std::this_thread::get_id();
            uint64 const value = result ? (*result)[0].Get<uint64>() : 0;
            auto statement = pool.GetPreparedStatement(LoaderTestConnection::LOADER_SEL_ECHO);
            ASSERT_TRUE(statement);
            statement->SetData(0, value + 22);
            callback.SetNextQuery(pool.AsyncQuery(std::move(statement)));
        })
        .WithChainingPreparedCallback([&](QueryCallback&, PreparedQueryResult result)
        {
            secondThread = std::this_thread::get_id();
            finalValue = result ? (*result)[0].Get<uint64>() : 0;
        }));
    ASSERT_TRUE(Pump(processor, std::chrono::seconds(30)));
    EXPECT_EQ(finalValue, 42u);
    EXPECT_EQ(firstThread, std::this_thread::get_id());
    EXPECT_EQ(secondThread, std::this_thread::get_id());

    auto holder = std::make_shared<SQLQueryHolder<LoaderTestConnection>>(1);
    auto statement = pool.GetPreparedStatement(LoaderTestConnection::LOADER_SEL_ECHO);
    ASSERT_TRUE(statement);
    statement->SetData(0, uint64{ 7 });
    holder->SetPreparedQuery(0, std::move(statement));
    AsyncCallbackProcessor<SQLQueryHolderCallback> holders;
    uint64 held = 0;
    holders.AddCallback(pool.DelayQueryHolder(holder).AfterComplete([&held](SQLQueryHolderBase const& done) { held = done.GetPreparedResult(0) ? (*done.GetPreparedResult(0))[0].Get<uint64>() : 0; }));
    ASSERT_TRUE(Pump(holders, std::chrono::seconds(30)));
    EXPECT_EQ(held, 7u);
}

TEST(DatabaseCallbackTest, ProcessorIsolatesThrowingCallbacksAndChainsRunInOrder)
{
    std::promise<QueryResult> first;
    std::promise<QueryResult> second;
    std::promise<QueryResult> third;
    std::vector<int> order;
    AsyncCallbackProcessor<QueryCallback> processor;
    processor.AddCallback(QueryCallback(first.get_future()).WithCallback([](QueryResult) { throw std::runtime_error("boom"); }));
    processor.AddCallback(QueryCallback(second.get_future())
        .WithChainingCallback([&](QueryCallback& callback, QueryResult)
        {
            order.push_back(1);
            callback.SetNextQuery(QueryCallback(third.get_future()).WithCallback([&](QueryResult) { order.push_back(2); }));
        })
        .WithCallback([&](QueryResult) { order.push_back(3); }));
    processor.ProcessReadyCallbacks();
    EXPECT_EQ(processor.GetPendingCount(), 2u);
    first.set_value(nullptr);
    second.set_value(nullptr);
    processor.ProcessReadyCallbacks();
    EXPECT_EQ(processor.GetPendingCount(), 1u);
    third.set_value(nullptr);
    processor.ProcessReadyCallbacks();
    EXPECT_EQ(processor.GetPendingCount(), 0u);
    EXPECT_EQ(order, (std::vector<int>{ 1, 2 }));
}

TEST(DatabaseTransactionTest, InvalidOrResubmittedTransactionsAreRefused)
{
    std::optional<std::string> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    LoaderPool pool("refused");
    ASSERT_TRUE(pool.SetConnectionInfo(*info, 1, 1));
    ASSERT_EQ(pool.Open(), 0u);

    auto withHole = pool.BeginTransaction();
    withHole->Append("SELECT 1");
    withHole->Append(std::unique_ptr<LoaderPool::Statement>());
    EXPECT_FALSE(withHole->IsValid());
    EXPECT_FALSE(pool.DirectCommitTransaction(withHole));

    auto once = pool.BeginTransaction();
    once->Append("SELECT 1");
    EXPECT_TRUE(pool.DirectCommitTransaction(once));
    EXPECT_FALSE(pool.DirectCommitTransaction(once));
    once->Append("SELECT 2");
    EXPECT_FALSE(once->IsValid());
}

TEST(DatabaseLoaderTest, PendingUpdatesLiveUnderTheUpdatersSourceFolder)
{
    LogTestDirectory directory;
    std::string source = ConfigMgr::PathToUtf8(directory.Path() / "source");
    std::replace(source.begin(), source.end(), '\\', '/');
    auto const configured = LoadConfig(directory, fmt::format("Updates.SourcePath = \"{}\"\n", source));
    EXPECT_EQ(DatabaseLoader::PendingUpdatesFolder(*configured, "world"), directory.Path() / "source" / "data" / "sql" / "updates" / "pending_db_world");

    auto const unset = LoadConfig(directory, "");
    EXPECT_EQ(DatabaseLoader::PendingUpdatesFolder(*unset, "characters"), Ambrose::FindSourceFolder() / "data" / "sql" / "updates" / "pending_db_characters")
        << "with no source path named, pending updates are read from the folder the build came from, never from wherever the server runs";
}

TEST(DatabaseLoaderTest, BadInfoFailsAndValidInfoLogsThePool)
{
    LogTestDirectory directory;
    {
        CapturedLog log;
        LoaderPool pool("login");
        auto const config = LoadConfig(directory, "LoginDatabaseInfo = not a connection string\n");
        DatabaseLoader loader(*config);
        loader.AddDatabase(pool, "Login");
        EXPECT_FALSE(loader.Load());
        EXPECT_FALSE(pool.IsOpen());
        EXPECT_TRUE(log.Contains("LoginDatabaseInfo is not a valid connection string"));
    }

    std::optional<std::string> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    CapturedLog log;
    LoaderPool pool("login");
    auto const config = LoadConfig(directory, fmt::format("LoginDatabaseInfo = \"{}\"\nLoginDatabase.WorkerThreads = 1\nLoginDatabase.SynchThreads = 1\n", *info));
    DatabaseLoader loader(*config);
    loader.AddDatabase(pool, "Login");
    ASSERT_TRUE(loader.Load());
    EXPECT_TRUE(pool.IsOpen());
    EXPECT_TRUE(log.Contains("Opened database connection pool login: 1 async, 1 sync"));
    loader.Close();
    EXPECT_FALSE(pool.IsOpen());
}

TEST(DatabaseLoaderTest, BrokenStatementStopsTheLoadAndUnwinds)
{
    std::optional<std::string> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    LogTestDirectory directory;
    CapturedLog log;
    LoaderPool first("world");
    DatabaseWorkerPool<BrokenLoginConnection> broken("login");
    auto const config = LoadConfig(directory, fmt::format("WorldDatabaseInfo = \"{}\"\nLoginDatabaseInfo = \"{}\"\n", *info, *info));
    DatabaseLoader loader(*config);
    loader.AddDatabase(first, "World").AddDatabase(broken, "Login");
    EXPECT_FALSE(loader.Load());
    EXPECT_TRUE(log.Contains("Could not prepare statement LOGIN_SEL_BROKEN"));
    EXPECT_FALSE(first.IsOpen());
    EXPECT_FALSE(broken.IsOpen());
}

TEST(DatabaseLoaderTest, ReloadSwapsPoolsWithoutDroppingQueuedWork)
{
    std::optional<std::string> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Path() / "reload.conf";
    std::ofstream(file) << fmt::format("LoginDatabaseInfo = \"{}\"\nLoginDatabase.WorkerThreads = 1\n", *info);
    ConfigMgr config;
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());
    LoaderPool pool("login");
    DatabaseLoader loader(config);
    loader.AddDatabase(pool, "Login");
    ASSERT_TRUE(loader.Load());
    EXPECT_EQ(pool.GetAsyncConnectionCount(), 1u);

    AsyncCallbackProcessor<QueryCallback> processor;
    std::atomic<int> answered{ 0 };
    for (int i = 0; i < 40; ++i)
        processor.AddCallback(pool.AsyncQuery("SELECT SLEEP(0.01)").WithCallback([&answered](QueryResult result) { if (result) answered.fetch_add(1); }));

    std::atomic<bool> producing{ true };
    std::atomic<int> syncFailures{ 0 };
    std::mutex concurrentMutex;
    std::vector<QueryCallback> concurrent;
    std::thread producer([&]
    {
        while (producing)
        {
            QueryCallback callback = pool.AsyncQuery("SELECT 2").WithCallback([&answered](QueryResult result) { if (result) answered.fetch_add(1); });
            {
                std::lock_guard<std::mutex> lock(concurrentMutex);
                concurrent.push_back(std::move(callback));
            }
            if (!pool.Query("SELECT 3"))
                syncFailures.fetch_add(1);
        }
    });
    ScopeExit const stopProducer([&] { producing = false; if (producer.joinable()) producer.join(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::ofstream(file) << fmt::format("LoginDatabaseInfo = \"{}\"\nLoginDatabase.WorkerThreads = 3\nLoginDatabase.SynchThreads = 2\n", *info);
    ASSERT_TRUE(config.Reload().Succeeded());
    EXPECT_TRUE(loader.ApplyConfig());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    producing = false;
    producer.join();
    EXPECT_EQ(syncFailures.load(), 0);
    std::size_t concurrentCount = 0;
    {
        std::lock_guard<std::mutex> lock(concurrentMutex);
        concurrentCount = concurrent.size();
        for (QueryCallback& callback : concurrent)
            processor.AddCallback(std::move(callback));
    }
    EXPECT_GT(concurrentCount, 0u);
    EXPECT_EQ(pool.GetAsyncConnectionCount(), 3u);
    EXPECT_EQ(pool.GetSyncConnectionCount(), 2u);
    for (int i = 0; i < 40; ++i)
        processor.AddCallback(pool.AsyncQuery("SELECT 1").WithCallback([&answered](QueryResult result) { if (result) answered.fetch_add(1); }));
    ASSERT_TRUE(Pump(processor, std::chrono::seconds(60)));
    EXPECT_EQ(answered.load(), static_cast<int>(80 + concurrentCount));

    CapturedLog log;
    std::ofstream(file) << "LoginDatabaseInfo = \"127.0.0.1;1;nobody;nothing;nowhere\"\nLoginDatabase.WorkerThreads = 3\nLoginDatabase.SynchThreads = 2\n";
    ASSERT_TRUE(config.Reload().Succeeded());
    EXPECT_FALSE(loader.ApplyConfig());
    EXPECT_TRUE(log.Contains("keeps its current connections"));
    QueryResult const still = pool.Query("SELECT 5");
    ASSERT_TRUE(still);
    EXPECT_EQ((*still)[0].Get<int32>(), 5);
    loader.Close();
}
