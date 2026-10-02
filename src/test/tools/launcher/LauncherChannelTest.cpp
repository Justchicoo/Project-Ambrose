/*
 * Project Ambrose by Imjustchico
 * Checks that the window can ask the launcher for nothing the terminal cannot ask for, and gets back the same answer: a message naming the same values the console options name builds the same plan the console builds, right down to the command, so no decision can drift into the window, the window size it shows is the one the client is started at, and the install it names is written as the console writes it; a message that leaves a field out leaves it for the launcher and its configuration to decide rather than filling it in a second time; a field of the wrong shape is refused by name instead of being coerced; a refusal names its reason, because a window that says only that something failed sends its user to a log file they do not have, and a password is hidden before the window is told anything, since the command is drawn on a screen that can be photographed while the client still has to be started with the real one; every plan and refusal carries the launch state it was answered in, and the client's log folder is answered with its path or with why there is none yet, and handed to the system as a file URL in which nothing a folder name holds can change what is opened.
 */

#include "ClientLocator.h"
#include "ConfigMgr.h"
#include "LauncherChannel.h"
#include "LauncherHarness.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

TEST(LauncherChannelTest, AMessageAndTheConsoleOptionsBuildTheSamePlan)
{
    LauncherHarness harness;
    harness.AddInstall();

    LauncherRequest typed;
    typed.Host = "10.0.0.4";
    typed.Port = "12010";
    typed.Locale = "de-DE";
    typed.Window = "1600x900";
    typed.Character = "Wolf";

    LauncherRequest sent;
    std::string error;
    ASSERT_TRUE(LauncherChannel::ReadRequest(
        R"({"host":"10.0.0.4","port":"12010","locale":"de-DE","window":"1600x900","character":"Wolf"})", sent, error))
        << error;

    std::optional<LauncherPlan> const fromConsole = harness.Prepare(typed);
    ASSERT_TRUE(fromConsole) << harness.Error;
    std::optional<LauncherPlan> const fromWindow = harness.Prepare(sent);
    ASSERT_TRUE(fromWindow) << harness.Error;

    EXPECT_EQ(fromWindow->Command(), fromConsole->Command())
        << "the window and the terminal reach one Prepare, so the command they would run is one command";
    EXPECT_EQ(fromWindow->Arguments, fromConsole->Arguments);
    EXPECT_EQ(fromWindow->Host, fromConsole->Host);
    EXPECT_EQ(fromWindow->Port, fromConsole->Port);
    EXPECT_EQ(fromWindow->Locale, fromConsole->Locale);
    EXPECT_EQ(fromWindow->RunFolder, fromConsole->RunFolder);
    EXPECT_EQ(fromWindow->Program, fromConsole->Program);
    EXPECT_EQ(nlohmann::json::parse(LauncherChannel::DescribePlan(*fromWindow))["window"], "1600x900")
        << "the window shows the size the client is started at, the one it asked for";
}

TEST(LauncherChannelTest, AFieldTheMessageLeavesOutIsLeftForTheLauncherToDecide)
{
    LauncherRequest sent;
    std::string error;
    ASSERT_TRUE(LauncherChannel::ReadRequest(R"({"host":"10.0.0.4"})", sent, error)) << error;

    EXPECT_TRUE(sent.Host.has_value());
    EXPECT_FALSE(sent.Port.has_value()) << "a window that did not say which port must not be read as having said the default";
    EXPECT_FALSE(sent.Locale.has_value());
    EXPECT_FALSE(sent.ClientDir.has_value());
    EXPECT_FALSE(sent.User.has_value());

    LauncherHarness harness;
    harness.AddInstall();
    std::optional<LauncherPlan> const plan = harness.Prepare(sent);
    ASSERT_TRUE(plan) << harness.Error;
    EXPECT_EQ(plan->Port, 12000) << "the launcher's own default is what fills a field the window left alone";
    EXPECT_EQ(plan->Locale, "en-US");
}

TEST(LauncherChannelTest, AFieldOfTheWrongShapeIsRefusedByName)
{
    LauncherRequest sent;
    std::string error;

    EXPECT_FALSE(LauncherChannel::ReadRequest(R"({"port":12010})", sent, error));
    EXPECT_NE(error.find("port"), std::string::npos) << error;
    EXPECT_NE(error.find("text"), std::string::npos) << error;

    EXPECT_FALSE(LauncherChannel::ReadRequest(R"({"user":"me"})", sent, error));
    EXPECT_NE(error.find("user"), std::string::npos) << error;

    EXPECT_FALSE(LauncherChannel::ReadRequest("not a message at all", sent, error));
    EXPECT_FALSE(error.empty());
}

