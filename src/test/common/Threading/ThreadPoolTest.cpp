/*
 * Project Ambrose by Imjustchico
 * Tests that the thread pool runs every posted task on its named threads and joins cleanly.
 */

#include "ThreadName.h"
#include "ThreadPool.h"

#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <future>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>

TEST(ThreadPoolTest, RunsEveryPostedTask)
{
    std::atomic<int> counter{ 0 };
    {
        ThreadPool pool(4, "test");
        EXPECT_EQ(pool.GetThreadCount(), 4u);
        for (int i = 0; i < 10000; ++i)
            pool.Post([&counter] { ++counter; });
        pool.Join();
        EXPECT_EQ(counter.load(), 10000);
    }
    EXPECT_EQ(counter.load(), 10000);
}

TEST(ThreadPoolTest, TasksRunOnNamedPoolThreads)
{
    ThreadPool pool(3, "worker");
    std::mutex mutex;
    std::set<std::string> names;
    std::atomic<int> onPool{ 0 };
    for (int i = 0; i < 300; ++i)
    {
        pool.Post([&]
        {
            if (pool.IsPoolThread())
                ++onPool;
            std::lock_guard lock(mutex);
            names.insert(Ambrose::Threading::GetCurrentThreadName());
        });
    }
    pool.Join();
    EXPECT_EQ(onPool.load(), 300);
    EXPECT_FALSE(pool.IsPoolThread());
    ASSERT_FALSE(names.empty());
    for (std::string const& name : names)
        EXPECT_EQ(name.rfind("worker-", 0), 0u) << name;
}

TEST(ThreadPoolTest, JoinWaitsForRunningWork)
{
    ThreadPool pool(2, "join");
    std::promise<void> release;
    std::shared_future<void> gate = release.get_future().share();
    std::atomic<bool> finished{ false };
    pool.Post([gate, &finished]
    {
        gate.wait();
        finished = true;
    });
    std::future<void> joined = std::async(std::launch::async, [&pool] { pool.Join(); });
    EXPECT_EQ(joined.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);
    release.set_value();
    joined.get();
    EXPECT_TRUE(finished.load());
}

TEST(ThreadPoolTest, StopEndsWorkThatWouldRunForever)
{
    ThreadPool pool(2, "stop");
    std::atomic<int> ran{ 0 };
    std::function<void()> repost;
    repost = [&pool, &ran, &repost]
    {
        ++ran;
        pool.Post(repost);
    };
    pool.Post(repost);
    while (ran.load() < 1000)
        std::this_thread::yield();
    pool.Stop();
    std::future<void> joined = std::async(std::launch::async, [&pool] { pool.Join(); });
    ASSERT_EQ(joined.wait_for(std::chrono::seconds(30)), std::future_status::ready);
    int const afterJoin = ran.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(ran.load(), afterJoin);
}

TEST(ThreadPoolTest, ThrowingTaskDoesNotStopThePool)
{
    ThreadPool pool(1, "throw");
    std::promise<int> result;
    pool.Post([] { throw std::runtime_error("task failure"); });
    pool.Post([&result] { result.set_value(7); });
    std::future<int> value = result.get_future();
    ASSERT_EQ(value.wait_for(std::chrono::seconds(30)), std::future_status::ready);
    EXPECT_EQ(value.get(), 7);
}

TEST(ThreadPoolTest, ConcurrentJoinsAreSafe)
{
    ThreadPool pool(2, "joins");
    std::atomic<int> ran{ 0 };
    for (int i = 0; i < 100; ++i)
        pool.Post([&ran] { ++ran; });
    std::future<void> first = std::async(std::launch::async, [&pool] { pool.Join(); });
    std::future<void> second = std::async(std::launch::async, [&pool] { pool.Join(); });
    first.get();
    second.get();
    EXPECT_EQ(ran.load(), 100);
}

TEST(ThreadPoolTest, ExecutorPostsIntoThePool)
{
    ThreadPool pool(2, "exec");
    std::promise<bool> result;
    asio::post(pool.GetExecutor(), [&] { result.set_value(pool.IsPoolThread()); });
    EXPECT_TRUE(result.get_future().get());
}
