/*
 * Project Ambrose by Imjustchico
 * Tests the typed line without a terminal: inserting and deleting whole characters and words, moving the cursor, the history around a half-written line, command-name completion, the display width of wide and combining characters, and the window a narrow terminal shows.
 */

#include "ConsoleLineEditor.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    ConsoleKey Press(ConsoleKeyKind kind)
    {
        ConsoleKey key;
        key.Kind = kind;
        return key;
    }

    ConsoleKey Letter(std::string text)
    {
        ConsoleKey key;
        key.Kind = ConsoleKeyKind::Character;
        key.Text = std::move(text);
        return key;
    }

    void Type(ConsoleLineEditor& editor, std::string_view text)
    {
        for (char const c : text)
            editor.Apply(Letter(std::string(1, c)));
    }

    std::string Submit(ConsoleLineEditor& editor)
    {
        EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::Enter)), ConsoleLineEditor::Action::Submit);
        return editor.TakeLine();
    }

    ConsoleLineEditor::Completer Commands(std::vector<std::string> names)
    {
        return [names = std::move(names)](std::string_view prefix)
        {
            std::vector<std::string> matches;
            for (std::string const& name : names)
                if (name.size() >= prefix.size() && std::string_view(name).substr(0, prefix.size()) == prefix)
                    matches.push_back(name);
            return matches;
        };
    }
}

TEST(ConsoleLineEditorTest, TypingAndCursorMovementEditInPlace)
{
    ConsoleLineEditor editor;
    Type(editor, "shutdown");
    EXPECT_EQ(editor.GetLine(), "shutdown");
    EXPECT_EQ(editor.GetCursor(), 8u);
    EXPECT_EQ(editor.GetColumnsAfterCursor(), 0u);

    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::Home)), ConsoleLineEditor::Action::Redraw);
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::Home)), ConsoleLineEditor::Action::None);
    EXPECT_EQ(editor.GetColumnsAfterCursor(), 8u);
    Type(editor, "x");
    EXPECT_EQ(editor.GetLine(), "xshutdown");
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::Delete)), ConsoleLineEditor::Action::Redraw);
    EXPECT_EQ(editor.GetLine(), "xhutdown");
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::End)), ConsoleLineEditor::Action::Redraw);
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::Delete)), ConsoleLineEditor::Action::None);
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::Backspace)), ConsoleLineEditor::Action::Redraw);
    EXPECT_EQ(editor.GetLine(), "xhutdow");
}

TEST(ConsoleLineEditorTest, WholeCharactersMoveAndDeleteTogether)
{
    ConsoleLineEditor editor;
    editor.Apply(Letter("\xc3\xa9"));
    editor.Apply(Letter("a"));
    EXPECT_EQ(editor.GetLine(), "\xc3\xa9\x61");
    EXPECT_EQ(editor.GetCursor(), 3u);
    EXPECT_EQ(ConsoleLineEditor::Columns(editor.GetLine()), 2u);
    editor.Apply(Press(ConsoleKeyKind::Left));
    EXPECT_EQ(editor.GetCursor(), 2u);
    editor.Apply(Press(ConsoleKeyKind::Left));
    EXPECT_EQ(editor.GetCursor(), 0u);
    EXPECT_EQ(editor.GetColumnsAfterCursor(), 2u);
    editor.Apply(Press(ConsoleKeyKind::Delete));
    EXPECT_EQ(editor.GetLine(), "a");
}

TEST(ConsoleLineEditorTest, WordKeysMoveAndDeleteWholeWords)
{
    ConsoleLineEditor editor;
    Type(editor, "account create wizard");
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::WordLeft)), ConsoleLineEditor::Action::Redraw);
    EXPECT_EQ(editor.GetCursor(), 15u);
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::WordRight)), ConsoleLineEditor::Action::Redraw);
    EXPECT_EQ(editor.GetCursor(), 21u);
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::DeleteWord)), ConsoleLineEditor::Action::Redraw);
    EXPECT_EQ(editor.GetLine(), "account create ");
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::DeleteWord)), ConsoleLineEditor::Action::Redraw);
    EXPECT_EQ(editor.GetLine(), "account ");
}

