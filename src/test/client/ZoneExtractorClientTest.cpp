/*
 * Project Ambrose by Imjustchico
 * Extracts every zone of the user's own install, when AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH name it, with the counts recorded for the installed revision, r806919's below: every zone archive reads without an error and no two zones share a path; the only object list entries left out anywhere are sigils, whose classes the dump does not describe; the Commons comes out as WizardCity/WC_Hub under its own display key with every placed object its data lists but its six sigils, 123 of them the server's to send, and with its start and exit places; Ravenwood holds its objects, 40 of them the server's to send, its places and the templates of its statues and teachers; and with AMBROSE_TEST_DB set the rows fill a new world database that the zone manager loads, the server sending the objects its data marks as the server's own to send, and the Commons' volumes.xml and triggers.xml read through the authored classes that database holds: the Ravenwood POI sphere with its enter and exit events, the trigger that fires on entering it, and TeleportToShoppingDistrict, whose one result is a ResTeleport with no destination, as the client's class has no properties.
 */

#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "KiwadArchive.h"
#include "ServerClassScript.h"
#include "TypeRegistry.h"
#include "Environment.h"
#include "InstalledRevision.h"
#include "LogConfig.h"
#include "StringHash.h"
#include "ZoneExtractor.h"
#include "ZoneMgr.h"
#include "ZoneScript.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr std::string_view Commons = "WizardCity/WC_Hub";
    constexpr std::string_view Ravenwood = "WizardCity/WC_Ravenwood";
    constexpr int64 SentByServer = 3;
    constexpr std::string_view SigilClasses[] = { "class CombatSigilInfo", "class MinigameSigilInfo", "class BattlegroundSigilInfo", "class PvPCombatSigilInfo",
        "class ConfigurableMinigameSigilInfo", "class DynamicSigilInfo" };

    bool IsSigil(uint32 classHash)
    {
        return std::any_of(std::begin(SigilClasses), std::end(SigilClasses), [classHash](std::string_view name) { return StringHash::KiStringHash(name) == classHash; });
    }

    class ZoneExtractorClientTest : public testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
            if (!client || client->empty() || !dump || dump->empty())
                return;
            std::string error;
            std::optional<ZoneExtraction> extraction = ZoneExtractor::ExtractFromInstall(LogConfig::Utf8Path(*client), LogConfig::Utf8Path(*dump), error);
            ASSERT_TRUE(extraction) << error;
            s_extraction = std::make_unique<ZoneExtraction>(std::move(*extraction));
        }

        static void TearDownTestSuite()
        {
            s_extraction.reset();
        }

        void SetUp() override
        {
            if (!s_extraction)
                GTEST_SKIP() << "AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH are not both set";
            ASSERT_TRUE(s_extraction->Ok()) << s_extraction->Errors.front();
        }

        static ExtractedZone const& Zone(std::string_view path)
        {
            ExtractedZone const* const found = s_extraction->Find(path);
            EXPECT_NE(found, nullptr) << path;
            static ExtractedZone const none;
            return found ? *found : none;
        }

        static std::size_t SkippedWhole(std::string_view zone)
        {
            return static_cast<std::size_t>(std::count_if(s_extraction->Skipped.begin(), s_extraction->Skipped.end(), [zone](SkippedZonePart const& part)
            {
                return part.Zone == zone && part.WholeObject;
            }));
        }

        static std::size_t SentByTheServer(ExtractedZone const& zone)
        {
            return static_cast<std::size_t>(std::count_if(zone.Objects.begin(), zone.Objects.end(), [](ExtractedObject const& object) { return object.LoadingType == SentByServer; }));
        }

        static bool HasPlace(ExtractedZone const& zone, std::string_view name)
        {
            return std::any_of(zone.Locations.begin(), zone.Locations.end(), [name](ExtractedLocation const& location) { return location.Name == name; });
        }

        static inline std::unique_ptr<ZoneExtraction> s_extraction;
    };
}

TEST_F(ZoneExtractorClientTest, EveryZoneArchiveReadsAndNoTwoZonesSharePath)
{
    EXPECT_GT(s_extraction->Archives, 0u);
    ASSERT_FALSE(s_extraction->Zones.empty());
    std::set<std::string> paths;
    for (ExtractedZone const& zone : s_extraction->Zones)
    {
        EXPECT_FALSE(zone.Path.empty());
        EXPECT_TRUE(paths.insert(zone.Path).second) << zone.Path << " comes out twice";
    }
    for (SkippedZonePart const& part : s_extraction->Skipped)
    {
        EXPECT_FALSE(part.Zone.empty());
        EXPECT_NE(part.ClassHash, 0u) << part.Zone << " " << part.Path;
    }
}

