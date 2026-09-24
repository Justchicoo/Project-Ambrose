/*
 * Project Ambrose by Imjustchico
 * Tests what a command is allowed to be and who is allowed to run it: a line is split into the deepest command that matches and the arguments left over, an account below a command's level is told there is no such command and the command does not run, a command_security row raising a level refuses somebody who could run it before, a group named on its own lists only what its caller may see, a console and a chat line each reach only the commands offered to them, the prefix a client types is taken off before the words are read while a console that types none is still understood, a command marked sensitive is described for the log without its arguments while an ordinary one keeps them, and the commands the scripts actually ship are found by the names an operator would type.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "CommandMgr.h"
#include "ScriptLoader.h"
#include "ScriptMgr.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace
{
    std::vector<std::string> Ran;

    ChatCommand::Handler Records(std::string name)
    {
        return [name = std::move(name)](CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            std::string line = name;
            for (std::string const& argument : arguments)
                line += " [" + argument + "]";
            Ran.push_back(line);
            caller.Reply(line);
            return true;
        };
    }

    class CommandMgrTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            Ran.clear();
            sCommandMgr.Clear();
            sCommandMgr.Load({
                { .Name = "character", .SecurityLevel = SEC_GAMEMASTER, .Help = "change a wizard", .Children = {
                    { .Name = "gold", .SecurityLevel = SEC_GAMEMASTER, .Help = "set gold", .Run = Records("character gold") },
                    { .Name = "level", .SecurityLevel = SEC_ADMINISTRATOR, .Help = "set level", .Run = Records("character level") },
                } },
                { .Name = "server", .SecurityLevel = SEC_PLAYER, .Help = "about this server", .Children = {
                    { .Name = "info", .SecurityLevel = SEC_PLAYER, .Help = "what is running", .Run = Records("server info") },
                } },
                { .Name = "gm", .SecurityLevel = SEC_GAMEMASTER, .AvailableOnConsole = false, .Help = "act as a game master", .Children = {
                    { .Name = "on", .SecurityLevel = SEC_GAMEMASTER, .AvailableOnConsole = false, .Help = "act as one", .Run = Records("gm on") },
                } },
                { .Name = "secret", .SecurityLevel = SEC_ADMINISTRATOR, .Help = "something with a secret in it", .Sensitive = true, .Run = Records("secret") },
            });
        }

        void TearDown() override
        {
            sCommandMgr.Clear();
            Ran.clear();
        }
    };
}

TEST_F(CommandMgrTest, ACommandIsTheDeepestNameThatMatchesAndTheRestAreArguments)
{
    CommandMatch const match = sCommandMgr.Parse(".character gold 500");
    EXPECT_TRUE(match.Found);
    EXPECT_EQ(match.Name, "character gold");
    EXPECT_EQ(match.Arguments, (std::vector<std::string>{ "500" }));
    EXPECT_EQ(match.SecurityLevel, SEC_GAMEMASTER);

    CommandMatch const group = sCommandMgr.Parse("character");
    EXPECT_TRUE(group.Found);
    EXPECT_EQ(group.Name, "character");
    EXPECT_TRUE(group.Arguments.empty());

    EXPECT_FALSE(sCommandMgr.Parse("nonsense").Found);
    EXPECT_FALSE(sCommandMgr.Parse("").Found);
}

TEST_F(CommandMgrTest, SplittingKeepsAQuotedWordWhole)
{
    EXPECT_EQ(CommandMgr::Split("character gold 500"), (std::vector<std::string>{ "character", "gold", "500" }));
    EXPECT_EQ(CommandMgr::Split("  spaced   out  "), (std::vector<std::string>{ "spaced", "out" }));
    EXPECT_EQ(CommandMgr::Split("say \"two words\" once"), (std::vector<std::string>{ "say", "two words", "once" }));
    EXPECT_EQ(CommandMgr::Split("empty \"\" kept"), (std::vector<std::string>{ "empty", "", "kept" }));
    EXPECT_TRUE(CommandMgr::Split("   ").empty());
}

TEST_F(CommandMgrTest, AnAccountBelowACommandsLevelIsToldThereIsNoSuchCommand)
{
    RecordingCaller player(SEC_PLAYER, false);
    EXPECT_EQ(sCommandMgr.Execute(player, "character gold 500"), CommandResult::Unknown);
    EXPECT_EQ(player.GetLines(), (std::vector<std::string>{ "There is no such command" }));
    EXPECT_TRUE(Ran.empty()) << "a command ran for an account that may not run it";

    RecordingCaller master(SEC_GAMEMASTER, false);
    EXPECT_EQ(sCommandMgr.Execute(master, "character gold 500"), CommandResult::Ran);
    EXPECT_EQ(Ran, (std::vector<std::string>{ "character gold [500]" }));
}

TEST_F(CommandMgrTest, ARefusalReadsExactlyLikeACommandThatDoesNotExist)
{
    RecordingCaller player(SEC_PLAYER, false);
    EXPECT_EQ(sCommandMgr.Execute(player, "character gold 500"), CommandResult::Unknown);
    std::vector<std::string> const refused = player.GetLines();

    player.Clear();
    EXPECT_EQ(sCommandMgr.Execute(player, "nonsense here"), CommandResult::Unknown);
    EXPECT_EQ(player.GetLines(), refused) << "a refusal told the caller the command exists";
}

TEST_F(CommandMgrTest, ACommandSecurityRowOverridesTheLevelTheScriptGave)
{
    RecordingCaller master(SEC_GAMEMASTER, false);
    ASSERT_EQ(sCommandMgr.Execute(master, "character gold 1"), CommandResult::Ran);

    sCommandMgr.SetOverrides({ { "character gold", SEC_ADMINISTRATOR } });
    Ran.clear();
    master.Clear();
    EXPECT_EQ(sCommandMgr.Execute(master, "character gold 1"), CommandResult::Unknown);
    EXPECT_TRUE(Ran.empty());

    RecordingCaller administrator(SEC_ADMINISTRATOR, false);
    EXPECT_EQ(sCommandMgr.Execute(administrator, "character gold 1"), CommandResult::Ran);
    EXPECT_EQ(Ran, (std::vector<std::string>{ "character gold [1]" }));

    sCommandMgr.SetOverrides({ { "character gold", SEC_PLAYER } });
    RecordingCaller player(SEC_PLAYER, false);
    Ran.clear();
    EXPECT_EQ(sCommandMgr.Execute(player, "character gold 1"), CommandResult::Ran) << "lowering a level did not open the command";
}

TEST_F(CommandMgrTest, AChildIsNeverEasierToReachThanItsGroup)
{
    sCommandMgr.SetOverrides({ { "character", SEC_ADMINISTRATOR } });
    RecordingCaller master(SEC_GAMEMASTER, false);
    EXPECT_EQ(sCommandMgr.Execute(master, "character gold 1"), CommandResult::Unknown)
        << "raising a group left a command under it reachable";
}

TEST_F(CommandMgrTest, AGroupOnItsOwnListsOnlyWhatItsCallerMaySee)
{
    RecordingCaller master(SEC_GAMEMASTER, false);
    EXPECT_EQ(sCommandMgr.Execute(master, "character"), CommandResult::Usage);
    ASSERT_EQ(master.GetLines().size(), 1u) << "a game master was shown a command they cannot run";
    EXPECT_NE(master.GetLines().front().find("character gold"), std::string::npos);

    RecordingCaller administrator(SEC_ADMINISTRATOR, false);
    EXPECT_EQ(sCommandMgr.Execute(administrator, "character"), CommandResult::Usage);
    EXPECT_EQ(administrator.GetLines().size(), 2u);
}

TEST_F(CommandMgrTest, AConsoleAndAChatLineReachOnlyWhatIsOfferedToThem)
{
    RecordingCaller console(SEC_CONSOLE, true);
    EXPECT_EQ(sCommandMgr.Execute(console, "gm on"), CommandResult::Unknown) << "a console reached a command only a character can run";

    RecordingCaller master(SEC_GAMEMASTER, false);
    EXPECT_EQ(sCommandMgr.Execute(master, "gm on"), CommandResult::Ran);

    std::vector<std::string> const forConsole = sCommandMgr.Describe(SEC_CONSOLE, true);
    EXPECT_TRUE(std::none_of(forConsole.begin(), forConsole.end(), [](std::string const& line) { return line.rfind("gm ", 0) == 0; }));
    std::vector<std::string> const inGame = sCommandMgr.Describe(SEC_GAMEMASTER, false);
    EXPECT_TRUE(std::any_of(inGame.begin(), inGame.end(), [](std::string const& line) { return line.rfind("gm on", 0) == 0; }));
}

TEST_F(CommandMgrTest, ALineLongerThanACommandMayBeIsRefusedBeforeItIsLookedUp)
{
    RecordingCaller console(SEC_CONSOLE, true);
    std::string const tooLong(CommandMgr::MaxCommandBytes + 1, 'a');
    EXPECT_EQ(sCommandMgr.Execute(console, tooLong), CommandResult::Refused);
    ASSERT_EQ(console.GetLines().size(), 1u);
    EXPECT_NE(console.GetLines().front().find("longer than"), std::string::npos);
    EXPECT_TRUE(Ran.empty());
}

TEST_F(CommandMgrTest, TheCommandsTheScriptsShipAreFoundByTheNamesAnOperatorTypes)
{
    sScriptMgr.Unload();
    sScriptMgr.LoadScripts(&AddScripts);
    sCommandMgr.Clear();
    sCommandMgr.Load(sScriptMgr.GetCommands());

    EXPECT_GT(sCommandMgr.GetCommandCount(), 0u);
    EXPECT_TRUE(sCommandMgr.Parse("server info").Found);
    EXPECT_TRUE(sCommandMgr.Parse("character gold 500").Found);
    EXPECT_EQ(sCommandMgr.Parse("character gold 500").Name, "character gold");
    EXPECT_TRUE(sCommandMgr.Parse("gm on").Found);

    RecordingCaller console(SEC_CONSOLE, true);
    EXPECT_EQ(sCommandMgr.Execute(console, "server info"), CommandResult::Ran);
    EXPECT_FALSE(console.GetLines().empty());
    EXPECT_NE(console.GetLines().front().find("Project Ambrose"), std::string::npos);

    sScriptMgr.Unload();
}

TEST_F(CommandMgrTest, TheClientsPrefixIsTakenOffBeforeTheWordsAreRead)
{
    EXPECT_EQ(sCommandMgr.GetPrefix(), std::string(CommandMgr::DefaultPrefix));

    EXPECT_TRUE(sCommandMgr.Parse(".character gold 500").Found);
    EXPECT_EQ(sCommandMgr.Parse(".character gold 500").Name, "character gold");
    EXPECT_TRUE(sCommandMgr.Parse("character gold 500").Found) << "a console types no prefix and must still be understood";
    EXPECT_TRUE(sCommandMgr.Parse("  .character gold 500 ").Found);

    RecordingCaller master(SEC_GAMEMASTER, false);
    EXPECT_EQ(sCommandMgr.Execute(master, ".character gold 500"), CommandResult::Ran);
    EXPECT_EQ(Ran, (std::vector<std::string>{ "character gold [500]" }));

    sCommandMgr.SetPrefix("!");
    EXPECT_TRUE(sCommandMgr.Parse("!character gold 1").Found);
    EXPECT_FALSE(sCommandMgr.Parse(".character gold 1").Found) << "the old prefix still worked after it was changed";
    EXPECT_EQ(sCommandMgr.Execute(master, "."), CommandResult::Unknown);
    sCommandMgr.SetPrefix(std::string(CommandMgr::DefaultPrefix));
}

TEST_F(CommandMgrTest, ACommandMarkedSensitiveIsDescribedWithoutItsArguments)
{
    EXPECT_EQ(sCommandMgr.DescribeForLog("secret hunter2"), "secret ***");
    EXPECT_EQ(sCommandMgr.DescribeForLog(".secret hunter2 again"), "secret ***");
    EXPECT_EQ(sCommandMgr.DescribeForLog("secret"), "secret");

    EXPECT_EQ(sCommandMgr.DescribeForLog("character gold 500"), "character gold 500");
    EXPECT_EQ(sCommandMgr.DescribeForLog(".character gold 500"), "character gold 500");
    EXPECT_EQ(sCommandMgr.DescribeForLog("nonsense here"), "nonsense here");

    EXPECT_TRUE(sCommandMgr.GetLogging());
    sCommandMgr.SetLogging(false);
    EXPECT_FALSE(sCommandMgr.GetLogging());
    sCommandMgr.SetLogging(true);
}