TEST(ConsoleLineEditorTest, ClearAndKillLeaveTheRestOfTheLine)
{
    ConsoleLineEditor editor;
    Type(editor, "shutdown 30");
    for (int i = 0; i < 3; ++i)
        editor.Apply(Press(ConsoleKeyKind::Left));
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::KillToEnd)), ConsoleLineEditor::Action::Redraw);
    EXPECT_EQ(editor.GetLine(), "shutdown");
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::KillToEnd)), ConsoleLineEditor::Action::None);
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::ClearLine)), ConsoleLineEditor::Action::Redraw);
    EXPECT_TRUE(editor.GetLine().empty());
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::ClearLine)), ConsoleLineEditor::Action::None);
}

TEST(ConsoleLineEditorTest, HistoryRecallsPreviousLinesAndTheDraft)
{
    ConsoleLineEditor editor;
    Type(editor, "status");
    EXPECT_EQ(Submit(editor), "status");
    Type(editor, "help");
    EXPECT_EQ(Submit(editor), "help");
    Type(editor, "shut");

    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::Up)), ConsoleLineEditor::Action::Redraw);
    EXPECT_EQ(editor.GetLine(), "help");
    EXPECT_EQ(editor.GetCursor(), 4u);
    editor.Apply(Press(ConsoleKeyKind::Up));
    EXPECT_EQ(editor.GetLine(), "status");
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::Up)), ConsoleLineEditor::Action::None);
    editor.Apply(Press(ConsoleKeyKind::Down));
    EXPECT_EQ(editor.GetLine(), "help");
    editor.Apply(Press(ConsoleKeyKind::Down));
    EXPECT_EQ(editor.GetLine(), "shut");
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::Down)), ConsoleLineEditor::Action::None);
}

TEST(ConsoleLineEditorTest, HistorySkipsBlankAndRepeatedLines)
{
    ConsoleLineEditor editor;
    Type(editor, "status");
    Submit(editor);
    Type(editor, "status");
    Submit(editor);
    Type(editor, "   ");
    Submit(editor);
    EXPECT_EQ(editor.GetHistory(), (std::vector<std::string>{ "status" }));
    editor.Apply(Press(ConsoleKeyKind::Up));
    EXPECT_EQ(editor.GetLine(), "status");
    editor.Clear();
    EXPECT_TRUE(editor.GetLine().empty());
}

TEST(ConsoleLineEditorTest, TabCompletesOneCommandAndSuggestsWhenSeveralMatch)
{
    ConsoleLineEditor editor;
    editor.SetCompleter(Commands({ "account create", "account set password", "help", "shutdown", "status" }));

    Type(editor, "he");
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::Tab)), ConsoleLineEditor::Action::Redraw);
    EXPECT_EQ(editor.GetLine(), "help ");
    EXPECT_EQ(editor.GetCursor(), 5u);
    editor.Clear();

    Type(editor, "s");
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::Tab)), ConsoleLineEditor::Action::Suggest);
    EXPECT_EQ(editor.GetLine(), "s");
    EXPECT_EQ(editor.GetSuggestions(), (std::vector<std::string>{ "shutdown", "status" }));
    editor.Clear();

    Type(editor, "account ");
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::Tab)), ConsoleLineEditor::Action::Suggest);
    EXPECT_EQ(editor.GetSuggestions().size(), 2u);
    editor.Clear();

    Type(editor, "zz");
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::Tab)), ConsoleLineEditor::Action::None);
    EXPECT_EQ(editor.GetLine(), "zz");
}

TEST(ConsoleLineEditorTest, CompletionExtendsToTheSharedSpelling)
{
    ConsoleLineEditor editor;
    editor.SetCompleter(Commands({ "reload config", "reload scripts" }));
    Type(editor, "re");
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::Tab)), ConsoleLineEditor::Action::Redraw);
    EXPECT_EQ(editor.GetLine(), "reload ");
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::Tab)), ConsoleLineEditor::Action::Suggest);
    EXPECT_EQ(editor.GetSuggestions().size(), 2u);
}

TEST(ConsoleLineEditorTest, InterruptAndEndOfFileAreReported)
{
    ConsoleLineEditor editor;
    Type(editor, "half typed");
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::Interrupt)), ConsoleLineEditor::Action::Interrupt);
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::EndOfFile)), ConsoleLineEditor::Action::None);
    editor.Clear();
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::EndOfFile)), ConsoleLineEditor::Action::Close);
}

