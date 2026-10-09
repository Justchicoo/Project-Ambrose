/*
 * Project Ambrose by Imjustchico
 * Tests quest goal madlib values and SerializerBinary headers using a self-contained type dump.
 */

#include "QuestWireEncoder.h"
#include "TemplateDumpFixtures.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace
{
    Quests::BountyGoalTemplate MakeBounty()
    {
        Quests::BountyGoalTemplate bounty;
        bounty.Type = Quests::GoalType::Bounty;
        bounty.Name = "bounty";
        bounty.LocationName = "Ravenwood";
        bounty.BountyTotal = 5;
        Quests::TallyCounterTemplate tally;
        tally.Descriptor = "BOUNTY_TOTAL";
        bounty.TallyCounter = std::move(tally);
        return bounty;
    }

    QuestMadlibs::Argument const* FindArgument(QuestMadlibs::Block const& block, std::string_view token)
    {
        for (QuestMadlibs::Argument const& argument : block.Arguments)
            if (argument.Token == token)
                return &argument;
        return nullptr;
    }
}

TEST(QuestWireEncoderTest, AThreeOfFiveBountyGoalBlockCarriesCountTotalAndTallyText)
{
    Quests::BountyGoalTemplate const bounty = MakeBounty();
    QuestMadlibs::Block const block = QuestMadlibs::BuildGoal(Quests::QuestGoal{ bounty }, 3);

    ASSERT_EQ(block.BlockToken, "GOAL");
    QuestMadlibs::Argument const* const count = FindArgument(block, "COUNT");
    ASSERT_NE(count, nullptr);
    ASSERT_TRUE(std::holds_alternative<int32>(count->Value));
    EXPECT_EQ(std::get<int32>(count->Value), 3);

    QuestMadlibs::Argument const* const total = FindArgument(block, "TOTAL");
    ASSERT_NE(total, nullptr);
    ASSERT_TRUE(std::holds_alternative<int32>(total->Value));
    EXPECT_EQ(std::get<int32>(total->Value), 5);

    QuestMadlibs::Argument const* const tallyText = FindArgument(block, "TALLYTEXT");
    ASSERT_NE(tallyText, nullptr);
    ASSERT_TRUE(std::holds_alternative<std::string>(tallyText->Value));
    EXPECT_EQ(std::get<std::string>(tallyText->Value), "BOUNTY_TOTAL");
}

TEST(QuestWireEncoderTest, EveryEncodedBlobStartsWithAStoredSerializerBinaryHeader)
{
    TemplateDumpFixtures::Json classes = TemplateDumpFixtures::Json::object();
    TemplateDumpFixtures::AddTemplateClasses(classes);
    TemplateDumpFixtures::AddQuestClasses(classes);

    TypeRegistry registry;
    ASSERT_TRUE(registry.LoadFromText(TemplateDumpFixtures::Dump(classes), "quest-wire-test.json"))
        << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
    TypeCatalogPtr const catalog = registry.GetCatalog();
    ASSERT_NE(catalog, nullptr);

    QuestMadlibs::Block const goal = QuestMadlibs::BuildGoal(Quests::QuestGoal{ MakeBounty() }, 3);
    QuestWireEncoder::ObjectBuildResult const madlib = QuestWireEncoder::BuildMadlibBlock(catalog, goal);
    ASSERT_TRUE(madlib.Ok()) << madlib.Error;

    QuestWireEncoder::BlobEncodeResult const encoded = QuestWireEncoder::Encode(*madlib.Object, QuestWireEncoder::GoalsAndRewardsMask);
    ASSERT_TRUE(encoded.Ok()) << encoded.Error;
    ASSERT_GE(encoded.Bytes.size(), BlobEnvelope::HeaderSize);

    uint32 const actualHeader = static_cast<uint32>(encoded.Bytes[0]) |
        (static_cast<uint32>(encoded.Bytes[1]) << 8) |
        (static_cast<uint32>(encoded.Bytes[2]) << 16) |
        (static_cast<uint32>(encoded.Bytes[3]) << 24);
    uint32 const expectedHeader = 0x80000000u | static_cast<uint32>(encoded.Bytes.size() - BlobEnvelope::HeaderSize);
    EXPECT_EQ(actualHeader, expectedHeader);
}