TEST_F(ZoneExtractorClientTest, OnlySigilEntriesAreLeftOutOfTheObjectLists)
{
    std::size_t whole = 0;
    for (SkippedZonePart const& part : s_extraction->Skipped)
        if (part.WholeObject)
        {
            ++whole;
            EXPECT_TRUE(IsSigil(part.ClassHash)) << part.Zone << " " << part.Path << " is class hash " << part.ClassHash;
        }
    EXPECT_EQ(whole, s_extraction->GetSkippedObjectCount());
    EXPECT_GT(whole, 0u) << "the sigil classes are not in the client's type dump, so their entries wait for the classes the server describes";
}

TEST_F(ZoneExtractorClientTest, TheCommonsHoldsItsObjectsAndPlaces)
{
    ExtractedZone const& hub = Zone(Commons);
    EXPECT_EQ(hub.DisplayNameKey, "WizardZone_TheCommons");
    InstalledRevision::Expect(hub.Objects.size(), { { "r806919", 177u } }, "Commons objects");
    InstalledRevision::Expect(SkippedWhole(Commons), { { "r806919", 6u } }, "Commons sigils");
    InstalledRevision::Expect(hub.Locations.size(), { { "r806919", 31u } }, "Commons places");
    EXPECT_TRUE(HasPlace(hub, "Start"));
    EXPECT_TRUE(HasPlace(hub, "Target location (WC_Hub Street1 Exit)"));
    EXPECT_TRUE(HasPlace(hub, "Target location(WC_Hub Ravenwood)"));
    InstalledRevision::Expect(SentByTheServer(hub), { { "r806919", 123u } }, "Commons objects the server sends");
}

TEST_F(ZoneExtractorClientTest, RavenwoodHoldsItsObjectsPlacesAndTeachers)
{
    ExtractedZone const& ravenwood = Zone(Ravenwood);
    InstalledRevision::Expect(ravenwood.Objects.size(), { { "r806919", 93u } }, "Ravenwood objects");
    InstalledRevision::Expect(SkippedWhole(Ravenwood), { { "r806919", 4u } }, "Ravenwood sigils");
    InstalledRevision::Expect(ravenwood.Locations.size(), { { "r806919", 24u } }, "Ravenwood places");
    std::set<uint64> templates;
    for (ExtractedObject const& object : ravenwood.Objects)
        templates.insert(object.TemplateId);
    if (InstalledRevision::Is("r806919"))
    {
        for (uint64 const wanted : { 38232u, 38230u, 81102u, 1451035u, 39088u })
            EXPECT_TRUE(templates.contains(wanted)) << wanted;
    }
    InstalledRevision::Expect(SentByTheServer(ravenwood), { { "r806919", 40u } }, "Ravenwood objects the server sends");
}

TEST_F(ZoneExtractorClientTest, TheRowsFillAWorldDatabaseTheZoneManagerLoads)
{
    std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
    if (!text || text->empty())
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    std::optional<MySQLConnectionInfo> server = MySQLConnectionInfo::Parse(*text);
    ASSERT_TRUE(server);
    MySQLConnectionInfo world = *server;
    world.Database = fmt::format("ambrose_client_zones_{:08x}", std::random_device()());
    server->Database.clear();
    struct Cleanup
    {
        MySQLConnectionInfo Server;
        std::string Name;
        ~Cleanup()
        {
            sZoneMgr.Clear();
            WorldDatabase.Close();
            MySQLConnection connection(Server);
            if (connection.Open() == 0)
                connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(Name)));
        }
    } const cleanup{ *server, world.Database };

    ASSERT_TRUE(DBUpdater::Run(world, "world", UpdaterSettings{}));
    std::string error;
    ASSERT_TRUE(ZoneScript::Build(*s_extraction).Apply(world, error)) << error;
    ASSERT_TRUE(WorldDatabase.SetConnectionInfo(world.ToConnectionString(), 1, 1));
    ASSERT_EQ(WorldDatabase.Open(), 0u);
    sZoneMgr.Clear();
    ZoneLoadResult const loaded = sZoneMgr.LoadAll();
    ASSERT_TRUE(loaded.Loaded) << (loaded.Errors.empty() ? std::string() : loaded.Errors.front());
    EXPECT_EQ(loaded.Zones, s_extraction->Zones.size());
    EXPECT_EQ(loaded.Locations, s_extraction->GetLocationCount());
    EXPECT_EQ(loaded.Objects, s_extraction->GetObjectCount());
    std::vector<ZoneObjectSpawn> const* const hub = sZoneMgr.GetObjects()->In(Commons);
    ASSERT_NE(hub, nullptr);
    ExtractedZone const& extracted = Zone(Commons);
    ASSERT_EQ(hub->size(), extracted.Objects.size());
    std::size_t sent = 0;
    for (std::size_t index = 0; index < hub->size(); ++index)
    {
        ZoneObjectSpawn const& row = (*hub)[index];
        auto const same = std::find_if(extracted.Objects.begin(), extracted.Objects.end(), [&row](ExtractedObject const& object)
        {
            return object.TemplateId == row.TemplateId && object.ObjectId == row.ObjectId && object.Location == row.Position;
        });
        EXPECT_NE(same, extracted.Objects.end()) << "row " << row.Id << " with template " << row.TemplateId;
        if (row.IsSentByServer())
            ++sent;
    }
    EXPECT_EQ(sent, SentByTheServer(extracted));
}

