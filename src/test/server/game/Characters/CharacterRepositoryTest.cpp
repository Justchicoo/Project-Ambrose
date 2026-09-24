/*
 * Project Ambrose by Imjustchico
 * Tests the character repository: a closed characters database is an error, and with AMBROSE_TEST_DB set it installs the characters schema and checks wizards round-tripping every field and appearance value bit for bit, random ones and ones at every width's smallest and largest value; soft deletion hiding an offline wizard from its account's list and count while it stays readable by guid and can be restored, and refusing an online one; a wizard without appearance counted as the list finds it; rows half deleted refused by the schema; duplicates and data that cannot be stored; the online flag; and guids resuming above the highest guid ever used, even after its row is gone.
 */

#include "CharacterRepository.h"
#include "DBUpdater.h"
#include "Environment.h"
#include "GuidGenerator.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <limits>
#include <random>
#include <string>
#include <vector>

namespace
{
    CharacterSummary MakeCharacter(std::mt19937& random, uint64 guid, uint64 account, uint64 created)
    {
        auto const bits = [&random](uint32 width) { return static_cast<uint32>(random() & ((uint64{ 1 } << width) - 1)); };
        CharacterSummary character;
        character.Guid = guid;
        character.Account = account;
        character.NameIndices = random();
        if (guid % 2 == 0)
            character.CustomName = "M\xC3\xA9rlin " + std::to_string(guid);
        character.ShouldRename = (random() & 1) != 0;
        character.SchoolId = 2343174;
        character.Level = static_cast<int32>(bits(7)) + 1;
        character.Experience = static_cast<int32>(bits(20));
        character.World = static_cast<int32>(bits(8));
        character.Zone = "WizardCity/WC_Ravenwood";
        character.ZoneDisplay = "Ravenwood";
        character.PositionX = static_cast<float>(random()) / 7.0f;
        character.PositionY = -static_cast<float>(random()) / 11.0f;
        character.PositionZ = 0.1f;
        character.Orientation = 3.14159274f;
        character.Created = created;
        character.LastLogout = created + 60;

        CharacterAppearance& look = character.Appearance;
        look.BehaviorTemplateNameId = random();
        look.Gender = bits(2);
        look.Race = random();
        look.HeadHandsModel = static_cast<uint8>(bits(2));
        look.HairModel = static_cast<uint8>(bits(4));
        look.HatModel = static_cast<uint8>(bits(2));
        look.TorsoModel = static_cast<uint8>(bits(2));
        look.FeetModel = static_cast<uint8>(bits(2));
        look.WandModel = static_cast<uint8>(bits(2));
        look.SkinColor = static_cast<uint8>(bits(4));
        look.SkinDecal = static_cast<uint8>(bits(4));
        look.HairColor = static_cast<uint8>(bits(7));
        look.HatColor = static_cast<uint8>(bits(5));
        look.HatDecal = static_cast<uint8>(bits(5));
        look.TorsoColor = static_cast<uint8>(bits(5));
        look.TorsoDecal = static_cast<uint8>(bits(5));
        look.TorsoDecal2 = static_cast<uint8>(bits(5));
        look.FeetColor = static_cast<uint8>(bits(5));
        look.FeetDecal = static_cast<uint8>(bits(5));
        look.SkinDecal2 = static_cast<uint16>(bits(16));
        look.ExtendedHairColor = static_cast<uint8>(bits(8));
        look.ExtendedSkinDecal = static_cast<uint16>(bits(16));
        look.AfterCombatDance = static_cast<uint8>(bits(8));
        look.AfterCombatVictoryDance = random();
        look.NewPlayerOptions = random();
        look.NewPlayerOptions2 = random() | 0x80000000u;
        return character;
    }

