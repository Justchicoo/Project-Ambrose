/*
 * Project Ambrose by Imjustchico
 * Fills the user's own r806919 ClientSpellbookBehavior with a tracker for Fire Cat, a tiered spell in group 4, and one for a plain spell in none, when AMBROSE_TYPE_DUMP_PATH names the install's type dump, and checks that every tracker reads back unchanged through the transmit form the player object MSG_LOGINCOMPLETE carries is written in, while the Save|Public mask the client loads MSG_SPELLLIST's Data with leaves the list out, since m_spellIDList is not Public: a spellbook reaches the client with the player and changes by MSG_ADDSPELLTOBOOK and MSG_REMOVESPELLFROMBOOK.
 */

#include "Environment.h"
#include "LogConfig.h"
#include "ObjectSerializer.h"
#include "PropertyFlags.h"
#include "SpellTracker.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    class SpellbookClientTest : public testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
            if (!dump || dump->empty())
                return;
            s_registry = std::make_unique<TypeRegistry>();
            ASSERT_TRUE(s_registry->LoadFromFile(LogConfig::Utf8Path(*dump)));
        }

        static void TearDownTestSuite()
        {
            s_registry.reset();
        }

        void SetUp() override
        {
            if (!s_registry)
                GTEST_SKIP() << "AMBROSE_TYPE_DUMP_PATH is not set";
        }

        static std::vector<SpellTracker> Read(PropertyObject const& book)
        {
            std::vector<SpellTracker> trackers;
            for (PropertyValue const& entry : *book.Get("m_spellIDList")->GetList())
            {
                PropertyObject const* const tracker = entry.AsObject();
                if (tracker == nullptr)
                    continue;
                trackers.push_back({ *tracker->Get("m_spellID")->GetIf<uint32>(), *tracker->Get("m_isRetired")->GetIf<bool>(),
                    *tracker->Get("m_tieredSpellGroupIndex")->GetIf<int32>() });
            }
            return trackers;
        }

        static inline std::unique_ptr<TypeRegistry> s_registry;
    };
}

TEST_F(SpellbookClientTest, TrackersTravelInTheTransmitFormAndNotInTheMaskSpellListIsReadWith)
{
    TypeCatalogPtr const catalog = s_registry->GetCatalog();
    PropertyObjectPtr const book = PropertyObject::Create(catalog, "class ClientSpellbookBehavior");
    ASSERT_TRUE(book) << "the type dump lists class ClientSpellbookBehavior";
    std::vector<SpellTracker> const spells{ { 103007158, false, 4 }, { 957065192, false, SpellTracker::NoGroup } };
    PropertyValue::List trackers;
    for (SpellTracker const& spell : spells)
    {
        PropertyObjectPtr tracker = PropertyObject::Create(catalog, "class SpellIDTracker");
        ASSERT_TRUE(tracker) << "the type dump lists class SpellIDTracker";
        ASSERT_EQ(tracker->Set("m_spellID", spell.SpellId), PropertySetResult::Ok);
        ASSERT_EQ(tracker->Set("m_isRetired", spell.Retired), PropertySetResult::Ok);
        ASSERT_EQ(tracker->Set("m_tieredSpellGroupIndex", spell.TieredGroupIndex), PropertySetResult::Ok);
        trackers.emplace_back(std::move(tracker));
    }
    ASSERT_EQ(book->Set("m_spellIDList", std::move(trackers)), PropertySetResult::Ok);

    EncodeResult const sent = ObjectSerializer::Encode(book.get());
    ASSERT_TRUE(sent.Ok()) << sent.Detail;
    DecodeResult const read = ObjectSerializer::Decode(catalog, sent.Bytes);
    ASSERT_TRUE(read.Ok() && read.Object) << read.Detail;
    EXPECT_EQ(Read(*read.Object), spells) << "the player object's own mask carries every tracker whole";

    SerializerOptions spellList;
    spellList.Mask = PropertyFlags::Bit(PropertyFlag::Save) | PropertyFlags::Bit(PropertyFlag::Public);
    EncodeResult const listed = ObjectSerializer::Encode(book.get(), spellList);
    ASSERT_TRUE(listed.Ok()) << listed.Detail;
    DecodeResult const reread = ObjectSerializer::Decode(catalog, listed.Bytes, spellList);
    ASSERT_TRUE(reread.Ok() && reread.Object) << reread.Detail;
    EXPECT_TRUE(Read(*reread.Object).empty()) << "m_spellIDList is not Public, so the mask MSG_SPELLLIST is read with leaves it out";
}
