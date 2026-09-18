/*
 * Project Ambrose by Imjustchico
 * The keys a console line editor understands, and a decoder that turns a terminal's byte stream, control codes and escape sequences into them.
 */

#ifndef AMBROSE_CONSOLEKEYDECODER_H
#define AMBROSE_CONSOLEKEYDECODER_H

#include "Types.h"

#include <string>
#include <string_view>

enum class ConsoleKeyKind : uint8
{
    None,
    Character,
    Enter,
    Backspace,
    Delete,
    Left,
    Right,
    WordLeft,
    WordRight,
    Home,
    End,
    Up,
    Down,
    Tab,
    DeleteWord,
    ClearLine,
    KillToEnd,
    Interrupt,
    EndOfFile
};

struct ConsoleKey
{
    ConsoleKeyKind Kind = ConsoleKeyKind::None;
    std::string Text;
};

class ConsoleKeyDecoder
{
public:
    static constexpr std::size_t MaxPending = 64;

    void Feed(std::string_view bytes);
    bool Next(ConsoleKey& key);
    void Flush();
    void Reset();

private:
    enum class Step : uint8
    {
        Key,
        More,
        Skip
    };

    Step Decode(ConsoleKey& key, std::size_t& used) const;
    Step DecodeEscape(ConsoleKey& key, std::size_t& used) const;

    std::string _pending;
};

#endif