    CharacterSummary MakeExtreme(bool largest, uint64 guid, uint64 account)
    {
        auto const top = [largest](uint32 width) { return largest ? static_cast<uint32>((uint64{ 1 } << width) - 1) : 0u; };
        CharacterSummary character;
        character.Guid = guid;
        character.Account = account;
        character.NameIndices = top(32);
        character.CustomName = largest ? std::string() + "\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80"
            "\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80" : std::string();
        character.ShouldRename = largest;
        character.SchoolId = top(32);
        character.Level = largest ? std::numeric_limits<int32>::max() : std::numeric_limits<int32>::min();
        character.Experience = largest ? std::numeric_limits<int32>::max() : std::numeric_limits<int32>::min();
        character.World = largest ? std::numeric_limits<int32>::max() : std::numeric_limits<int32>::min();
        character.Zone = largest ? std::string(128, 'z') : std::string();
        character.ZoneDisplay = largest ? std::string(128, 'd') : std::string();
        character.PositionX = largest ? std::numeric_limits<float>::max() : std::numeric_limits<float>::lowest();
        character.PositionY = largest ? std::numeric_limits<float>::min() : -std::numeric_limits<float>::min();
        character.PositionZ = largest ? 1.0e-30f : 0.0f;
        character.Orientation = largest ? 6.2831855f : -3.4028234e38f;
        character.Created = largest ? std::numeric_limits<uint64>::max() : 0;
        character.LastLogout = largest ? std::numeric_limits<uint64>::max() - 1 : 0;

        CharacterAppearance& look = character.Appearance;
        look.BehaviorTemplateNameId = top(32);
        look.Gender = top(32);
        look.Race = top(32);
        look.HeadHandsModel = static_cast<uint8>(top(2));
        look.HairModel = static_cast<uint8>(top(4));
        look.HatModel = static_cast<uint8>(top(2));
        look.TorsoModel = static_cast<uint8>(top(2));
        look.FeetModel = static_cast<uint8>(top(2));
        look.WandModel = static_cast<uint8>(top(2));
        look.SkinColor = static_cast<uint8>(top(4));
        look.SkinDecal = static_cast<uint8>(top(4));
        look.HairColor = static_cast<uint8>(top(7));
        look.HatColor = static_cast<uint8>(top(5));
        look.HatDecal = static_cast<uint8>(top(5));
        look.TorsoColor = static_cast<uint8>(top(5));
        look.TorsoDecal = static_cast<uint8>(top(5));
        look.TorsoDecal2 = static_cast<uint8>(top(5));
        look.FeetColor = static_cast<uint8>(top(5));
        look.FeetDecal = static_cast<uint8>(top(5));
        look.SkinDecal2 = static_cast<uint16>(top(16));
        look.ExtendedHairColor = static_cast<uint8>(top(8));
        look.ExtendedSkinDecal = static_cast<uint16>(top(16));
        look.AfterCombatDance = static_cast<uint8>(top(8));
        look.AfterCombatVictoryDance = top(32);
        look.NewPlayerOptions = top(32);
        look.NewPlayerOptions2 = top(32);
        return character;
    }

    class CharacterRepositoryDatabaseTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            info->Database = fmt::format("ambrose_characters_{:08x}", std::random_device()());
            _info = *info;
            ASSERT_TRUE(DBUpdater::Run(_info, "characters", UpdaterSettings{}));
            ASSERT_TRUE(CharacterDatabase.SetConnectionInfo(_info.ToConnectionString(), 1, 1));
            ASSERT_EQ(CharacterDatabase.Open(), 0u);
            _open = true;
        }

        void TearDown() override
        {
            if (_open)
                CharacterDatabase.Close();
            if (_info.Database.empty())
                return;
            MySQLConnectionInfo server = _info;
            server.Database.clear();
            MySQLConnection connection(server);
            if (connection.Open() == 0)
                connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(_info.Database)));
        }

        MySQLConnectionInfo _info;
        bool _open = false;
    };
}

