/*
 * Project Ambrose by Imjustchico
 * Open chat outranks menu chat as the speaker's client ranks them, only an account above player level runs a command from chat, an empty prefix makes nothing a command, the prefix is compared unit by unit once read as UTF-16, a typed line is judged from its text as the client packs it, an extended phrase is split at spaces with the empty words between two spaces dropped, as the client's own split drops them, and the range is compared squared so no root is taken.
 */

#include "ChatMgr.h"
#include "AccountMgr.h"
#include "ChatText.h"
#include "Utf.h"

#include <algorithm>
#include <optional>
#include <vector>

uint8 ChatMgr::FilterFor(uint32 permissions) noexcept
{
    if ((permissions & OpenChatPermission) != 0)
        return OpenChatFilter;
    if ((permissions & MenuChatPermission) != 0)
        return MenuChatFilter;
    return 0;
}

bool ChatMgr::IsCommand(std::u16string_view text, std::string_view prefix)
{
    if (prefix.empty())
        return false;
    std::optional<std::u16string> const wide = Utf::Utf8ToUtf16(prefix, Utf::InvalidPolicy::Reject);
    return wide && !wide->empty() && text.starts_with(*wide);
}

bool ChatMgr::IsCustomEmoteText(std::u16string_view text, std::string_view prefix)
{
    return !text.empty() && Utf::Utf16ToUtf8(text, Utf::InvalidPolicy::Reject).has_value() && !IsCommand(text, prefix);
}

TypedLine ChatMgr::Judge(std::string_view message, std::string_view prefix)
{
    std::optional<std::u16string> const text = ChatText::Read(message);
    if (!text)
        return TypedLine::Unreadable;
    if (text->empty())
        return TypedLine::Empty;
    return IsCommand(*text, prefix) ? TypedLine::Command : TypedLine::Shown;
}

bool ChatMgr::IsExtendedPhrase(std::string_view message)
{
    std::vector<std::string_view> words;
    for (std::size_t start = 0; start <= message.size();)
    {
        std::size_t const space = std::min(message.find(' ', start), message.size());
        if (space > start)
            words.push_back(message.substr(start, space - start));
        start = space + 1;
    }
    return words.size() >= MinPhraseWords && std::ranges::find(PhraseKinds, words.front()) != PhraseKinds.end();
}

CommandLine ChatMgr::FateOf(uint8 securityLevel, bool playersChat) noexcept
{
    if (securityLevel > SEC_PLAYER)
        return CommandLine::Run;
    return playersChat ? CommandLine::Chat : CommandLine::Refuse;
}

bool ChatMgr::CanHear(float dx, float dy, float dz, float range) noexcept
{
    if (range <= 0.0f)
        return true;
    return dx * dx + dy * dy + dz * dz <= range * range;
}
