/*
 * Project Ambrose by Imjustchico
 * Takes what a wizard's client asks the wizards around it to see, once the wizard stands shown in an instance: a typed line whose text reads as the client packs it, unless it is a command, which is never shown to anyone, a quick chat phrase the install's QuickChat.xml holds, an extended phrase as the client wrote it once the client's own parser would read it, and an emote whose animation the install's animation list holds, played through an EmoteStateOverrideInfo naming that animation, which is encoded once for everyone who sees it, each listener first told the speaker's object is back in Unremarkable, since its client plays an emote only as the object enters Emoting. Each is kept until the world's next tick, a wizard keeping only so many between ticks, and then shown to each listener as the message its client plays it from, naming the speaker by its packed name and global id and showing a line under the speaker's own chat level.
 */

#include "AnimationListMgr.h"
#include "CommandMgr.h"
#include "GameSession.h"
#include "Log.h"
#include "PlayerStates.h"
#include "QuickChatMgr.h"
#include "SpeechMessages.h"
#include "StringUtil.h"
#include "TypeRegistry.h"

#include <optional>
#include <utility>

bool GameSession::CanSpeak(std::string_view what) const
{
    if (_mapId && !_publicObject.empty())
        return true;
    LOG_DEBUG("server.gamesession", "Session {} sent {} before its wizard stands shown in an instance; nobody is shown it", GetSessionId(), what);
    return false;
}

void GameSession::QueueSpeech(Speech speech, std::string_view what)
{
    if (_speech.size() >= ChatMgr::MaxQueuedSpeech)
    {
        LOG_DEBUG("server.gamesession", "Session {}'s wizard {} sent more than {} things to show in one tick; {} is dropped", GetSessionId(), _worldGuid, ChatMgr::MaxQueuedSpeech, what);
        return;
    }
    _speech.push_back(std::move(speech));
}

void GameSession::QueueEmote(std::string_view name, uint8 excludeOriginator, std::string_view what)
{
    if (!sAnimationListMgr.GetList()->Contains(name))
    {
        LOG_DEBUG("server.gamesession", "Session {} sent {} naming the animation {}, which the install's animation list does not hold; nobody is shown it", GetSessionId(), what,
            Ambrose::ForLog(name, 64));
        return;
    }
    std::optional<std::string> state = SpeechMessages::EmoteState(sTypeRegistry.GetCatalog(), name);
    if (!state)
    {
        LOG_WARN("server.gamesession", "Session {}'s {} cannot be shown: the loaded type dump cannot build the {} that plays {}", GetSessionId(), what,
            PlayerStates::EmoteOverrideClass, Ambrose::ForLog(name, 64));
        return;
    }
    Speech speech;
    speech.Kind = SpeechKind::Emote;
    speech.Payload = std::move(*state);
    speech.Animation = std::string(name);
    speech.SpeakerSees = excludeOriginator == 0;
    QueueSpeech(std::move(speech), what);
}

void GameSession::HandleRequestRadialChat(GameMessages::RequestRadialChat& message)
{
    if (!CanSpeak("a chat line"))
        return;
    switch (ChatMgr::Judge(message.Message, sCommandMgr.GetPrefix()))
    {
        case TypedLine::Unreadable:
            LOG_DEBUG("server.gamesession", "Session {} sent a chat line of {} bytes that does not read as a count and that many UTF-16 units; nobody is shown it", GetSessionId(),
                message.Message.size());
            return;
        case TypedLine::Empty:
            return;
        case TypedLine::Command:
            LOG_DEBUG("server.gamesession", "Session {}'s wizard {} typed a command, which is never shown to anyone", GetSessionId(), _worldGuid);
            return;
        case TypedLine::Shown:
            break;
    }
    Speech speech;
    speech.Kind = SpeechKind::Say;
    speech.Payload = std::move(message.Message);
    QueueSpeech(std::move(speech), "a chat line");
}

void GameSession::HandleRequestRadialQuickChat(GameMessages::RequestRadialQuickChat& message)
{
    if (!CanSpeak("a quick chat phrase"))
        return;
    if (!sQuickChatMgr.GetPhrases()->Find(message.MessageId))
    {
        LOG_DEBUG("server.gamesession", "Session {} sent quick chat phrase {}, which the install's {} does not hold; nobody is shown it", GetSessionId(), message.MessageId,
            QuickChatMgr::Entry);
        return;
    }
    Speech speech;
    speech.Kind = SpeechKind::QuickChat;
    speech.PhraseId = message.MessageId;
    QueueSpeech(std::move(speech), "a quick chat phrase");
}

void GameSession::HandleRequestRadialQuickChatExt(GameMessages::RequestRadialQuickChatExt& message)
{
    if (!CanSpeak("an extended quick chat phrase"))
        return;
    if (!ChatMgr::IsExtendedPhrase(message.Message))
    {
        LOG_DEBUG("server.gamesession", "Session {} sent an extended quick chat phrase the client's own parser would not read, {}; nobody is shown it", GetSessionId(),
            Ambrose::ForLog(message.Message, 64));
        return;
    }
    Speech speech;
    speech.Kind = SpeechKind::QuickChatExt;
    speech.Payload = std::move(message.Message);
    QueueSpeech(std::move(speech), "an extended quick chat phrase");
}

void GameSession::HandleCoreEmote(GameMessages::CoreEmote& message)
{
    if (CanSpeak("an emote"))
        QueueEmote(message.Name, message.ExcludeOriginator, "an emote");
}

std::vector<Speech> GameSession::TakeSpeech()
{
    return std::exchange(_speech, {});
}

void GameSession::HearSpeech(ChatSpeaker const& who, Speech const& speech)
{
    switch (speech.Kind)
    {
        case SpeechKind::Say:
            SendDmlMessage(SpeechMessages::Say(who, speech));
            return;
        case SpeechKind::QuickChat:
            SendDmlMessage(SpeechMessages::QuickChat(who, speech));
            return;
        case SpeechKind::QuickChatExt:
            SendDmlMessage(SpeechMessages::QuickChatExt(who, speech));
            return;
        case SpeechKind::Emote:
            SendDmlMessage(SpeechMessages::EndEmote(who));
            SendDmlMessage(SpeechMessages::Emote(who, speech));
            return;
    }
}
