/*
 * Project Ambrose by Imjustchico
 * Copies the speaker and the Speech into each message field for field; ending an emote enters Unremarkable with no override; the emote override is an EmoteStateOverrideInfo whose state id is the Emoting state's and whose emote name is the animation, the rest left as the class defaults them, written in the compact format under the transmit mask, which is how the client reads MSG_ENTERSTATE's Data, and none is built when the loaded catalog cannot make one.
 */

#include "SpeechMessages.h"
#include "ObjectSerializer.h"
#include "PlayerStates.h"
#include "PropertyObject.h"

GameMessages::RadialChat SpeechMessages::Say(ChatSpeaker const& speaker, Speech const& speech)
{
    GameMessages::RadialChat message;
    message.SourceName = speaker.Name;
    message.SourceId = speaker.Guid;
    message.Message = speech.Payload;
    message.Filter = speaker.Filter;
    return message;
}

GameMessages::RadialQuickChat SpeechMessages::QuickChat(ChatSpeaker const& speaker, Speech const& speech)
{
    GameMessages::RadialQuickChat message;
    message.SourceName = speaker.Name;
    message.SourceId = speaker.Guid;
    message.MessageId = speech.PhraseId;
    message.Filter = speaker.Filter;
    return message;
}

GameMessages::RadialQuickChatExt SpeechMessages::QuickChatExt(ChatSpeaker const& speaker, Speech const& speech)
{
    GameMessages::RadialQuickChatExt message;
    message.SourceName = speaker.Name;
    message.SourceId = speaker.Guid;
    message.Message = speech.Payload;
    message.Filter = speaker.Filter;
    return message;
}

GameMessages::EnterState SpeechMessages::EndEmote(ChatSpeaker const& speaker)
{
    GameMessages::EnterState message;
    message.GameObjectId = speaker.Guid;
    message.State = PlayerStates::Unremarkable;
    return message;
}

GameMessages::EnterState SpeechMessages::Emote(ChatSpeaker const& speaker, Speech const& speech)
{
    GameMessages::EnterState message;
    message.GameObjectId = speaker.Guid;
    message.State = PlayerStates::Emoting;
    message.Data = speech.Payload;
    return message;
}

std::optional<std::string> SpeechMessages::EmoteState(TypeCatalogPtr const& catalog, std::string_view animation)
{
    PropertyObjectPtr const state = catalog ? PropertyObject::Create(catalog, PlayerStates::EmoteOverrideClass) : nullptr;
    if (!state || state->Set("m_stateNameID", PlayerStates::Emoting) != PropertySetResult::Ok || state->Set("m_emoteName", std::string(animation)) != PropertySetResult::Ok)
        return std::nullopt;
    EncodeResult const encoded = ObjectSerializer::Encode(state.get(), SerializerOptions());
    if (!encoded.Ok())
        return std::nullopt;
    return std::string(encoded.Bytes.begin(), encoded.Bytes.end());
}