TEST(CharacterRepositoryTest, AClosedCharactersDatabaseIsReportedAsAnError)
{
    CharacterDatabase.Close();
    std::mt19937 random(7);
    EXPECT_EQ(CharacterRepository::Create(MakeCharacter(random, 1, 1, 1)), CharacterOpResult::DatabaseError);
    EXPECT_EQ(CharacterRepository::LoadByAccount(1).Result, CharacterOpResult::DatabaseError);
    EXPECT_EQ(CharacterRepository::Load(1).Result, CharacterOpResult::DatabaseError);
    EXPECT_FALSE(CharacterRepository::CountByAccount(1));
    EXPECT_EQ(CharacterRepository::SoftDelete(1, 1, 1), CharacterOpResult::DatabaseError);
    EXPECT_EQ(CharacterRepository::Restore(1), CharacterOpResult::DatabaseError);
    EXPECT_EQ(CharacterRepository::SetOnline(1, true), CharacterOpResult::DatabaseError);
    EXPECT_FALSE(CharacterRepository::GetMaxGuid());
    EXPECT_EQ(CharacterRepository::GetResultName(CharacterOpResult::NotFound), "no such character");
}

TEST_F(CharacterRepositoryDatabaseTest, CharactersRoundTripEveryFieldBitForBit)
{
    std::mt19937 random(20260916);
    std::vector<CharacterSummary> created;
    for (uint64 guid = 101; guid <= 103; ++guid)
    {
        created.push_back(MakeCharacter(random, guid, 7, 1800000000 + guid));
        ASSERT_EQ(CharacterRepository::Create(created.back()), CharacterOpResult::Ok) << guid;
    }
    CharacterSummary const stranger = MakeCharacter(random, 200, 8, 1800000500);
    ASSERT_EQ(CharacterRepository::Create(stranger), CharacterOpResult::Ok);

    CharacterList const listed = CharacterRepository::LoadByAccount(7);
    ASSERT_EQ(listed.Result, CharacterOpResult::Ok);
    ASSERT_EQ(listed.Characters.size(), 3u);
    for (std::size_t index = 0; index < created.size(); ++index)
        EXPECT_TRUE(listed.Characters[index] == created[index]) << "character " << created[index].Guid;
    EXPECT_EQ(CharacterRepository::CountByAccount(7), 3u);
    EXPECT_EQ(CharacterRepository::CountByAccount(8), 1u);
    EXPECT_EQ(CharacterRepository::CountByAccount(9), 0u);
    EXPECT_TRUE(CharacterRepository::LoadByAccount(9).Characters.empty());

    CharacterSummary const largest = MakeExtreme(true, std::numeric_limits<uint64>::max(), std::numeric_limits<uint64>::max());
    CharacterSummary const smallest = MakeExtreme(false, 1, std::numeric_limits<uint64>::max());
    ASSERT_EQ(CharacterRepository::Create(smallest), CharacterOpResult::Ok);
    ASSERT_EQ(CharacterRepository::Create(largest), CharacterOpResult::Ok);
    CharacterList const extremes = CharacterRepository::LoadByAccount(std::numeric_limits<uint64>::max());
    ASSERT_EQ(extremes.Result, CharacterOpResult::Ok);
    ASSERT_EQ(extremes.Characters.size(), 2u);
    EXPECT_TRUE(extremes.Characters[0] == smallest);
    EXPECT_TRUE(extremes.Characters[1] == largest);
    EXPECT_EQ(extremes.Characters[1].CustomName->size(), CharacterRepository::MaxCustomNameBytes);

    CharacterLoad const one = CharacterRepository::Load(102);
    ASSERT_EQ(one.Result, CharacterOpResult::Ok);
    ASSERT_TRUE(one.Character);
    EXPECT_TRUE(*one.Character == created[1]);
    EXPECT_EQ(CharacterRepository::Load(999).Result, CharacterOpResult::NotFound);

    EXPECT_EQ(CharacterRepository::Create(created[0]), CharacterOpResult::AlreadyExists);
    CharacterSummary bad = MakeCharacter(random, 300, 7, 1);
    for (std::string const& name : { std::string(65, 'n'), std::string("bad\xFF"), std::string("tab\tname"), std::string("nul\0name", 8) })
    {
        bad.CustomName = name;
        EXPECT_EQ(CharacterRepository::Create(bad), CharacterOpResult::InvalidData);
    }
    bad.CustomName.reset();
    bad.Zone = std::string(129, 'z');
    EXPECT_EQ(CharacterRepository::Create(bad), CharacterOpResult::InvalidData);
    bad.Zone = "Zone";
    bad.DeletedAt = 5;
    EXPECT_EQ(CharacterRepository::Create(bad), CharacterOpResult::InvalidData);
    bad.DeletedAt.reset();
    bad.Guid = 0;
    EXPECT_EQ(CharacterRepository::Create(bad), CharacterOpResult::InvalidData);
    bad.Guid = 300;
    bad.Account = 0;
    EXPECT_EQ(CharacterRepository::Create(bad), CharacterOpResult::InvalidData);
    EXPECT_EQ(CharacterRepository::Load(300).Result, CharacterOpResult::NotFound);

    EXPECT_EQ(CharacterRepository::SetOnline(101, true), CharacterOpResult::Ok);
    EXPECT_EQ(CharacterRepository::SetOnline(101, true), CharacterOpResult::Ok);
    CharacterLoad const online = CharacterRepository::Load(101);
    ASSERT_TRUE(online.Character);
    EXPECT_TRUE(online.Character->Online);
    EXPECT_EQ(CharacterRepository::SetOnline(999, true), CharacterOpResult::NotFound);
}