TEST(ConsoleLineEditorTest, OverlongLinesAreRefused)
{
    ConsoleLineEditor editor;
    editor.Apply(Letter(std::string(ConsoleLineEditor::MaxLine, 'a')));
    EXPECT_EQ(editor.GetLine().size(), ConsoleLineEditor::MaxLine);
    EXPECT_EQ(editor.Apply(Letter("b")), ConsoleLineEditor::Action::None);
    EXPECT_EQ(editor.GetLine().size(), ConsoleLineEditor::MaxLine);
}

TEST(ConsoleLineEditorTest, WideAndCombiningCharactersCountTheColumnsTheyTake)
{
    EXPECT_EQ(ConsoleLineEditor::Columns("shutdown"), 8u);
    EXPECT_EQ(ConsoleLineEditor::Columns("\xc3\xa9"), 1u);
    EXPECT_EQ(ConsoleLineEditor::Columns("\xe6\x97\xa5"), 2u);
    EXPECT_EQ(ConsoleLineEditor::Columns("e\xcc\x81"), 1u);
    EXPECT_EQ(ConsoleLineEditor::Columns("\xe2\x80\x8b"), 0u);
    EXPECT_EQ(ConsoleLineEditor::Columns("\xf0\x9f\x98\x80"), 2u);

    ConsoleLineEditor editor;
    editor.Apply(Letter("\xe6\x97\xa5"));
    EXPECT_EQ(editor.GetColumnsAfterCursor(), 0u);
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::Left)), ConsoleLineEditor::Action::Redraw);
    EXPECT_EQ(editor.GetCursor(), 0u);
    EXPECT_EQ(editor.GetColumnsAfterCursor(), 2u);
}

TEST(ConsoleLineEditorTest, FitKeepsTheCursorInsideTheColumnsOnOffer)
{
    ConsoleLineEditor::Window window = ConsoleLineEditor::Fit("abcdef", 6, 10);
    EXPECT_EQ(window.Start, 0u);
    EXPECT_EQ(window.End, 6u);
    EXPECT_EQ(window.CursorColumn, 6u);
    EXPECT_EQ(window.Columns, 6u);

    window = ConsoleLineEditor::Fit("abcdefghij", 10, 4);
    EXPECT_EQ(window.Start, 6u);
    EXPECT_EQ(window.End, 10u);
    EXPECT_EQ(window.CursorColumn, 4u);
    EXPECT_EQ(window.Columns, 4u);

    window = ConsoleLineEditor::Fit("abcdefghij", 0, 4);
    EXPECT_EQ(window.Start, 0u);
    EXPECT_EQ(window.End, 4u);
    EXPECT_EQ(window.CursorColumn, 0u);
    EXPECT_EQ(window.Columns, 4u);

    window = ConsoleLineEditor::Fit("\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5", 9, 5);
    EXPECT_EQ(window.Start, 3u);
    EXPECT_EQ(window.End, 9u);
    EXPECT_EQ(window.CursorColumn, 4u);
    EXPECT_EQ(window.Columns, 4u);
}

TEST(ConsoleLineEditorTest, TabInsideAWordLeavesTheLineAlone)
{
    ConsoleLineEditor editor;
    editor.SetCompleter(Commands({ "shutdown", "status" }));
    Type(editor, "shutdown 30");
    for (int step = 0; step < 8; ++step)
        editor.Apply(Press(ConsoleKeyKind::Left));
    EXPECT_EQ(editor.GetCursor(), 3u);
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::Tab)), ConsoleLineEditor::Action::None);
    EXPECT_EQ(editor.GetLine(), "shutdown 30");
    EXPECT_EQ(editor.GetCursor(), 3u);
}

TEST(ConsoleLineEditorTest, TabAtTheEndOfAWordCompletesWithoutDoublingTheSpace)
{
    ConsoleLineEditor editor;
    editor.SetCompleter(Commands({ "shutdown" }));
    Type(editor, "shut 30");
    for (int step = 0; step < 3; ++step)
        editor.Apply(Press(ConsoleKeyKind::Left));
    EXPECT_EQ(editor.GetCursor(), 4u);
    EXPECT_EQ(editor.Apply(Press(ConsoleKeyKind::Tab)), ConsoleLineEditor::Action::Redraw);
    EXPECT_EQ(editor.GetLine(), "shutdown 30");
    EXPECT_EQ(editor.GetCursor(), 8u);
}
