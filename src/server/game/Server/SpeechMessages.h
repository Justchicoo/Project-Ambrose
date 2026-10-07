/*
 * Project Ambrose by Imjustchico
 * The message each listener's client is sent to show what a wizard said or played, built from the speaker, its name as the client's name codec packs it, its global id and the chat level its lines show under, and from the Speech kept for it: MSG_RADIALCHAT with the typed line as the speaker's client packed it, MSG_RADIALQUICKCHAT with the phrase id, MSG_RADIALQUICKCHATEXT with the extended phrase, MSG_PIIRADIALMENUPLAYEMOTE with the custom emote's animation and text, and MSG_ENTERSTATE putting an ordinary emote's object in Emoting with the EmoteStateOverrideInfo that names the animation, after EndEmote's MSG_ENTERSTATE has put it back in Unremarkable, since a client plays an emote only as the object enters Emoting. The client's own MSG_CORE_PIIRADIALMENUEMOTE supplies the custom emote's state separately. EmoteState encodes that override as the client reads it from MSG_ENTERSTATE's Data.
 */

#ifndef AMBROSE_SPEECHMESSAGES_H
#define AMBROSE_SPEECHMESSAGES_H

#include "ChatMgr.h"
#include "GameMessages.h"
#include "TypeRegistry.h"

#include <optional>
#include <string>
#include <string_view>

struct ChatSpeaker
{
    std::string Name;
    uint64 Guid = 0;
    uint8 Filter = 0;
};

namespace SpeechMessages
{
    GameMessages::RadialChat Say(ChatSpeaker const& speaker, Speech const& speech);
    GameMessages::RadialQuickChat QuickChat(ChatSpeaker const& speaker, Speech const& speech);
    GameMessages::RadialQuickChatExt QuickChatExt(ChatSpeaker const& speaker, Speech const& speech);
    GameMessages::PiiRadialMenuPlayEmote CustomEmote(ChatSpeaker const& speaker, Speech const& speech);
    GameMessages::EnterState EndEmote(ChatSpeaker const& speaker);
    GameMessages::EnterState Emote(ChatSpeaker const& speaker, Speech const& speech);
    std::optional<std::string> EmoteState(TypeCatalogPtr const& catalog, std::string_view animation);
}

#endif
