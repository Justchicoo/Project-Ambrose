/*
 * Project Ambrose by Imjustchico
 * Tests that terminal bytes decode into editor keys: control characters, CSI and SS3 sequences, sequences split across reads, whole UTF-8 characters, and input that never completes.
 */

#include "ConsoleKeyDecoder.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    std::vector<ConsoleKey> Decode(ConsoleKeyDecoder& decoder, std::string_view bytes)
    {
        decoder.Feed(bytes);
        std::vector<ConsoleKey> keys;
        ConsoleKey key;
        while (decoder.Next(key))
            keys.push_back(key);
        return keys;
    }

    std::vector<ConsoleKeyKind> Kinds(std::vector<ConsoleKey> const& keys)
    {
        std::vector<ConsoleKeyKind> kinds;
        for (ConsoleKey const& key : keys)
            kinds.push_back(key.Kind);
        return kinds;
    }
}

TEST(ConsoleKeyDecoderTest, ControlCharactersBecomeEditingKeys)
{
    ConsoleKeyDecoder decoder;
    EXPECT_EQ(Kinds(Decode(decoder, "\r\n\t\x7f\x08")), (std::vector<ConsoleKeyKind>{
        ConsoleKeyKind::Enter, ConsoleKeyKind::Enter, ConsoleKeyKind::Tab, ConsoleKeyKind::Backspace, ConsoleKeyKind::Backspace }));
    EXPECT_EQ(Kinds(Decode(decoder, "\x01\x05\x0b\x15\x17\x04\x03")), (std::vector<ConsoleKeyKind>{
        ConsoleKeyKind::Home, ConsoleKeyKind::End, ConsoleKeyKind::KillToEnd, ConsoleKeyKind::ClearLine,
        ConsoleKeyKind::DeleteWord, ConsoleKeyKind::EndOfFile, ConsoleKeyKind::Interrupt }));
}

TEST(ConsoleKeyDecoderTest, EscapeSequencesBecomeNavigationKeys)
{
    ConsoleKeyDecoder decoder;
    EXPECT_EQ(Kinds(Decode(decoder, "\x1b[A\x1b[B\x1b[C\x1b[D")), (std::vector<ConsoleKeyKind>{
        ConsoleKeyKind::Up, ConsoleKeyKind::Down, ConsoleKeyKind::Right, ConsoleKeyKind::Left }));
    EXPECT_EQ(Kinds(Decode(decoder, "\x1bOA\x1bOH\x1bOF")), (std::vector<ConsoleKeyKind>{
        ConsoleKeyKind::Up, ConsoleKeyKind::Home, ConsoleKeyKind::End }));
    EXPECT_EQ(Kinds(Decode(decoder, "\x1b[H\x1b[F\x1b[3~\x1b[1~\x1b[4~")), (std::vector<ConsoleKeyKind>{
        ConsoleKeyKind::Home, ConsoleKeyKind::End, ConsoleKeyKind::Delete, ConsoleKeyKind::Home, ConsoleKeyKind::End }));
    EXPECT_EQ(Kinds(Decode(decoder, "\x1b[1;5C\x1b[1;5D")), (std::vector<ConsoleKeyKind>{
        ConsoleKeyKind::WordRight, ConsoleKeyKind::WordLeft }));
}

TEST(ConsoleKeyDecoderTest, SequencesSplitAcrossReadsWaitForTheRest)
{
    ConsoleKeyDecoder decoder;
    EXPECT_TRUE(Decode(decoder, "\x1b").empty());
    EXPECT_TRUE(Decode(decoder, "[").empty());
    std::vector<ConsoleKey> const keys = Decode(decoder, "A");
    ASSERT_EQ(keys.size(), 1u);
    EXPECT_EQ(keys[0].Kind, ConsoleKeyKind::Up);
}

TEST(ConsoleKeyDecoderTest, CharactersArriveWholeAndKeepTheirBytes)
{
    ConsoleKeyDecoder decoder;
    std::vector<ConsoleKey> const typed = Decode(decoder, "hi");
    ASSERT_EQ(typed.size(), 2u);
    EXPECT_EQ(typed[0].Kind, ConsoleKeyKind::Character);
    EXPECT_EQ(typed[0].Text, "h");
    EXPECT_EQ(typed[1].Text, "i");

    EXPECT_TRUE(Decode(decoder, "\xc3").empty());
    std::vector<ConsoleKey> const accented = Decode(decoder, "\xa9");
    ASSERT_EQ(accented.size(), 1u);
    EXPECT_EQ(accented[0].Kind, ConsoleKeyKind::Character);
    EXPECT_EQ(accented[0].Text, "\xc3\xa9");
}

TEST(ConsoleKeyDecoderTest, UnknownSequencesAndBytesAreDropped)
{
    ConsoleKeyDecoder decoder;
    EXPECT_TRUE(Decode(decoder, "\x1b[5~").empty());
    EXPECT_TRUE(Decode(decoder, "\x1bZ").empty());
    EXPECT_TRUE(Decode(decoder, "\x1c\x1f").empty());
    EXPECT_TRUE(Decode(decoder, "\x80\xff").empty());
    std::vector<ConsoleKey> const keys = Decode(decoder, "a");
    ASSERT_EQ(keys.size(), 1u);
    EXPECT_EQ(keys[0].Text, "a");
}

TEST(ConsoleKeyDecoderTest, IncompleteEscapesAndRunawayInputAreDiscarded)
{
    ConsoleKeyDecoder decoder;
    EXPECT_TRUE(Decode(decoder, "\x1b").empty());
    decoder.Flush();
    std::vector<ConsoleKey> const after = Decode(decoder, "x");
    ASSERT_EQ(after.size(), 1u);
    EXPECT_EQ(after[0].Text, "x");

    decoder.Feed(std::string(ConsoleKeyDecoder::MaxPending + 1, '1'));
    ConsoleKey key;
    EXPECT_FALSE(decoder.Next(key));

    decoder.Feed("q");
    ASSERT_TRUE(decoder.Next(key));
    EXPECT_EQ(key.Text, "q");
    decoder.Reset();
    EXPECT_FALSE(decoder.Next(key));
}
