/*
 * Project Ambrose by Imjustchico
 * Tests the messages that show a listener what a wizard said or played: a typed line, a quick chat phrase and an extended phrase each name the speaker by its packed name and global id and carry the speaker's chat level, the line and the extended phrase as the speaker's client wrote them and the phrase by its id, and an emote puts the speaker's object back in Unremarkable and then in its Emoting state with the override; and the override itself, which reads back through the transmit form as an EmoteStateOverrideInfo holding the Emoting state's id and the animation with the rest at the class's defaults, and is not built without a catalog that holds the class.
 */

#include "CharacterTypeFixtures.h"
#include "ChatText.h"
#include "ObjectSerializer.h"
#include "PackedName.h"
#include "PlayerStates.h"
#include "SpeechMessages.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>

#include <span>
#include <string>

namespace
{
    using CharacterTypeFixtures::Detail::AddClass;
    using CharacterTypeFixtures::Detail::Json;
    using CharacterTypeFixtures::Detail::Property;

    constexpr uint32 Wire = 0x1F;

    TypeCatalogPtr LoadCatalog(bool withOverride)
    {
        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", Json::array(), {});
        AddClass(classes, "class ObjStateOverrideInfo", Json::array({ "PropertyClass" }), { { "m_stateNameID", Property("unsigned int", "m_stateNameID", 0, Wire) } });
        if (withOverride)
            AddClass(classes, "class EmoteStateOverrideInfo", Json::array({ "ObjStateOverrideInfo", "PropertyClass" }), {
                { "m_stateNameID", Property("unsigned int", "m_stateNameID", 0, Wire) },
                { "m_emoteName", Property("std::string", "m_emoteName", 1, Wire) },
                { "m_particleAsset", Property("std::string", "m_particleAsset", 2, Wire) },
                { "m_loop", Property("bool", "m_loop", 3, Wire) },
                { "m_particleNode", Property("std::string", "m_particleNode", 4, Wire) },
                { "m_soundAsset", Property("std::string", "m_soundAsset", 5, Wire) },
                { "m_wizBangID", Property("unsigned int", "m_wizBangID", 6, Wire) } });
        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(Json{ { "version", 2 }, { "classes", std::move(classes) } }.dump(), "emote.json"))
            << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }

    ChatSpeaker Speaker()
    {
        return ChatSpeaker{ PackedName::FromIndices(0x00030201, 1), 0x1122334455667788ull, ChatMgr::OpenChatFilter };
    }
}

TEST(SpeechMessagesTest, EachMessageNamesTheSpeakerAndCarriesWhatItDid)
{
    ChatSpeaker const speaker = Speaker();
    Speech line;
    line.Payload = ChatText::Write(u"hello");
    GameMessages::RadialChat const say = SpeechMessages::Say(speaker, line);
    EXPECT_EQ(say.SourceName, speaker.Name);
    EXPECT_EQ(say.SourceId, speaker.Guid);
    EXPECT_EQ(say.Message, line.Payload) << "the line goes out as the speaker's client packed it";
    EXPECT_EQ(say.Filter, ChatMgr::OpenChatFilter);

    Speech phrase;
    phrase.Kind = SpeechKind::QuickChat;
    phrase.PhraseId = 1043;
    GameMessages::RadialQuickChat const quick = SpeechMessages::QuickChat(speaker, phrase);
    EXPECT_EQ(quick.SourceName, speaker.Name);
    EXPECT_EQ(quick.SourceId, speaker.Guid);
    EXPECT_EQ(quick.MessageId, 1043u);
    EXPECT_EQ(quick.Filter, ChatMgr::OpenChatFilter);

    Speech extended;
    extended.Kind = SpeechKind::QuickChatExt;
    extended.Payload = std::string("\x01\x02\x03", 3);
    GameMessages::RadialQuickChatExt const ext = SpeechMessages::QuickChatExt(speaker, extended);
    EXPECT_EQ(ext.SourceId, speaker.Guid);
    EXPECT_EQ(ext.Message, extended.Payload);
    EXPECT_EQ(ext.Filter, ChatMgr::OpenChatFilter);

    Speech emote;
    emote.Kind = SpeechKind::Emote;
    emote.Payload = "override";
    GameMessages::EnterState const state = SpeechMessages::Emote(speaker, emote);
    EXPECT_EQ(state.GameObjectId, speaker.Guid);
    EXPECT_EQ(state.State, PlayerStates::Emoting);
    EXPECT_EQ(state.Data, "override");

    GameMessages::EnterState const calm = SpeechMessages::EndEmote(speaker);
    EXPECT_EQ(calm.GameObjectId, speaker.Guid);
    EXPECT_EQ(calm.State, PlayerStates::Unremarkable);
    EXPECT_TRUE(calm.Data.empty()) << "the base state takes no override";
}

TEST(SpeechMessagesTest, TheEmoteOverrideReadsBackAsTheEmotingStateNamingTheAnimation)
{
    TypeCatalogPtr const catalog = LoadCatalog(true);
    std::optional<std::string> const bytes = SpeechMessages::EmoteState(catalog, "Wave");
    ASSERT_TRUE(bytes);
    DecodeResult const decoded = ObjectSerializer::Decode(catalog, std::span(reinterpret_cast<uint8 const*>(bytes->data()), bytes->size()), SerializerOptions());
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    ASSERT_TRUE(decoded.Object);
    EXPECT_EQ(decoded.BytesRead, bytes->size());
    EXPECT_EQ(decoded.Object->GetClass().Name, PlayerStates::EmoteOverrideClass);
    EXPECT_EQ(*decoded.Object->Get("m_stateNameID")->GetIf<uint32>(), PlayerStates::Emoting);
    EXPECT_EQ(*decoded.Object->Get("m_emoteName")->GetIf<std::string>(), "Wave");
    EXPECT_EQ(*decoded.Object->Get("m_loop")->GetIf<bool>(), false);
    EXPECT_EQ(*decoded.Object->Get("m_wizBangID")->GetIf<uint32>(), 0u);
    EXPECT_TRUE(decoded.Object->Get("m_particleAsset")->GetIf<std::string>()->empty());
}

TEST(SpeechMessagesTest, NoOverrideIsBuiltWithoutACatalogThatHoldsTheClass)
{
    EXPECT_FALSE(SpeechMessages::EmoteState(nullptr, "Wave"));
    EXPECT_FALSE(SpeechMessages::EmoteState(LoadCatalog(false), "Wave"));
}
