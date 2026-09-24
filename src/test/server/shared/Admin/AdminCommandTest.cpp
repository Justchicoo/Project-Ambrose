/*
 * Project Ambrose by Imjustchico
 * Tests the route that runs a command for an operator: a command runs and its output comes back with the request it belongs to, a command that changes something irreversibly is refused until it is confirmed on purpose, a caller asking to run below the console level is told there is no such command and nothing runs, a failing command says so rather than pretending, a body that is too long or carries a key this app does not take is refused before anything runs, and an argument of a command marked sensitive reaches neither the record nor the answer.
 */

#include "AdminAuth.h"
#include "AdminCommand.h"
#include "AdminRouter.h"
#include "ConsoleCommandTable.h"
#include "LogTestDirectory.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>

#include <string>
#include <vector>

namespace
{
    void Fill(ConsoleCommandTable& table)
    {
        table.Register({ "ping", "", "answers once", false,
            [](std::vector<std::string> const&, ConsoleCommandTable::Reply const& reply) { reply("pong"); return true; } });
        table.Register({ "account create", "<name> <password>", "makes an account", true,
            [](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
            {
                reply("made " + (arguments.empty() ? std::string("nobody") : arguments.front()));
                return true;
            } });
        table.Register({ "shutdown", "", "stops the server", false,
            [](std::vector<std::string> const&, ConsoleCommandTable::Reply const& reply) { reply("stopping"); return true; } });
        table.Register({ "fails", "", "never works", false,
            [](std::vector<std::string> const&, ConsoleCommandTable::Reply const& reply) { reply("it did not work"); return false; } });
    }
}

TEST(AdminCommandTest, ACommandRunsAndItsOutputComesBack)
{
    ConsoleCommandTable table;
    Fill(table);
    AdminCommandOutcome const outcome = AdminCommand::RunThroughTable(table, "ping", AdminCommand::ConsoleLevel, false);

    EXPECT_TRUE(outcome.Ran);
    EXPECT_FALSE(outcome.Refused);
    ASSERT_EQ(outcome.Lines.size(), 1u);
    EXPECT_EQ(outcome.Lines.front(), "pong");
}

TEST(AdminCommandTest, SomethingThatCannotBeUndoneWaitsForAConfirmation)
{
    ConsoleCommandTable table;
    Fill(table);
    EXPECT_TRUE(AdminCommand::IsDestructive("shutdown"));
    EXPECT_FALSE(AdminCommand::IsDestructive("ping"));

    AdminCommandOutcome const refused = AdminCommand::RunThroughTable(table, "shutdown", AdminCommand::ConsoleLevel, false);
    EXPECT_FALSE(refused.Ran);
    EXPECT_TRUE(refused.Refused);
    EXPECT_NE(refused.Reason.find("confirm"), std::string::npos) << refused.Reason;
    EXPECT_TRUE(refused.Lines.empty()) << "nothing ran, so nothing was said";

    AdminCommandOutcome const confirmed = AdminCommand::RunThroughTable(table, "shutdown", AdminCommand::ConsoleLevel, true);
    EXPECT_TRUE(confirmed.Ran);
}

TEST(AdminCommandTest, BelowTheConsoleLevelThereIsNoSuchCommand)
{
    ConsoleCommandTable table;
    Fill(table);
    AdminCommandOutcome const outcome = AdminCommand::RunThroughTable(table, "ping", 1, false);

    EXPECT_FALSE(outcome.Ran);
    EXPECT_TRUE(outcome.Refused);
    EXPECT_EQ(outcome.Reason, "there is no such command") << "a caller below the level is told the same as one asking for nothing";
    EXPECT_TRUE(outcome.Lines.empty());
}

TEST(AdminCommandTest, AFailingCommandSaysSoAndAnUnknownOneIsNotPretendedTo)
{
    ConsoleCommandTable table;
    Fill(table);
    AdminCommandOutcome const failed = AdminCommand::RunThroughTable(table, "fails", AdminCommand::ConsoleLevel, false);
    EXPECT_FALSE(failed.Ran);
    EXPECT_TRUE(failed.Refused);
    ASSERT_FALSE(failed.Lines.empty());
    EXPECT_EQ(failed.Lines.front(), "it did not work") << "what it said is kept even though it failed";

    AdminCommandOutcome const unknown = AdminCommand::RunThroughTable(table, "nosuchthing", AdminCommand::ConsoleLevel, false);
    EXPECT_TRUE(unknown.Refused);
    EXPECT_EQ(unknown.Reason, "there is no such command");
}

TEST(AdminCommandTest, ASensitiveArgumentIsNotInWhatGetsWrittenDown)
{
    ConsoleCommandTable table;
    Fill(table);
    std::string const written = table.DescribeForLog("account create tester hunter2");

    EXPECT_EQ(written.find("hunter2"), std::string::npos) << written;
    EXPECT_NE(written.find("account create"), std::string::npos) << written;
}

namespace
{
    constexpr char const* Token = "0123456789abcdef0123456789abcdef";

