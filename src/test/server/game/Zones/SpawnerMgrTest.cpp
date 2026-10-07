/*
 * Project Ambrose by Imjustchico
 * Tests the zone spawners on the zone object classes the fixtures lay out: an instance fills each spawner to its count, a despawned object comes back after its respawn time and not before, and no number of passes ever holds more than the count; Rate.Respawn changed as a live setting scales the delay of the next despawn with nothing restarted; a new set with a raised count spawns only the difference and a lowered one takes the extra away; a spawner with requirements places nothing; a game master's spawn is placed and deleted with its despawn effect while a zone's own object cannot be; a set with a broken row fails its build and names the row; and with AMBROSE_TEST_DB set, `.reload zone_spawner` with a raised count spawns the difference and a reload over a broken row keeps the old spawners serving.
 */

#include "ConfigMgr.h"
#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "LogTestDirectory.h"
#include "MemorySettingStore.h"
#include "ReloadMgr.h"
#include "Settings.h"
#include "SpawnerMgr.h"
#include "ZoneObjectFixtures.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace
{
    using namespace std::chrono_literals;

    ZoneSpawner Spawner(uint32 index, uint32 count, uint32 respawnSeconds)
    {
        ZoneSpawner spawner;
        spawner.Index = index;
        spawner.Name = fmt::format("SpawnPoint_Wood_0{}", index + 1);
        spawner.MaxSpawns = count;
        spawner.RespawnSeconds = respawnSeconds;
        ZoneSpawnEntry entry;
        entry.PercentChance = 100;
        entry.Object = ZoneObjectFixtures::Row(0, ZoneObjectFixtures::KioskTemplate, ZoneObjectLoading::DynamicServer, { 10.0f * index, 0.0f, 0.0f });
        spawner.Entries.push_back(entry);
        return spawner;
    }

    std::size_t Alive(Map const& map, uint32 index)
    {
        return static_cast<std::size_t>(std::count_if(map.GetObjects().begin(), map.GetObjects().end(), [index](MapObject const& object)
        {
            return object.Origin == MapObjectOrigin::Spawner && object.SpawnerIndex == index;
        }));
    }

    class SpawnerMgrTest : public testing::Test, protected ZoneObjectFixtures
    {
    protected:
        void SetUp() override
        {
            std::string error;
            ASSERT_TRUE(Build(error)) << error;
        }

        SpawnerContext Context(Map::Clock::time_point now, float rate = 1.0f)
        {
            SpawnerContext context;
            context.Now = now;
            context.ReleaseDelay = 2000ms;
            context.RespawnRate = rate;
            context.Sources = Sources();
            context.Roll = [](uint32) { return 0u; };
            return context;
        }

        uint64 FirstOf(Map const& map, uint32 index)
        {
            for (MapObject const& object : map.GetObjects())
                if (object.Origin == MapObjectOrigin::Spawner && object.SpawnerIndex == index)
                    return object.GlobalId;
            return 0;
        }

        Map::Clock::time_point _start = Map::Clock::now();
    };

    class SpawnerMgrDatabaseTest : public SpawnerMgrTest
    {
    protected:
        void SetUp() override
        {
            SpawnerMgrTest::SetUp();
            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            _info = *info;
            _info.Database = fmt::format("ambrose_spawn_{:08x}", std::random_device()());
            ASSERT_TRUE(DBUpdater::Run(_info, "world", UpdaterSettings{}));
            ASSERT_TRUE(WorldDatabase.SetConnectionInfo(_info.ToConnectionString(), 1, 1));
            ASSERT_EQ(WorldDatabase.Open(), 0u);
            _open = true;
            Execute(fmt::format("INSERT INTO `zone_template` (`zone_path`, `display_name_key`) VALUES ('{}', 'WizardCity_WC_Hub')", Hub));
            Execute(fmt::format("INSERT INTO `zone_spawner` (`zone_path`, `spawner_index`, `name`, `max_spawns`, `respawn_rate`) VALUES ('{}', 0, 'SpawnPoint_Wood_01', 1, 30)", Hub));
            Execute(fmt::format("INSERT INTO `zone_spawner_entry` (`zone_path`, `spawner_index`, `position`, `percent_chance`, `template_id`, `loading_type`) "
                "VALUES ('{}', 0, 0, 100, {}, 3)", Hub, KioskTemplate));
            sReloadMgr.Clear();
            sSpawnerMgr.Clear();
            sSpawnerMgr.RegisterReloadTargets();
        }

        void TearDown() override
        {
            sSpawnerMgr.Clear();
            sReloadMgr.Clear();
            if (_open)
                WorldDatabase.Close();
            if (_info.Database.empty())
                return;
            MySQLConnectionInfo server = _info;
            server.Database.clear();
            MySQLConnection connection(server);
            if (connection.Open() == 0)
                connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(_info.Database)));
        }

        void Execute(std::string const& sql)
        {
            ASSERT_TRUE(WorldDatabase.DirectExecute(sql)) << sql;
        }

        MapObjectChanges Update(Map& map, Map::Clock::time_point now)
        {
            std::shared_ptr<ZoneSpawners const> const spawners = sSpawnerMgr.Get();
            std::vector<ZoneSpawner> const* const list = spawners->In(Hub);
            return SpawnerMgr::Update(map, list ? *list : std::vector<ZoneSpawner>{}, sSpawnerMgr.GetGeneration(), Context(now));
        }

        MySQLConnectionInfo _info;
        bool _open = false;
    };
}

