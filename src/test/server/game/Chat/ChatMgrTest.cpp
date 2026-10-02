/*
 * Project Ambrose by Imjustchico
 * Tests the chat rules: a line shows under the chat level the speaker's permissions give it, open chat before menu chat and neither as 0, the default permissions open chat; a line is a command when it starts with the command prefix, whatever that prefix is, and nothing is a command without one; and a listener hears a speaker within the say range, and anywhere when the range is 0; a typed line is shown unless it is a command, empty or does not read as the client packs it; an account above player level runs a command line while a player's is said or refused as the setting says; and an extended phrase is shown only when the client's own parser would read it.
 */

#include "AccountMgr.h"
#include "ChatMgr.h"
#include "ChatText.h"

#include <gtest/gtest.h>

TEST(ChatMgrTest, ALineShowsUnderTheChatLevelTheSpeakersPermissionsGiveIt)
{
    EXPECT_EQ(ChatMgr::FilterFor(47), ChatMgr::OpenChatFilter) << "the default permissions hold open chat";
    EXPECT_EQ(ChatMgr::FilterFor(ChatMgr::OpenChatPermission), 2);
    EXPECT_EQ(ChatMgr::FilterFor(ChatMgr::MenuChatPermission), 1);
    EXPECT_EQ(ChatMgr::FilterFor(ChatMgr::MenuChatPermission | ChatMgr::OpenChatPermission), 2) << "open chat outranks menu chat";
    EXPECT_EQ(ChatMgr::FilterFor(0x20), 0);
}

TEST(ChatMgrTest, AccountChatModeLimitsThePermissionsInThePlayerObject)
{
    EXPECT_EQ(ChatMgr::PermissionsForMode(47, 0), 47) << "open chat leaves the configured permissions unchanged";
    EXPECT_EQ(ChatMgr::PermissionsForMode(47, 1), 0x23) << "filtered chat keeps menu chat but removes open chat";
    EXPECT_EQ(ChatMgr::PermissionsForMode(47, 2), 0x20) << "closed chat clears only chat permissions";
    EXPECT_EQ(ChatMgr::PermissionsForMode(47, 3), 0x20) << "an invalid stored mode fails closed";
}

TEST(ChatMgrTest, ALineThatStartsWithTheCommandPrefixIsACommand)
{
    EXPECT_TRUE(ChatMgr::IsCommand(u".help", "."));
    EXPECT_TRUE(ChatMgr::IsCommand(u".", "."));
    EXPECT_FALSE(ChatMgr::IsCommand(u"hello.", "."));
    EXPECT_FALSE(ChatMgr::IsCommand(u" .help", ".")) << "the prefix must start the line";
    EXPECT_TRUE(ChatMgr::IsCommand(u"!!gm on", "!!"));
    EXPECT_FALSE(ChatMgr::IsCommand(u"!gm on", "!!"));
    EXPECT_TRUE(ChatMgr::IsCommand(u"\u00a7help", "\xC2\xA7")) << "a prefix outside ASCII is compared as text";
    EXPECT_FALSE(ChatMgr::IsCommand(u".help", "")) << "no prefix makes nothing a command";
}

TEST(ChatMgrTest, AListenerHearsASpeakerWithinTheSayRange)
{
    EXPECT_TRUE(ChatMgr::CanHear(5000.0f, 0.0f, 0.0f, 0.0f)) << "0 reaches the whole instance";
    EXPECT_TRUE(ChatMgr::CanHear(30.0f, 40.0f, 0.0f, 50.0f)) << "exactly at the range";
    EXPECT_FALSE(ChatMgr::CanHear(30.0f, 40.0f, 1.0f, 50.0f));
    EXPECT_TRUE(ChatMgr::CanHear(-10.0f, 0.0f, -10.0f, 20.0f));
}

TEST(ChatMgrTest, ATypedLineIsShownUnlessItIsACommandOrDoesNotRead)
{
    EXPECT_EQ(ChatMgr::Judge(ChatText::Write(u"hello"), "."), TypedLine::Shown);
    EXPECT_EQ(ChatMgr::Judge(ChatText::Write(u".help"), "."), TypedLine::Command) << "a command is never broadcast";
    EXPECT_EQ(ChatMgr::Judge(ChatText::Write(u".help"), "!"), TypedLine::Shown) << "the prefix in use decides";
    EXPECT_EQ(ChatMgr::Judge(ChatText::Write(u""), "."), TypedLine::Empty);
    EXPECT_EQ(ChatMgr::Judge("hello", "."), TypedLine::Unreadable);
}

TEST(ChatMgrTest, AnExtendedPhraseIsShownOnlyWhenTheClientsParserWouldReadIt)
{
    EXPECT_TRUE(ChatMgr::IsExtendedPhrase("Quest PersonaChatF GOAL_A 12 34 name"));
    EXPECT_TRUE(ChatMgr::IsExtendedPhrase("Stats Health 0"));
    EXPECT_TRUE(ChatMgr::IsExtendedPhrase("Duel 1 2 a b c d"));
    EXPECT_TRUE(ChatMgr::IsExtendedPhrase("  Tour  a  b")) << "the empty words between spaces are dropped, as the client drops them";
    EXPECT_FALSE(ChatMgr::IsExtendedPhrase("Quest PersonaChatF")) << "fewer than three words";
    EXPECT_FALSE(ChatMgr::IsExtendedPhrase("Quest  a")) << "two words once the empty one is dropped";
    EXPECT_FALSE(ChatMgr::IsExtendedPhrase("Hello there friend")) << "a kind the client does not parse";
    EXPECT_FALSE(ChatMgr::IsExtendedPhrase("quest a b")) << "kinds are compared exactly";
    EXPECT_FALSE(ChatMgr::IsExtendedPhrase("Questx a b"));
    EXPECT_FALSE(ChatMgr::IsExtendedPhrase(""));
}

TEST(ChatMgrTest, AStaffAccountRunsACommandLineAndAPlayersIsSaidOrRefused)
{
    EXPECT_EQ(ChatMgr::FateOf(SEC_GAMEMASTER, true), CommandLine::Run);
    EXPECT_EQ(ChatMgr::FateOf(SEC_MODERATOR, false), CommandLine::Run);
    EXPECT_EQ(ChatMgr::FateOf(SEC_PLAYER, true), CommandLine::Chat) << "a player's line is said as it was typed";
    EXPECT_EQ(ChatMgr::FateOf(SEC_PLAYER, false), CommandLine::Refuse);
}
