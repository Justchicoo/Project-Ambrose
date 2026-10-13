/*
 * Project Ambrose by Imjustchico
 * Tests for the panel's client data page: installs are listed with revision and archive count, type dumps show revision and fast copy state, caches list by kind with size, world table sets mark revision mismatches, and rebuilds and switches need their grants.
 */

#include "PanelClientData.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

namespace
{
    class FakeBackend : public ClientDataBackend
    {
    public:
        std::vector<PanelClientInstallRecord> Installs;
        std::vector<PanelServerRevisionRecord> Servers;
        std::optional<PanelTypeDumpRecord> Dump;
        std::vector<PanelCacheRecord> Caches;
        std::vector<PanelWorldTableSetRecord> Sets;
        PanelRevisionStateRecord State;
        bool Rebuilding = false;
        std::string LastRebuildRevision;
        std::string LastSwitchInstall;
        std::string LastSwitchRevision;

        std::vector<PanelClientInstallRecord> ListInstalls() override { return Installs; }
        std::vector<PanelServerRevisionRecord> ServerRevisions() override { return Servers; }
        std::optional<PanelTypeDumpRecord> TypeDumpInUse() override { return Dump; }
        std::vector<PanelCacheRecord> ListCaches() override { return Caches; }
        std::vector<PanelWorldTableSetRecord> ListWorldTableSets() override { return Sets; }
        PanelRevisionStateRecord RevisionState() override { return State; }

        PanelClientDataResult Rebuild(std::string_view revision, std::string&) override
        {
            LastRebuildRevision = std::string(revision);
            return PanelClientDataResult::Ok;
        }

        PanelClientDataResult SwitchInstall(std::string_view installId, std::string_view revision, std::string&) override
        {
            LastSwitchInstall = std::string(installId);
            LastSwitchRevision = std::string(revision);
            return PanelClientDataResult::Ok;
        }

        bool IsRebuildInProgress() override { return Rebuilding; }
    };

    PanelClientData::Caller FullCaller()
    {
        PanelClientData::Caller caller;
        caller.UserId = 1;
        caller.Username = "operator";
        caller.CanRead = true;
        caller.CanRebuild = true;
        caller.CanSwitch = true;
        return caller;
    }

    TEST(PanelClientDataTest, PageListsInstallsWithRevisionAndArchives)
    {
        FakeBackend backend;
        PanelClientInstallRecord install;
        install.Id = "install-1";
        install.DisplayName = "Wizard101";
        install.Revision = "12345";
        install.HasProgram = true;
        install.Archives = 42;
        install.InUse = true;
        backend.Installs.push_back(install);
        PanelClientData service(backend);
        auto const [result, page] = service.GetPage(FullCaller());
        EXPECT_EQ(result, PanelClientDataResult::Ok);
        ASSERT_EQ(page.Installs.size(), 1u);
        EXPECT_EQ(page.Installs[0].Revision, "12345");
        EXPECT_EQ(page.Installs[0].Archives, 42u);
        EXPECT_TRUE(page.Installs[0].InUse);
    }

    TEST(PanelClientDataTest, PageDeniedWithoutRead)
    {
        FakeBackend backend;
        PanelClientData service(backend);
        PanelClientData::Caller caller = FullCaller();
        caller.CanRead = false;
        auto const [result, page] = service.GetPage(caller);
        EXPECT_EQ(result, PanelClientDataResult::PermissionDenied);
    }

    TEST(PanelClientDataTest, RebuildDeniedWithoutGrant)
    {
        FakeBackend backend;
        PanelClientData service(backend);
        PanelClientData::Caller caller = FullCaller();
        caller.CanRebuild = false;
        std::string error;
        EXPECT_EQ(service.Rebuild(caller, "12345", error), PanelClientDataResult::PermissionDenied);
    }

    TEST(PanelClientDataTest, RebuildRefusedWhileInProgress)
    {
        FakeBackend backend;
        backend.Rebuilding = true;
        PanelClientData service(backend);
        std::string error;
        EXPECT_EQ(service.Rebuild(FullCaller(), "12345", error), PanelClientDataResult::RebuildInProgress);
    }

    TEST(PanelClientDataTest, SwitchDeniedWithoutGrant)
    {
        FakeBackend backend;
        PanelClientData service(backend);
        PanelClientData::Caller caller = FullCaller();
        caller.CanSwitch = false;
        std::string error;
        EXPECT_EQ(service.SwitchInstall(caller, "install-1", "12345", error), PanelClientDataResult::PermissionDenied);
    }

    TEST(PanelClientDataTest, PageMarksRevisionMismatch)
    {
        FakeBackend backend;
        PanelWorldTableSetRecord set;
        set.Name = "names";
        set.RowCount = 1000;
        set.ExtractionRevision = "11111";
        set.RevisionMismatch = true;
        backend.Sets.push_back(set);
        PanelClientData service(backend);
        auto const [result, page] = service.GetPage(FullCaller());
        EXPECT_EQ(result, PanelClientDataResult::Ok);
        ASSERT_EQ(page.WorldTableSets.size(), 1u);
        EXPECT_TRUE(page.WorldTableSets[0].RevisionMismatch);
    }

    TEST(PanelClientDataTest, PageJsonHasNoFileBytes)
    {
        FakeBackend backend;
        PanelClientInstallRecord install;
        install.Id = "install-1";
        install.DisplayName = "Test Install";
        install.Revision = "99999";
        install.Archives = 10;
        backend.Installs.push_back(install);
        PanelClientData service(backend);
        auto const [result, page] = service.GetPage(FullCaller());
        nlohmann::json const json = PanelClientData::PageToJson(page);
        std::string const text = json.dump();
        EXPECT_EQ(text.find("Root.wad"), std::string::npos);
        EXPECT_EQ(text.find(".wad"), std::string::npos);
    }

    TEST(PanelClientDataTest, TypeDumpJsonHasRevisionAndHash)
    {
        FakeBackend backend;
        PanelTypeDumpRecord dump;
        dump.Revision = "12345";
        dump.ExecutableHash = "abc123";
        dump.ExtractorVersion = "1.0";
        dump.BuildTime = 1234567890;
        dump.FastCopyCurrent = true;
        dump.InUse = true;
        backend.Dump = dump;
        PanelClientData service(backend);
        auto const [result, page] = service.GetPage(FullCaller());
        nlohmann::json const json = PanelClientData::PageToJson(page);
        EXPECT_EQ(json["type_dump"]["revision"], "12345");
        EXPECT_EQ(json["type_dump"]["executable_hash"], "abc123");
        EXPECT_TRUE(json["type_dump"]["fast_copy_current"].get<bool>());
    }
}