TEST_F(SpawnerMgrTest, AfterADespawnASpawnerRespawnsAfterItsTimeAndNeverAboveItsCount)
{
    Map map(1, Hub, true);
    std::vector<ZoneSpawner> const spawners{ Spawner(0, 2, 30) };
    MapObjectChanges const first = SpawnerMgr::Update(map, spawners, 1, Context(_start));
    EXPECT_EQ(first.Added.size(), 2u) << "an instance fills its spawner to its count";
    EXPECT_TRUE(first.Problems.empty()) << first.Problems.front().Text;
    EXPECT_EQ(Alive(map, 0), 2u);
    EXPECT_FALSE(SpawnerMgr::Update(map, spawners, 1, Context(_start + 1s)).Changed()) << "a full spawner places nothing";

    uint64 const killed = FirstOf(map, 0);
    MapObjectChanges despawned;
    ASSERT_TRUE(SpawnerMgr::Despawn(map, killed, std::nullopt, 0, Context(_start + 10s), despawned));
    EXPECT_EQ(despawned.Removed, std::vector<uint64>{ killed });
    EXPECT_EQ(map.FindObject(killed), nullptr);
    EXPECT_EQ(Alive(map, 0), 1u);

    EXPECT_FALSE(SpawnerMgr::Update(map, spawners, 1, Context(_start + 39s)).Changed()) << "nothing comes back before the respawn time has passed";
    EXPECT_EQ(Alive(map, 0), 1u);
    MapObjectChanges const back = SpawnerMgr::Update(map, spawners, 1, Context(_start + 40s));
    EXPECT_EQ(back.Added.size(), 1u) << "one comes back once its 30 seconds have passed";
    EXPECT_EQ(Alive(map, 0), 2u);

    for (int pass = 0; pass < 20; ++pass)
    {
        SpawnerMgr::Update(map, spawners, 1, Context(_start + 41s + std::chrono::seconds(pass * 60)));
        ASSERT_LE(Alive(map, 0), 2u) << "never more than the count, pass " << pass;
    }
    for (int kill = 0; kill < 2; ++kill)
    {
        MapObjectChanges changes;
        ASSERT_TRUE(SpawnerMgr::Despawn(map, FirstOf(map, 0), std::nullopt, 0, Context(_start + 2000s), changes));
    }
    EXPECT_EQ(Alive(map, 0), 0u);
    EXPECT_FALSE(SpawnerMgr::Update(map, spawners, 1, Context(_start + 2029s)).Changed());
    EXPECT_EQ(SpawnerMgr::Update(map, spawners, 1, Context(_start + 2030s)).Added.size(), 2u);
    EXPECT_EQ(Alive(map, 0), 2u);
}

