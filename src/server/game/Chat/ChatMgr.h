/*
 * Project Ambrose by Imjustchico
 * Models queued typed, quick-chat, extended, ordinary-emote and custom-emote speech, command handling, chat permissions and filters, and which in-instance listeners hear it.
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
    static constexpr uint32 ChatPermissionMask = 0xF;
    static constexpr uint8 OpenChatFilter = 2;
    static constexpr uint8 MenuChatFilter = 1;
    static constexpr std::size_t MaxQueuedSpeech = 32;
    static constexpr std::size_t MinPhraseWords = 3;
    static constexpr std::string_view TalkingEmote = "Chat";
    static constexpr std::array<std::string_view, 4> PhraseKinds{ "Quest", "Duel", "Stats", "Tour" };

    ChatMgr() = delete;

    static uint8 FilterFor(uint32 permissions) noexcept;
    static uint32 PermissionsForMode(uint32 permissions, uint8 chatMode) noexcept;
    static bool IsCommand(std::u16string_view text, std::string_view prefix);
    static bool IsCustomEmoteText(std::u16string_view text, std::string_view prefix);
    static TypedLine Judge(std::string_view message, std::string_view prefix);
    static CommandLine FateOf(uint8 securityLevel, bool playersChat) noexcept;
    static bool IsExtendedPhrase(std::string_view message);
    static bool CanHear(float dx, float dy, float dz, float range) noexcept;
};

#endif
