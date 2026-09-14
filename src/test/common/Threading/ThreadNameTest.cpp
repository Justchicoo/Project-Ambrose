/*
 * Project Ambrose by Imjustchico
 * Tests setting and reading thread names, including truncation to the portable 15-byte limit.
 */

#include "ThreadName.h"

#include <gtest/gtest.h>

#include <string>
#include <thread>

TEST(ThreadNameTest, SetNameIsReadBackOnTheSameThread)
{
    bool set = false;
    std::string name;
    std::thread([&]
    {
        set = Ambrose::Threading::SetCurrentThreadName("ambrose-test");
        name = Ambrose::Threading::GetCurrentThreadName();
    }).join();
    EXPECT_TRUE(set);
    EXPECT_EQ(name, "ambrose-test");
}

TEST(ThreadNameTest, LongNamesAreTruncatedTo15Bytes)
{
    std::string name;
    std::thread([&]
    {
        Ambrose::Threading::SetCurrentThreadName("a-very-long-thread-name-indeed");
        name = Ambrose::Threading::GetCurrentThreadName();
    }).join();
    EXPECT_EQ(name, "a-very-long-thr");
}

TEST(ThreadNameTest, TruncationNeverSplitsAUtf8Character)
{
    std::string name;
    std::thread([&]
    {
        Ambrose::Threading::SetCurrentThreadName("fourteen-bytes\xC3\xA9");
        name = Ambrose::Threading::GetCurrentThreadName();
    }).join();
    EXPECT_EQ(name, "fourteen-bytes");
}