TEST_F(SpawnerMgrTest, ChangingRateRespawnHalvesTheDelayOfTheNextDespawnWithoutARestart)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Write("gameserver.conf", "Realm.Name = Test\n");
    ConfigMgr config([](std::string const&) -> std::optional<std::string> { return std::nullopt; });
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());
    sSettings.Clear();
    std::vector<std::string> errors;
    ASSERT_TRUE(sSettings.DeclareFor(SettingApps::Game, errors)) << (errors.empty() ? "" : errors.front());
    std::vector<std::string> warnings;
    ASSERT_TRUE(sSettings.Start(config, std::make_shared<MemorySettingStore>(), warnings));
    sSpawnerMgr.SetRateReader([] { return sSettings.Get<float>("Rate.Respawn"); });

    Map map(1, Hub, true);
    std::vector<ZoneSpawner> const spawners{ Spawner(0, 1, 30) };
    SpawnerMgr::Update(map, spawners, 1, Context(_start, sSpawnerMgr.GetRespawnRate()));
    MapObjectChanges changes;
    ASSERT_TRUE(SpawnerMgr::Despawn(map, FirstOf(map, 0), std::nullopt, 0, Context(_start, sSpawnerMgr.GetRespawnRate()), changes));
    EXPECT_FALSE(SpawnerMgr::Update(map, spawners, 1, Context(_start + 29s, sSpawnerMgr.GetRespawnRate())).Changed()) << "at the default rate the delay is the spawner's 30 seconds";
    ASSERT_EQ(SpawnerMgr::Update(map, spawners, 1, Context(_start + 30s, sSpawnerMgr.GetRespawnRate())).Added.size(), 1u);

    SettingAuthor const author{ "test", 1, "unit_test" };
    ASSERT_TRUE(sSettings.Set("Rate.Respawn", "0.5", author, "faster respawns").Ok());
    EXPECT_FLOAT_EQ(sSpawnerMgr.GetRespawnRate(), 0.5f);
    Map::Clock::time_point const killedAt = _start + 100s;
    ASSERT_TRUE(SpawnerMgr::Despawn(map, FirstOf(map, 0), std::nullopt, 0, Context(killedAt, sSpawnerMgr.GetRespawnRate()), changes));
    EXPECT_FALSE(SpawnerMgr::Update(map, spawners, 1, Context(killedAt + 14s, sSpawnerMgr.GetRespawnRate())).Changed());
    EXPECT_EQ(SpawnerMgr::Update(map, spawners, 1, Context(killedAt + 15s, sSpawnerMgr.GetRespawnRate())).Added.size(), 1u) << "the next despawn waits half as long";

    sSpawnerMgr.Clear();
    sSettings.Clear();
}

TEST_F(SpawnerMgrTest, ANewSetWithARaisedCountSpawnsOnlyTheDifferenceAndALoweredOneTakesTheExtraAway)
{
    Map map(1, Hub, true);
    SpawnerMgr::Update(map, { Spawner(0, 1, 30) }, 1, Context(_start));
    uint64 const kept = FirstOf(map, 0);
    MapObjectChanges const raised = SpawnerMgr::Update(map, { Spawner(0, 3, 30) }, 2, Context(_start + 1s));
    EXPECT_EQ(raised.Added.size(), 2u) << "a count raised from one to three spawns two";
    EXPECT_TRUE(raised.Removed.empty());
    EXPECT_NE(map.FindObject(kept), nullptr) << "the object already alive stays";
    EXPECT_EQ(Alive(map, 0), 3u);

    MapObjectChanges const lowered = SpawnerMgr::Update(map, { Spawner(0, 2, 30) }, 3, Context(_start + 2s));
    EXPECT_EQ(lowered.Removed.size(), 1u);
    EXPECT_EQ(Alive(map, 0), 2u);

    ZoneSpawner moved = Spawner(0, 2, 30);
    moved.Entries.front().Object.Position = { 500.0f, 0.0f, 0.0f };
    MapObjectChanges const replaced = SpawnerMgr::Update(map, { moved }, 4, Context(_start + 3s));
    EXPECT_EQ(replaced.Removed.size(), 2u) << "objects the entries no longer place are taken away";
    EXPECT_EQ(replaced.Added.size(), 2u) << "and the spawner fills again from its new entries";

    MapObjectChanges const gone = SpawnerMgr::Update(map, {}, 5, Context(_start + 4s));
    EXPECT_EQ(gone.Removed.size(), 2u) << "a spawner that is no longer in the set takes its objects with it";
    EXPECT_EQ(Alive(map, 0), 0u);
}