TEST(ZoneExtractorClientTriggerTest, TheCommonsVolumesAndTriggersReadThroughTheAuthoredClasses)
{
    std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
    std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
    std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
    if (!client || client->empty() || !dump || dump->empty() || !text || text->empty())
        GTEST_SKIP() << "AMBROSE_CLIENT_DIR, AMBROSE_TYPE_DUMP_PATH and AMBROSE_TEST_DB are not all set";
    std::optional<MySQLConnectionInfo> server = MySQLConnectionInfo::Parse(*text);
    ASSERT_TRUE(server);
    MySQLConnectionInfo world = *server;
    world.Database = fmt::format("ambrose_client_triggers_{:08x}", std::random_device()());
    server->Database.clear();
    struct Cleanup
    {
        MySQLConnectionInfo Server;
        std::string Name;
        ~Cleanup()
        {
            WorldDatabase.Close();
            MySQLConnection connection(Server);
            if (connection.Open() == 0)
                connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(Name)));
        }
    } const cleanup{ *server, world.Database };
    ASSERT_TRUE(DBUpdater::Run(world, "world", UpdaterSettings{}));
    ASSERT_TRUE(WorldDatabase.SetConnectionInfo(world.ToConnectionString(), 1, 1));
    ASSERT_EQ(WorldDatabase.Open(), 0u);
    TypeDumpLoader::RawDump authored;
    std::vector<std::string> errors;
    ASSERT_TRUE(ServerClassScript::Read(authored, errors, ServerClassScript::AuthoredSource)) << (errors.empty() ? std::string() : errors.front());

    TypeRegistry registry;
    ASSERT_TRUE(registry.SetSupplement(std::move(authored), "the authored classes", errors)) << (errors.empty() ? std::string() : errors.front());
    ASSERT_TRUE(registry.LoadFromFile(LogConfig::Utf8Path(*dump)));
    std::string error;
    std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(LogConfig::Utf8Path(*client) / "Data" / "GameData" / "WizardCity-WC_Hub.wad", error);
    ASSERT_TRUE(archive) << error;
    KiwadReadResult const volumes = archive->Read(ZoneExtractor::VolumeEntry, ZoneExtractor::MaxEntryBytes);
    KiwadReadResult const triggers = archive->Read(ZoneExtractor::TriggerEntry, ZoneExtractor::MaxEntryBytes);
    ASSERT_TRUE(volumes.Succeeded()) << volumes.Error;
    ASSERT_TRUE(triggers.Succeeded()) << triggers.Error;

    ZoneExtraction extraction;
    ExtractedZone zone;
    zone.Path = Commons;
    ZoneExtractor::ReadVolumes(registry.GetCatalog(), zone, volumes.Data, extraction);
    ZoneExtractor::ReadTriggers(registry.GetCatalog(), zone, triggers.Data, extraction);
    ASSERT_TRUE(extraction.TriggerFailures.empty()) << extraction.TriggerFailures.front().File << ": " << extraction.TriggerFailures.front().Detail;

    auto const volume = std::find_if(zone.Volumes.begin(), zone.Volumes.end(), [](ExtractedVolume const& row) { return row.Name == "Ravenwood POI"; });
    ASSERT_NE(volume, zone.Volumes.end());
    EXPECT_EQ(volume->Shape, "Sphere");
    EXPECT_EQ(volume->EnterEvents, std::vector<std::string>{ "Enter_Ravenwood POI" });
    EXPECT_EQ(volume->ExitEvents, std::vector<std::string>{ "Exit_Ravenwood POI" });
    EXPECT_EQ(volume->LoadingType, 0) << "STATIC_CLIENT_SERVER";
    EXPECT_GT(volume->Radius, 0.0f);

    auto const named = [&zone](std::string_view name)
    {
        return std::find_if(zone.Triggers.begin(), zone.Triggers.end(), [name](ExtractedTrigger const& row) { return row.Name == name; });
    };
    auto const poi = named("Trigger POI Ravenwood");
    ASSERT_NE(poi, zone.Triggers.end());
    EXPECT_EQ(poi->FireEvents, std::vector<std::string>{ "Enter_Ravenwood POI" });
    auto const teleport = named("TeleportToShoppingDistrict");
    ASSERT_NE(teleport, zone.Triggers.end());
    ASSERT_EQ(teleport->Results.size(), 1u);
    EXPECT_EQ(teleport->Results[0].ClassName, std::optional<std::string>("class ResTeleport"));
    EXPECT_EQ(teleport->Results[0].ClassHash, StringHash::KiStringHash("class ResTeleport"));
    ClassInfo const* const resTeleport = registry.GetCatalog()->FindClass("class ResTeleport");
    ASSERT_NE(resTeleport, nullptr);
    EXPECT_TRUE(resTeleport->Properties.empty()) << "no destination is held; destinations come from the world's own data";
}
