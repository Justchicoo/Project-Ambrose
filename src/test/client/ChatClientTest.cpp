/*
 * Project Ambrose by Imjustchico
 * Reads what chat and emotes rest on from the user's own install, when AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH name it, with the counts recorded for the installed revision: QuickChat.xml holds r806919's 1106 phrases under its folders, among them Hello/Goodbye's first phrase, 267, which plays Wave; the animation list holds Chat, which every typed line plays, and every animation a phrase plays, so no phrase's emote is refused; and the EmoteStateOverrideInfo the server sends with the Emoting state reads back through the type dump as the class holding that state's id and the animation.
 */

#include "AnimationListMgr.h"
#include "Environment.h"
#include "InstalledRevision.h"
#include "KiwadArchive.h"
#include "LogConfig.h"
#include "ObjectSerializer.h"
#include "PlayerStates.h"
#include "QuickChatMgr.h"
#include "SpeechMessages.h"
#include "TypeRegistry.h"
#include "TypedView.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    class ChatClientTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
            if (!client || client->empty() || !dump || dump->empty())
                GTEST_SKIP() << "set AMBROSE_CLIENT_DIR to your own install and AMBROSE_TYPE_DUMP_PATH to its type dump to run this test";
            _registry = std::make_unique<TypeRegistry>(&sTypedViewRegistry);
            ASSERT_TRUE(_registry->LoadFromFile(LogConfig::Utf8Path(*dump))) << (_registry->GetErrors().empty() ? std::string() : _registry->GetErrors().front());
            std::string error;
            _archive = KiwadArchive::Open(LogConfig::Utf8Path(*client) / "Data" / "GameData" / std::string(QuickChatMgr::RootArchive), error);
            ASSERT_TRUE(_archive) << error;
        }

        std::unique_ptr<TypeRegistry> _registry;
        std::unique_ptr<KiwadArchive> _archive;
    };
}

TEST_F(ChatClientTest, QuickChatHoldsEveryPhraseWithTheAnimationItPlays)
{
    std::vector<std::string> errors;
    std::shared_ptr<QuickChatPhrases const> const phrases = QuickChatMgr::Read(*_archive, _registry->GetCatalog(), errors);
    ASSERT_TRUE(phrases) << errors.front();
    InstalledRevision::Expect(phrases->Size(), { { "r806919", 1106u } }, "quick chat phrases");
    EXPECT_EQ(phrases->Find(0), nullptr) << "folders carry id 0";
    QuickChatPhrase const* const hello = phrases->Find(267);
    ASSERT_NE(hello, nullptr);
    EXPECT_EQ(hello->Label, "chatEntryKey488");
    EXPECT_EQ(hello->CharAnim, "Wave");
}

TEST_F(ChatClientTest, TheAnimationListHoldsChatAndEveryAnimationAPhrasePlays)
{
    std::vector<std::string> errors;
    std::shared_ptr<AnimationList const> const animations = AnimationListMgr::Read(*_archive, errors);
    ASSERT_TRUE(animations) << errors.front();
    InstalledRevision::Expect(animations->Size(), { { "r806919", 472u } }, "animations (r806919 has 473 records, RunBack_Girl_Relic keyed twice)");
    for (std::string_view const name : { "Chat", "Wave", "Scold", "Cheer", "Shrug", "Cry", "Beg", "Clap", "Laugh", "Bow", "DabDance" })
        EXPECT_TRUE(animations->Contains(name)) << name;
    EXPECT_FALSE(animations->Contains("wave")) << "animation types are matched exactly";
}

TEST_F(ChatClientTest, TheEmoteOverrideReadsBackThroughTheTypeDump)
{
    std::optional<std::string> const bytes = SpeechMessages::EmoteState(_registry->GetCatalog(), "Wave");
    ASSERT_TRUE(bytes);
    DecodeResult const decoded = ObjectSerializer::Decode(_registry->GetCatalog(), std::span(reinterpret_cast<uint8 const*>(bytes->data()), bytes->size()), SerializerOptions());
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_EQ(decoded.BytesRead, bytes->size());
    EXPECT_EQ(decoded.Object->GetClass().Name, PlayerStates::EmoteOverrideClass);
    EXPECT_EQ(*decoded.Object->Get("m_stateNameID")->GetIf<uint32>(), PlayerStates::Emoting);
    EXPECT_EQ(*decoded.Object->Get("m_emoteName")->GetIf<std::string>(), "Wave");
}