TEST_F(CharacterRepositoryDatabaseTest, ASoftDeletedCharacterLeavesTheListAndCountButStaysReadable)
{
    std::mt19937 random(99);
    for (uint64 guid = 1; guid <= 3; ++guid)
        ASSERT_EQ(CharacterRepository::Create(MakeCharacter(random, guid, 42, 1800000000 + guid)), CharacterOpResult::Ok);

    EXPECT_EQ(CharacterRepository::SoftDelete(2, 41, 1900000000), CharacterOpResult::NotFound);
    EXPECT_EQ(CharacterRepository::SoftDelete(2, 42, 1900000000), CharacterOpResult::Ok);
    EXPECT_EQ(CharacterRepository::SoftDelete(2, 42, 1900000001), CharacterOpResult::NotFound);
    EXPECT_EQ(CharacterRepository::SoftDelete(77, 42, 1900000000), CharacterOpResult::NotFound);

    CharacterList const listed = CharacterRepository::LoadByAccount(42);
    ASSERT_EQ(listed.Result, CharacterOpResult::Ok);
    ASSERT_EQ(listed.Characters.size(), 2u);
    EXPECT_EQ(listed.Characters[0].Guid, 1u);
    EXPECT_EQ(listed.Characters[1].Guid, 3u);
    EXPECT_EQ(CharacterRepository::CountByAccount(42), 2u);

    CharacterLoad const deleted = CharacterRepository::Load(2);
    ASSERT_EQ(deleted.Result, CharacterOpResult::Ok);
    ASSERT_TRUE(deleted.Character);
    EXPECT_TRUE(deleted.Character->IsDeleted());
    EXPECT_EQ(deleted.Character->DeletedAt, 1900000000u);
    EXPECT_EQ(deleted.Character->DeletedAccount, 42u);
    EXPECT_EQ(deleted.Character->Account, 0u);

    EXPECT_EQ(CharacterRepository::Restore(2), CharacterOpResult::Ok);
    EXPECT_EQ(CharacterRepository::CountByAccount(42), 3u);
    CharacterLoad const restored = CharacterRepository::Load(2);
    ASSERT_TRUE(restored.Character);
    EXPECT_FALSE(restored.Character->IsDeleted());
    EXPECT_EQ(restored.Character->Account, 42u);
    EXPECT_FALSE(restored.Character->DeletedAccount);
    EXPECT_EQ(CharacterRepository::Restore(2), CharacterOpResult::NotFound);
    EXPECT_EQ(CharacterRepository::Restore(77), CharacterOpResult::NotFound);

    ASSERT_EQ(CharacterRepository::SetOnline(3, true), CharacterOpResult::Ok);
    EXPECT_EQ(CharacterRepository::SoftDelete(3, 42, 1900000000), CharacterOpResult::CharacterOnline);
    CharacterLoad const stillOnline = CharacterRepository::Load(3);
    ASSERT_TRUE(stillOnline.Character);
    EXPECT_FALSE(stillOnline.Character->IsDeleted());
    EXPECT_TRUE(stillOnline.Character->Online);
    EXPECT_EQ(CharacterRepository::SoftDelete(3, 41, 1900000000), CharacterOpResult::NotFound);
}