    AdminRequest Post(std::string body)
    {
        AdminRequest request;
        request.Method = "POST";
        request.Path = "/api/command";
        request.RemoteAddress = "127.0.0.1";
        request.Id = "req-1";
        request.Authorization = std::string("Bearer ") + Token;
        request.Body = std::move(body);
        return request;
    }

    std::string ReadAll(std::filesystem::path const& file)
    {
        std::ifstream in(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
}

TEST(AdminCommandRouteTest, ARunIsAnsweredAndWrittenDownAndASecretIsInNeither)
{
    LogTestDirectory directory;
    std::filesystem::path const audit = directory.Path() / "audit" / "commands.jsonl";
    ConsoleCommandTable table;
    Fill(table);
    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter router(auth);
    AdminCommand::Register(router, table, "gameserver", audit);

    AdminResponse const answer = router.Dispatch(Post(R"({"command":"account create tester hunter2"})"));
    EXPECT_EQ(answer.Status, 200) << answer.Body;
    nlohmann::json const body = nlohmann::json::parse(answer.Body, nullptr, false);
    ASSERT_TRUE(body.is_object()) << answer.Body;
    EXPECT_TRUE(body["success"].get<bool>());
    EXPECT_EQ(body["request_id"], "req-1");
    EXPECT_EQ(answer.Body.find("hunter2"), std::string::npos) << "the answer carries no secret";

    std::string const written = ReadAll(audit);
    EXPECT_NE(written.find("account create"), std::string::npos) << written;
    EXPECT_EQ(written.find("hunter2"), std::string::npos) << "the record carries no secret either";

    nlohmann::json const row = nlohmann::json::parse(written, nullptr, false);
    ASSERT_TRUE(row.is_object()) << written;
    EXPECT_FALSE(row["who"].get<std::string>().empty()) << "the caller the listener decided on, never the one the request claimed";
    EXPECT_EQ(row["address"], "127.0.0.1");
    EXPECT_EQ(row["app"], "gameserver");
    EXPECT_TRUE(row["ran"].get<bool>());
    for (std::string const& field : AdminCommand::Fields())
        EXPECT_TRUE(row.contains(field)) << "the record declares " << field << " and does not write it";
}

TEST(AdminCommandRouteTest, ABodyTooLongOrCarryingAnUnknownKeyRunsNothing)
{
    LogTestDirectory directory;
    std::filesystem::path const audit = directory.Path() / "audit" / "commands.jsonl";
    ConsoleCommandTable table;
    Fill(table);
    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter router(auth);
    AdminCommand::Register(router, table, "gameserver", audit);

    EXPECT_EQ(router.Dispatch(Post(R"({"command":"ping","sneaky":true})")).Status, 422);
    EXPECT_EQ(router.Dispatch(Post("{\"command\":\"ping " + std::string(AdminCommand::MaxCommandBytes, 'x') + "\"}")).Status, 422);
    EXPECT_EQ(router.Dispatch(Post(R"({"command":"   "})")).Status, 422);
    EXPECT_FALSE(std::filesystem::exists(audit)) << "nothing ran, so nothing was written down";
}

TEST(AdminCommandRouteTest, ARefusalIsAnsweredAndRecordedWithItsReason)
{
    LogTestDirectory directory;
    std::filesystem::path const audit = directory.Path() / "audit" / "commands.jsonl";
    ConsoleCommandTable table;
    Fill(table);
    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter router(auth);
    AdminCommand::Register(router, table, "gameserver", audit);

    AdminResponse const refused = router.Dispatch(Post(R"({"command":"ping","level":1})"));
    EXPECT_EQ(refused.Status, 409) << refused.Body;
    nlohmann::json const body = nlohmann::json::parse(refused.Body, nullptr, false);
    EXPECT_FALSE(body["success"].get<bool>());
    EXPECT_EQ(body["reason"], "there is no such command");

    std::string const written = ReadAll(audit);
    EXPECT_NE(written.find("\"refused\":true"), std::string::npos) << written;
    EXPECT_NE(written.find("there is no such command"), std::string::npos) << written;
}
