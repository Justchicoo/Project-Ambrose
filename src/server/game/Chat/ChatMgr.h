/*
 * Project Ambrose by Imjustchico
 * What a wizard says for the others around it (ChatMgr): each typed line, quick chat phrase, extended phrase, ordinary emote and custom emote its client asks the server to show is kept as a Speech until the world's next tick. The filter a line is shown under is the chat level the speaker's own client shows its line at, 2 for open chat, 1 for menu chat and 0 for neither, read from the permissions the server gave it; a typed line that starts with the command prefix is a command and is never shown to anyone: an account above player level runs it, and a player's is shown as an ordinary line or refused as GM.PlayerCommandsAsChat says, and a line whose text does not read or is empty is not shown either; a custom emote's WSTR text must be nonempty, valid UTF-16 and not a command; an extended phrase is shown only when the client's QuickChatX parser would read it, at least three words between spaces, the first naming one of the kinds it knows, Quest, Duel, Stats or Tour; and a listener in the same instance hears a wizard within the say range of it, anywhere in the instance when the range is 0.
 */

#ifndef AMBROSE_CHATMGR_H
#define AMBROSE_CHATMGR_H

#include "Types.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

enum class SpeechKind : uint8
{
    Say,
    QuickChat,
    QuickChatExt,
    Emote,
    CustomEmote
};

enum class TypedLine : uint8
{
    Shown,
    Command,
    Empty,
    Unreadable
};

enum class CommandLine : uint8
{
    Run,
    Chat,
    Refuse
};

struct Speech
{
    SpeechKind Kind = SpeechKind::Say;
    std::string Payload;
    std::u16string WidePayload;
    std::string Animation;
    uint32 PhraseId = 0;
    bool SpeakerSees = false;
};

class ChatMgr
{
public:
    static constexpr uint32 MenuChatPermission = 0x1;
    static constexpr uint32 OpenChatPermission = 0x4;
    static constexpr uint8 OpenChatFilter = 2;
    static constexpr uint8 MenuChatFilter = 1;
    static constexpr std::size_t MaxQueuedSpeech = 32;
    static constexpr std::size_t MinPhraseWords = 3;
    static constexpr std::string_view TalkingEmote = "Chat";
    static constexpr std::array<std::string_view, 4> PhraseKinds{ "Quest", "Duel", "Stats", "Tour" };

    ChatMgr() = delete;

    static uint8 FilterFor(uint32 permissions) noexcept;
    static bool IsCommand(std::u16string_view text, std::string_view prefix);
    static bool IsCustomEmoteText(std::u16string_view text, std::string_view prefix);
    static TypedLine Judge(std::string_view message, std::string_view prefix);
    static CommandLine FateOf(uint8 securityLevel, bool playersChat) noexcept;
    static bool IsExtendedPhrase(std::string_view message);
    static bool CanHear(float dx, float dy, float dz, float range) noexcept;
};

#endif