TEST_F(SpawnerMgrTest, ASpawnerWithRequirementsOrAnInactiveOnePlacesNothing)
{
    Map map(1, Hub, true);
    ZoneSpawner holiday = Spawner(0, 2, 30);
    holiday.Name = "HalloweenSpawner1";
    holiday.HasRequirements = true;
    ZoneSpawner idle = Spawner(1, 2, 30);
    idle.Active = false;
    ZoneSpawner gated = Spawner(2, 2, 30);
    gated.Entries.front().Object.HasSpawnRequirements = true;
    EXPECT_FALSE(SpawnerMgr::Update(map, { holiday, idle, gated }, 1, Context(_start)).Changed()) << "requirements fail closed until the requirement engine exists";
    EXPECT_TRUE(map.GetObjects().empty());
}

TEST_F(SpawnerMgrTest, AGameMastersSpawnIsDeletedWithItsDespawnEffectAndAZoneObjectIsNot)
{
    Map map(1, Hub, true);
    MapObjectSpawner::Reconcile(map, { Row(1, DoorTemplate, ZoneObjectLoading::DynamicServer, {}) }, MapObjectStamp{ 1 }, Sources(), _start, 2000ms);
    MapObjectChanges placed;
    std::optional<uint64> const spawned = SpawnerMgr::SpawnTemporary(map, KioskTemplate, { 150.0f, 0.0f, 0.0f }, 0.0f, Context(_start), placed);
    ASSERT_TRUE(spawned) << (placed.Problems.empty() ? std::string() : placed.Problems.front().Text);
    EXPECT_EQ(placed.Added, std::vector<uint64>{ *spawned });
    MapObject const* const object = map.FindObject(*spawned);
    ASSERT_NE(object, nullptr);
    EXPECT_EQ(object->Origin, MapObjectOrigin::Command);
    DecodeResult const decoded = Decode(*object);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;

    EXPECT_EQ(SpawnerMgr::FindNearest(map, { 100.0f, 0.0f, 0.0f }, 600.0f), object) << "the nearest spawned object, never the zone's own";
    MapObjectChanges missing;
    EXPECT_FALSE(SpawnerMgr::SpawnTemporary(map, 999, {}, 0.0f, Context(_start), missing)) << "a template that cannot be read places nothing";
    ASSERT_EQ(missing.Problems.size(), 1u);
    EXPECT_TRUE(missing.Problems.front().MissingTemplate);

    MapObjectChanges refused;
    EXPECT_FALSE(SpawnerMgr::Despawn(map, map.FindSpawn(1)->GlobalId, 7, 0, Context(_start), refused)) << "a zone's own object is not taken away by a delete";
    MapObjectChanges deleted;
    ASSERT_TRUE(SpawnerMgr::Despawn(map, *spawned, 7, 42, Context(_start), deleted));
    ASSERT_EQ(deleted.Deleted.size(), 1u);
    EXPECT_EQ(deleted.Deleted.front().GlobalId, *spawned);
    EXPECT_EQ(deleted.Deleted.front().Effect, 7u);
    EXPECT_EQ(deleted.Deleted.front().Killer, 42u);
    EXPECT_TRUE(deleted.Removed.empty()) << "a despawn with an effect is not a plain removal";
    EXPECT_EQ(map.FindObject(*spawned), nullptr);
}