TEST_F(CharacterRepositoryDatabaseTest, TheSchemaKeepsTheListCountAndDeleteStateConsistent)
{
    std::mt19937 random(3);
    ASSERT_EQ(CharacterRepository::Create(MakeCharacter(random, 10, 5, 1800000000)), CharacterOpResult::Ok);
    ASSERT_TRUE(CharacterDatabase.DirectExecute("INSERT INTO `characters` (`guid`, `account`, `school_id`) VALUES (11, 5, 2343174)"));
    EXPECT_EQ(CharacterRepository::LoadByAccount(5).Characters.size(), 1u);
    EXPECT_EQ(CharacterRepository::CountByAccount(5), 1u);

    EXPECT_FALSE(CharacterDatabase.DirectExecute("UPDATE `characters` SET `deleted_at` = 1 WHERE `guid` = 10"));
    CharacterLoad const untouched = CharacterRepository::Load(10);
    ASSERT_TRUE(untouched.Character);
    EXPECT_FALSE(untouched.Character->IsDeleted());
}

TEST_F(CharacterRepositoryDatabaseTest, GuidsResumeAboveTheHighestGuidEverUsedAfterARestart)
{
    EXPECT_EQ(CharacterRepository::GetMaxGuid(), 0u);
    std::mt19937 random(5);
    {
        GuidGenerator generator;
        generator.Resume(*CharacterRepository::GetMaxGuid());
        for (int index = 0; index < 5; ++index)
        {
            std::optional<uint64> const guid = generator.Generate();
            ASSERT_TRUE(guid);
            ASSERT_EQ(CharacterRepository::Create(MakeCharacter(random, *guid, 1, 1800000000)), CharacterOpResult::Ok);
        }
        std::optional<uint64> const skipped = generator.Generate();
        ASSERT_TRUE(skipped);
        ASSERT_EQ(CharacterRepository::Create(MakeCharacter(random, *skipped + 1000, 1, 1800000000)), CharacterOpResult::Ok);
    }
    ASSERT_TRUE(CharacterDatabase.DirectExecute("DELETE FROM `characters` WHERE `guid` = 1006"));
    EXPECT_EQ(CharacterRepository::Load(1006).Result, CharacterOpResult::NotFound);
    std::optional<uint64> const highest = CharacterRepository::GetMaxGuid();
    ASSERT_TRUE(highest);
    EXPECT_EQ(*highest, 1006u);

    GuidGenerator restarted;
    restarted.Resume(*highest);
    std::optional<uint64> const next = restarted.Generate();
    ASSERT_TRUE(next);
    EXPECT_EQ(*next, 1007u);
    EXPECT_EQ(CharacterRepository::Create(MakeCharacter(random, *next, 1, 1800000000)), CharacterOpResult::Ok);
    EXPECT_EQ(CharacterRepository::GetMaxGuid(), 1007u);
}
