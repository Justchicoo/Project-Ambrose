/*
 * Project Ambrose by Imjustchico
 * Takes what a wizard's client asks the wizards around it to see, once the wizard stands shown in an instance: a typed line whose text reads as the client packs it, unless it is a command, which is never shown to anyone: an account above player level runs it through CommandMgr with its replies gathered into as few MSG_SERVERMESSAGE as their length allows, each of which its client adds to its chat window and shows once as a notice, and hides the talking emote its client plays with every line, and a player's is shown as an ordinary line or refused with a reply as GM.PlayerCommandsAsChat says, a quick chat phrase the install's QuickChat.xml holds, an extended phrase as the client wrote it once the client's own parser would read it, an emote whose animation the install's animation list holds, played through an EmoteStateOverrideInfo naming that animation, and an owned custom emote relayed with its client-written line. Each is kept until the world's next tick, a wizard keeping only so many between ticks, and then shown to each listener as the message its client plays it from, naming the speaker by its packed name and global id and showing a line under the speaker's own chat level.
 */

#include "AnimationListMgr.h"
#include "ChatText.h"
#include "CommandCaller.h"
#include "CommandMgr.h"
#include "GameSession.h"
#include "Log.h"
#include "PlayerStates.h"
#include "QuickChatMgr.h"
#include "Settings.h"
#include "SpeechMessages.h"
#include "StringUtil.h"
#include "TypeRegistry.h"
#include "Utf.h"

#include <fmt/format.h>

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

namespace
{
    class SessionCaller final : public CommandCaller
    {
    public:
        static constexpr std::size_t MaxReplyUnits = 1500;

        explicit SessionCaller(GameSession& session) : _session(session), _level(session.GetSecurityLevel()) {}

        uint8 GetSecurityLevel() const override { return _level; }
        bool IsConsole() const override { return false; }
        std::string GetName() const override { return fmt::format("account {} with wizard {}", _session.GetAccountId(), _session.GetCharacterId()); }
        uint64 GetCharacterId() const override { return _session.GetCharacterId(); }

        void Reply(std::string_view line) override
        {
            std::optional<std::u16string> const wide = Utf::Utf8ToUtf16(line, Utf::InvalidPolicy::ReplaceWithU_FFFD);
            if (!wide)
                return;
            std::u16string_view rest = *wide;
            do
            {
                if (!_pending.empty() && _pending.size() + 1 + rest.size() > MaxReplyUnits)
                    Flush();
                if (!_pending.empty())
                    _pending.push_back(u'\n');
                std::size_t const taken = std::min(rest.size(), MaxReplyUnits - _pending.size());
                _pending += rest.substr(0, taken);
                rest.remove_prefix(taken);
                if (!rest.empty())
                    Flush();
            } while (!rest.empty());
        }

        void Flush()
        {
            if (_pending.empty())
                return;
            _session.SendServerMessage(std::exchange(_pending, {}));
        }

    private:
        GameSession& _session;
        uint8 _level;
        std::u16string _pending;
    };
}

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
    _hideNextChatEmote = false;
    switch (ChatMgr::Judge(message.Message, sCommandMgr.GetPrefix()))
    {
        case TypedLine::Unreadable:
            LOG_DEBUG("server.gamesession", "Session {} sent a chat line of {} bytes that does not read as a count and that many UTF-16 units; nobody is shown it", GetSessionId(),
                message.Message.size());
            return;
        case TypedLine::Empty:
            return;
        case TypedLine::Command:
            if (!TakeCommandLine(message.Message))
                return;
            break;
        case TypedLine::Shown:
            break;
    }
    if (!CanSpeak("a chat line"))
        return;
    Speech speech;
    speech.Kind = SpeechKind::Say;
    speech.Payload = std::move(message.Message);
    QueueSpeech(std::move(speech), "a chat line");
}

bool GameSession::TakeCommandLine(std::string_view packed)
{
    switch (ChatMgr::FateOf(GetSecurityLevel(), sSettings.Get<bool>("GM.PlayerCommandsAsChat")))
    {
        case CommandLine::Chat:
            return true;
        case CommandLine::Refuse:
            _hideNextChatEmote = true;
            SendServerMessage(u"Commands are for game masters, so that line was not said.");
            LOG_DEBUG("server.gamesession", "Session {}'s wizard {} typed a command at player level, which GM.PlayerCommandsAsChat refuses", GetSessionId(), _worldGuid);
            return false;
        case CommandLine::Run:
            break;
    }
    _hideNextChatEmote = true;
    std::optional<std::u16string> const text = ChatText::Read(packed);
    std::optional<std::string> const line = text ? Utf::Utf16ToUtf8(*text, Utf::InvalidPolicy::ReplaceWithU_FFFD) : std::nullopt;
    if (!line)
        return false;
    SessionCaller caller(*this);
    sCommandMgr.Execute(caller, *line);
    caller.Flush();
    return false;
}

void GameSession::HandleRequestRadialQuickChat(GameMessages::RequestRadialQuickChat& message)
{
    _hideNextChatEmote = false;
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
    _hideNextChatEmote = false;
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
    if (_hideNextChatEmote && message.Name == ChatMgr::TalkingEmote)
    {
        _hideNextChatEmote = false;
        return;
    }
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
        case SpeechKind::CustomEmote:
            HearCustomEmote(who, speech);
            return;
    }
}
