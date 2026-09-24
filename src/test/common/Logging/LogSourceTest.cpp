/*
 * Project Ambrose by Imjustchico
 * Tests that a record knows where in the code it was written: a logging call carries its file, line and function, the file is the path inside the repository rather than the folder this machine happens to build in, two calls on different lines are told apart while one call site repeated is not, a line written as text carries no location rather than a wrong one, a logging call inside another macro is still one argument rather than several, which is what the source macro's own commas would otherwise make of it, and a file reads the same whichever separator the compiler handed over.
 */

#include "Log.h"
#include "LogTestHarness.h"

#include <gtest/gtest.h>

#include <vector>

namespace
{
    class SLogSourceTest : public testing::Test
    {
    protected:
        void SetUp() override { _harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 3,Capture\n"); }

        std::vector<LogMessage> Written() const { return _harness.SharedStore()->Messages("Capture"); }

        LogTestHarness _harness;
    };

    TEST_F(SLogSourceTest, ARecordCarriesTheFileLineAndFunctionOfItsCall)
    {
        uint32 const line = static_cast<uint32>(__LINE__) + 1;
        AMBROSE_LOG(_harness.GetLog(), LogLevel::Info, "server.test", "a line with a source");

        std::vector<LogMessage> const messages = Written();
        ASSERT_EQ(messages.size(), 1u);
        ASSERT_TRUE(messages[0].Source.Known());
        EXPECT_EQ(messages[0].Source.Line, line);
        EXPECT_EQ(messages[0].Source.Function, std::string_view("TestBody"));
        EXPECT_NE(messages[0].Source.File.find("LogSourceTest.cpp"), std::string_view::npos);
    }

    TEST_F(SLogSourceTest, TheFileIsThePathInsideTheRepository)
    {
        AMBROSE_LOG(_harness.GetLog(), LogLevel::Info, "server.test", "a line with a source");

        std::vector<LogMessage> const messages = Written();
        ASSERT_EQ(messages.size(), 1u);
        std::string const file = LogSourcePath::Portable(messages[0].Source.File);
        EXPECT_EQ(file, "src/test/common/Logging/LogSourceTest.cpp");
        EXPECT_EQ(file.find(':'), std::string::npos);
        EXPECT_NE(file.front(), '/');
    }

    TEST_F(SLogSourceTest, TwoLinesAreToldApartAndOneLineRepeatedIsNot)
    {
        for (int repeat = 0; repeat < 2; ++repeat)
            AMBROSE_LOG(_harness.GetLog(), LogLevel::Info, "server.test", "the same call site twice");
        AMBROSE_LOG(_harness.GetLog(), LogLevel::Info, "server.test", "a different call site");

        std::vector<LogMessage> const messages = Written();
        ASSERT_EQ(messages.size(), 3u);
        EXPECT_EQ(messages[0].Source.Line, messages[1].Source.Line);
        EXPECT_NE(messages[0].Source.Line, messages[2].Source.Line);
        EXPECT_EQ(messages[0].Source.File, messages[2].Source.File);
    }

    TEST_F(SLogSourceTest, ALineWrittenAsTextCarriesNoLocation)
    {
        _harness.GetLog().WriteText("server.test", LogLevel::Info, "captured from somewhere else");

        std::vector<LogMessage> const messages = Written();
        ASSERT_EQ(messages.size(), 1u);
        EXPECT_FALSE(messages[0].Source.Known());
        EXPECT_TRUE(messages[0].Source.File.empty());
        EXPECT_EQ(messages[0].Source.Line, 0u);
    }

    TEST_F(SLogSourceTest, ALoggingCallStillReadsAsOneArgumentInsideAnotherMacro)
    {
        EXPECT_NO_THROW(AMBROSE_LOG(_harness.GetLog(), LogLevel::Info, "server.test", "inside another macro"));

        std::vector<LogMessage> const messages = Written();
        ASSERT_EQ(messages.size(), 1u);
        EXPECT_TRUE(messages[0].Source.Known());
    }

    TEST(LogSourcePathTest, AFileReadsTheSameWhicheverSeparatorTheCompilerHandedOver)
    {
        EXPECT_EQ(LogSourcePath::Portable("src\\common\\Logging\\Log.cpp"), "src/common/Logging/Log.cpp");
        EXPECT_EQ(LogSourcePath::Portable("src/common/Logging/Log.cpp"), "src/common/Logging/Log.cpp");
        EXPECT_EQ(LogSourcePath::Portable(""), "");
    }
}
