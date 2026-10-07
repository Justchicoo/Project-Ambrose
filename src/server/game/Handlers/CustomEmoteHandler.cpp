/*
 * Project Ambrose by Imjustchico
 * Handles the radial menu's custom emote messages without changing ordinary chat emotes: a custom animation must be present in the user's templates and the wizard's saved ownership masks, and a play request is relayed only with non-command valid UTF-16 text.
 */

#include "AnimationListMgr.h"
#include "ChatMgr.h"
#include "CommandMgr.h"
#include "CustomEmoteMgr.h"
#include "GameSession.h"
#include "Log.h"
#include "SpeechMessages.h"
#include "StringUtil.h"

#include <utility>

void GameSession::HandleCorePiiRadialMenuEmote(GameMessages::CorePiiRadialMenuEmote& message)
{
    if (!CanSpeak("a custom emote"))
        return;
    PlayerStats const* const stats = GetStats();
    std::shared_ptr<CustomEmoteStore const> const emotes = sCustomEmoteMgr.GetEmotes();
    if (stats == nullptr || !emotes->OwnsAnimation(message.EmoteAnimationName, stats->GetPurchasedCustomEmotes()))
    {
        LOG_DEBUG("server.gamesession", "Session {} requested custom emote animation {} without owning it; nobody is shown it", GetSessionId(), Ambrose::ForLog(message.EmoteAnimationName, 64));
        return;
    }
    QueueEmote(message.EmoteAnimationName, message.ExcludeOriginator, "a custom emote");
}

void GameSession::HandleRequestPiiRadialMenuPlayEmote(GameMessages::RequestPiiRadialMenuPlayEmote& message)
{
    if (!CanSpeak("a custom emote"))
        return;
    PlayerStats const* const stats = GetStats();
    std::shared_ptr<CustomEmoteStore const> const emotes = sCustomEmoteMgr.GetEmotes();
    if (stats == nullptr || !emotes->OwnsAnimation(message.EmoteAnimationName, stats->GetPurchasedCustomEmotes()))
    {
        LOG_DEBUG("server.gamesession", "Session {} requested custom emote animation {} without owning it; nobody is shown it", GetSessionId(), Ambrose::ForLog(message.EmoteAnimationName, 64));
        return;
    }
    if (!sAnimationListMgr.GetList()->Contains(message.EmoteAnimationName))
    {
        LOG_DEBUG("server.gamesession", "Session {} sent a custom emote naming the animation {}, which the install's animation list does not hold; nobody is shown it",
            GetSessionId(), Ambrose::ForLog(message.EmoteAnimationName, 64));
        return;
    }
    if (!ChatMgr::IsCustomEmoteText(message.EmoteText, sCommandMgr.GetPrefix()))
    {
        LOG_DEBUG("server.gamesession", "Session {} sent a custom emote with empty, unreadable or command text; nobody is shown it", GetSessionId());
        return;
    }
    Speech speech;
    speech.Kind = SpeechKind::CustomEmote;
    speech.WidePayload = std::move(message.EmoteText);
    speech.Animation = std::move(message.EmoteAnimationName);
    speech.SpeakerSees = true;
    QueueSpeech(std::move(speech), "a custom emote");
}

void GameSession::HearCustomEmote(ChatSpeaker const& speaker, Speech const& speech)
{
    SendDmlMessage(SpeechMessages::CustomEmote(speaker, speech));
}