TEST(LauncherChannelTest, TheAnswerDescribesWhatWouldHappenOrWhyItWouldNot)
{
    LauncherHarness harness;
    harness.AddInstall();
    std::optional<LauncherPlan> const plan = harness.Prepare(LauncherRequest{});
    ASSERT_TRUE(plan) << harness.Error;

    nlohmann::json const described = nlohmann::json::parse(LauncherChannel::DescribePlan(*plan));
    EXPECT_EQ(described["schema"], LauncherChannel::SchemaVersion);
    EXPECT_TRUE(described["ready"]);
    EXPECT_EQ(described["revision"], LauncherTestData::Revision);
    EXPECT_EQ(described["host"], "127.0.0.1");
    EXPECT_EQ(described["port"], 12000);
    EXPECT_EQ(described["window"], "1280x720") << "the window shows the size the launcher settled when nothing asked for one";
    EXPECT_FALSE(described["command"].get<std::string>().empty()) << "the window shows what will be run, not a shape only this program reads";
    EXPECT_EQ(described["install"], ClientLocator::PathText(plan->Install.Root))
        << "the window names the install exactly as the console's install line does, so what one shows the other can be checked against";
    EXPECT_FALSE(described["run_folder"].get<std::string>().empty());
    EXPECT_EQ(described["state"], "play") << "a plan answered with nothing else said is one the window can play";
    EXPECT_EQ(nlohmann::json::parse(LauncherChannel::DescribePlan(*plan, "launching"))["state"], "launching");

    nlohmann::json const refused = nlohmann::json::parse(LauncherChannel::DescribeRefusal("no install was found"));
    EXPECT_FALSE(refused["ready"]);
    EXPECT_EQ(refused["reason"], "no install was found") << "a refusal carries the reason, so the window can say what to do about it";
    EXPECT_EQ(refused["state"], "retry");
    EXPECT_EQ(nlohmann::json::parse(LauncherChannel::DescribeRefusal("no install was found", "locate"))["state"], "locate");
}

TEST(LauncherChannelTest, TheLogFolderIsTheLaunchersOwnAndItsAddressCannotBeBent)
{
    nlohmann::json const opened = nlohmann::json::parse(LauncherChannel::DescribeFolder(ConfigMgr::PathFromUtf8("C:/Users/wiz/AppData/Local/ProjectAmbrose/client/r1")));
    EXPECT_TRUE(opened["opened"]);
    EXPECT_EQ(opened["folder"], "C:/Users/wiz/AppData/Local/ProjectAmbrose/client/r1");

    nlohmann::json const none = nlohmann::json::parse(LauncherChannel::DescribeFolder(std::nullopt));
    EXPECT_FALSE(none["opened"]);
    EXPECT_FALSE(none["reason"].get<std::string>().empty()) << "a folder that is not there yet says why";

    EXPECT_EQ(LauncherChannel::FolderAddress(ConfigMgr::PathFromUtf8("C:/Users/wiz/Ambrose client/r1")), "file:///C:/Users/wiz/Ambrose%20client/r1");
    EXPECT_EQ(LauncherChannel::FolderAddress(ConfigMgr::PathFromUtf8("/home/wiz/a#b?c")), "file:///home/wiz/a%23b%3Fc")
        << "a hash or a question mark in a folder name is part of the name, never a fragment or a query";
}

TEST(LauncherChannelTest, AnAccountArrivesWholeOrIsRefused)
{
    LauncherRequest sent;
    std::string error;
    ASSERT_TRUE(LauncherChannel::ReadRequest(R"({"user":{"user_id":"7","key":"a-key","name":"Wolf"}})", sent, error)) << error;
    ASSERT_TRUE(sent.User.has_value());
    EXPECT_EQ(sent.User->UserId, "7");
    EXPECT_EQ(sent.User->Key, "a-key");
    EXPECT_EQ(sent.User->Name, "Wolf");

    LauncherRequest second;
    EXPECT_FALSE(LauncherChannel::ReadRequest(R"({"user":{"user_id":7}})", second, error));
    EXPECT_NE(error.find("user_id"), std::string::npos) << error;
}

TEST(LauncherChannelTest, APasswordNeverReachesTheWindow)
{
    LauncherHarness harness;
    harness.AddInstall();

    LauncherRequest request;
    ClientLogin login;
    login.UserId = "tester";
    login.Key = "a-real-password";
    login.Name = "tester";
    request.User = login;

    std::optional<LauncherPlan> const plan = harness.Prepare(request);
    ASSERT_TRUE(plan) << harness.Error;
    ASSERT_NE(plan->Command().find("a-real-password"), std::string::npos)
        << "the command the client is actually started with carries the password, which is the whole point of hiding it from the window";

    std::string const described = LauncherChannel::DescribePlan(*plan);
    EXPECT_EQ(described.find("a-real-password"), std::string::npos)
        << "a screen anybody can see, or photograph, must never carry a password: " << described;

    nlohmann::json const body = nlohmann::json::parse(described);
    EXPECT_NE(body["command"].get<std::string>().find(LauncherChannel::Hidden), std::string::npos)
        << "and the window is shown that something was hidden rather than a command that looks complete";
    EXPECT_NE(body["command"].get<std::string>().find("tester"), std::string::npos)
        << "the account name is not a secret and stays readable";

    bool hidden = false;
    for (auto const& argument : body["arguments"])
    {
        EXPECT_NE(argument.get<std::string>(), "a-real-password");
        if (argument.get<std::string>() == LauncherChannel::Hidden)
            hidden = true;
    }
    EXPECT_TRUE(hidden);
}
