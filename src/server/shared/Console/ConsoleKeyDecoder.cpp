/*
 * Project Ambrose by Imjustchico
 * Decodes control characters, CSI and SS3 escape sequences and whole UTF-8 characters from a terminal's bytes, holding back a sequence that has not arrived in full and dropping one that never completes.
 */

#include "ConsoleKeyDecoder.h"

namespace
{
    std::size_t Utf8Length(unsigned char lead)
    {
        if (lead < 0x80)
            return 1;
        if (lead >= 0xC2 && lead <= 0xDF)
            return 2;
        if (lead >= 0xE0 && lead <= 0xEF)
            return 3;
        if (lead >= 0xF0 && lead <= 0xF4)
            return 4;
        return 0;
    }
}

void ConsoleKeyDecoder::Feed(std::string_view bytes)
{
    _pending.append(bytes);
    if (_pending.size() > MaxPending)
        _pending.clear();
}

bool ConsoleKeyDecoder::Next(ConsoleKey& key)
{
    while (!_pending.empty())
    {
        key.Kind = ConsoleKeyKind::None;
        key.Text.clear();
        std::size_t used = 0;
        Step const step = Decode(key, used);
        if (step == Step::More)
            return false;
        _pending.erase(0, used);
        if (step == Step::Key)
            return true;
    }
    return false;
}

void ConsoleKeyDecoder::Flush()
{
    if (!_pending.empty() && _pending.front() == '\x1b')
        _pending.clear();
}

void ConsoleKeyDecoder::Reset()
{
    _pending.clear();
}

ConsoleKeyDecoder::Step ConsoleKeyDecoder::Decode(ConsoleKey& key, std::size_t& used) const
{
    unsigned char const first = static_cast<unsigned char>(_pending.front());
    used = 1;
    switch (first)
    {
        case 0x1B: return DecodeEscape(key, used);
        case '\r': case '\n': key.Kind = ConsoleKeyKind::Enter; return Step::Key;
        case 0x08: case 0x7F: key.Kind = ConsoleKeyKind::Backspace; return Step::Key;
        case '\t': key.Kind = ConsoleKeyKind::Tab; return Step::Key;
        case 0x01: key.Kind = ConsoleKeyKind::Home; return Step::Key;
        case 0x02: key.Kind = ConsoleKeyKind::Left; return Step::Key;
        case 0x03: key.Kind = ConsoleKeyKind::Interrupt; return Step::Key;
        case 0x04: key.Kind = ConsoleKeyKind::EndOfFile; return Step::Key;
        case 0x05: key.Kind = ConsoleKeyKind::End; return Step::Key;
        case 0x06: key.Kind = ConsoleKeyKind::Right; return Step::Key;
        case 0x0B: key.Kind = ConsoleKeyKind::KillToEnd; return Step::Key;
        case 0x0E: key.Kind = ConsoleKeyKind::Down; return Step::Key;
        case 0x10: key.Kind = ConsoleKeyKind::Up; return Step::Key;
        case 0x15: key.Kind = ConsoleKeyKind::ClearLine; return Step::Key;
        case 0x17: key.Kind = ConsoleKeyKind::DeleteWord; return Step::Key;
        default: break;
    }
    if (first < 0x20)
        return Step::Skip;
    std::size_t const length = Utf8Length(first);
    if (length == 0)
        return Step::Skip;
    if (_pending.size() < length)
        return Step::More;
    for (std::size_t i = 1; i < length; ++i)
        if ((static_cast<unsigned char>(_pending[i]) & 0xC0) != 0x80)
            return Step::Skip;
    used = length;
    key.Kind = ConsoleKeyKind::Character;
    key.Text.assign(_pending, 0, length);
    return Step::Key;
}

ConsoleKeyDecoder::Step ConsoleKeyDecoder::DecodeEscape(ConsoleKey& key, std::size_t& used) const
{
    if (_pending.size() < 2)
        return Step::More;
    char const introducer = _pending[1];
    if (introducer != '[' && introducer != 'O')
    {
        used = 2;
        return Step::Skip;
    }
    std::size_t index = 2;
    while (index < _pending.size())
    {
        unsigned char const byte = static_cast<unsigned char>(_pending[index]);
        if (byte < 0x20 || byte > 0x3F)
            break;
        ++index;
    }
    if (index >= _pending.size())
        return Step::More;
    unsigned char const last = static_cast<unsigned char>(_pending[index]);
    used = index + 1;
    if (last < 0x40 || last > 0x7E)
        return Step::Skip;
    std::string_view const parameters(_pending.data() + 2, index - 2);
    bool const control = parameters == "1;5";
    switch (last)
    {
        case 'A': key.Kind = ConsoleKeyKind::Up; return Step::Key;
        case 'B': key.Kind = ConsoleKeyKind::Down; return Step::Key;
        case 'C': key.Kind = control ? ConsoleKeyKind::WordRight : ConsoleKeyKind::Right; return Step::Key;
        case 'D': key.Kind = control ? ConsoleKeyKind::WordLeft : ConsoleKeyKind::Left; return Step::Key;
        case 'H': key.Kind = ConsoleKeyKind::Home; return Step::Key;
        case 'F': key.Kind = ConsoleKeyKind::End; return Step::Key;
        case '~':
            if (parameters == "1" || parameters == "7")
                key.Kind = ConsoleKeyKind::Home;
            else if (parameters == "3")
                key.Kind = ConsoleKeyKind::Delete;
            else if (parameters == "4" || parameters == "8")
                key.Kind = ConsoleKeyKind::End;
            else
                return Step::Skip;
            return Step::Key;
        default: break;
    }
    return Step::Skip;
}