TEST_F(SpawnerMgrTest, ASetWithABrokenRowFailsItsBuildAndNamesTheRow)
{
    std::set<std::string, std::less<>> const zones{ Hub };
    std::vector<std::string> errors;
    ZoneSpawnEntry entry;
    entry.Position = 0;
    entry.PercentChance = 100;
    entry.Object.TemplateId = KioskTemplate;
    std::optional<ZoneSpawners> const good = SpawnerMgr::Build({ { Hub, Spawner(0, 1, 30) } }, { { Hub, { 0, entry } } }, zones, errors);
    ASSERT_TRUE(good) << errors.front();
    ASSERT_NE(good->In(Hub), nullptr);
    EXPECT_EQ(good->In(Hub)->front().Entries.size(), 2u) << "an entry joins the spawner its row names";

    ZoneSpawnEntry broken = entry;
    broken.Position = 1;
    broken.Object.TemplateId = 0;
    ZoneSpawnEntry stray = entry;
    stray.Position = 2;
    EXPECT_FALSE(SpawnerMgr::Build({ { Hub, Spawner(0, 1, 30) }, { "WizardCity/Nowhere", Spawner(1, 1, 30) }, { Hub, Spawner(2, 5000, 30) } },
        { { Hub, { 0, broken } }, { Hub, { 9, stray } } }, zones, errors));
    ASSERT_EQ(errors.size(), 4u);
    EXPECT_NE(errors[0].find("WizardCity/Nowhere"), std::string::npos) << errors[0];
    EXPECT_NE(errors[1].find("5000"), std::string::npos) << errors[1];
    EXPECT_NE(errors[2].find("places no template"), std::string::npos) << errors[2];
    EXPECT_NE(errors[3].find("not there"), std::string::npos) << errors[3];
}

TEST_F(SpawnerMgrDatabaseTest, AReloadWithARaisedCountSpawnsTheDifferenceAndABrokenRowKeepsTheOldSpawners)
{
    std::vector<std::string> errors;
    ASSERT_TRUE(sSpawnerMgr.Load(errors)) << errors.front();
    Map map(1, Hub, true);
    EXPECT_EQ(Update(map, _start).Added.size(), 1u);

    Execute(fmt::format("UPDATE `zone_spawner` SET `max_spawns` = 3 WHERE `zone_path` = '{}'", Hub));
    ReloadOutcome const raised = sReloadMgr.Reload(SpawnerMgr::ReloadTarget);
    ASSERT_TRUE(raised.Ok) << (raised.Errors.empty() ? std::string() : raised.Errors.front());
    MapObjectChanges const more = Update(map, _start + 1s);
    EXPECT_EQ(more.Added.size(), 2u) << "the reload spawns the difference";
    EXPECT_TRUE(more.Removed.empty());
    EXPECT_EQ(Alive(map, 0), 3u);

    uint64 const served = sSpawnerMgr.GetGeneration();
    Execute(fmt::format("INSERT INTO `zone_spawner_entry` (`zone_path`, `spawner_index`, `position`, `percent_chance`, `template_id`, `loading_type`) VALUES ('{}', 0, 1, 100, 0, 3)", Hub));
    Execute(fmt::format("UPDATE `zone_spawner` SET `max_spawns` = 5 WHERE `zone_path` = '{}'", Hub));
    ReloadOutcome const broken = sReloadMgr.Reload(SpawnerMgr::ReloadTarget);
    EXPECT_FALSE(broken.Ok);
    std::string const report = fmt::format("{}", fmt::join(broken.Errors, "\n"));
    EXPECT_NE(report.find("places no template"), std::string::npos) << report;
    EXPECT_EQ(sSpawnerMgr.GetGeneration(), served) << "the old spawners keep serving";
    EXPECT_EQ(sSpawnerMgr.Get()->In(Hub)->front().MaxSpawns, 3u);
    EXPECT_FALSE(Update(map, _start + 2s).Changed());
    EXPECT_EQ(Alive(map, 0), 3u);
}
