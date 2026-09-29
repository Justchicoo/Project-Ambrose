/*
 * Project Ambrose by Imjustchico
 * The text of a chat line as the client packs it into a byte-string field such as MSG_REQUESTRADIALCHAT's Message: a 16-bit count of UTF-16 code units, then the units, little-endian, the way a wide-string field is written. Read takes a line apart, refusing one whose count does not match its bytes, and Write puts one together.
 */

#ifndef AMBROSE_CHATTEXT_H
#define AMBROSE_CHATTEXT_H

#include "Types.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

class ChatText
{
public:
    static constexpr std::size_t MaxUnits = 65535;

    ChatText() = delete;

    static std::optional<std::u16string> Read(std::string_view bytes);
    static std::string Write(std::u16string_view text);
};

#endif
