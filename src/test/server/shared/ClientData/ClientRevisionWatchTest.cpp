/*
 * Project Ambrose by Imjustchico
 * Tests that an install's update is reported once two polls agree on it, that the program's size counts as a change, that an unreadable install is no change, and that a reported update is not reported again once accepted.
 */

#include "ClientRevisionWatch.h"
#include "ClientExtractionScript.h"
#include "ClientSystem.h"
#include "LogTestDirectory.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    void WriteFile(std::filesystem::path const& file, std::string const& content)
    {
        std::filesystem::create_directories(file.parent_path());
        std::ofstream stream(file, std::ios::binary | std::ios::trunc);
        stream << content;
    }

    std::filesystem::path MakeInstall(LogTestDirectory const& directory, std::string const& revision)
    {
        std::filesystem::path const root = directory.Path() / "Wizard101";
        WriteFile(root / "Data" / "GameData" / "Root.wad", "KIWAD");
        WriteFile(root / "Bin" / "revision.dat", revision);
        WriteFile(root / "Bin" / "WizardGraphicalClient.exe", "MZ");
        return root;
    }
}

TEST(ClientRevisionWatchTest, AnUpdateIsReportedOnceTwoPollsAgreeAndNotAgainOnceAccepted)
{
    LogTestDirectory const directory;
    std::filesystem::path const root = MakeInstall(directory, "r806919.Wizard_1_610");
    LocalClientSystem const system;
    std::optional<ClientFingerprint> const first = ClientFingerprint::Read(system, root);
    ASSERT_TRUE(first);
    EXPECT_EQ(first->Revision, "r806919.Wizard_1_610");
    EXPECT_EQ(first->ExecutableSize, 2u);

    ClientRevisionWatch watch;
    EXPECT_FALSE(watch.Poll(system)) << "a watch with no install reports nothing";
    watch.Watch(root, *first);
    EXPECT_FALSE(watch.Poll(system));

    WriteFile(root / "Bin" / "revision.dat", "r807500.Wizard_1_620");
    EXPECT_FALSE(watch.Poll(system)) << "the first poll that sees a new revision waits for a second";
    std::optional<ClientFingerprint> const updated = watch.Poll(system);
    ASSERT_TRUE(updated);
    EXPECT_EQ(updated->Revision, "r807500.Wizard_1_620");
    EXPECT_EQ(watch.GetCurrent(), *first) << "the watch keeps the old revision until the update is accepted";

    watch.Accept(*updated);
    EXPECT_FALSE(watch.Poll(system));
    EXPECT_FALSE(watch.Poll(system));
}

TEST(ClientRevisionWatchTest, ANewProgramIsAnUpdateAndAnUpdateStillBeingWrittenIsNot)
{
    LogTestDirectory const directory;
    std::filesystem::path const root = MakeInstall(directory, "r806919");
    LocalClientSystem const system;
    ClientRevisionWatch watch;
    watch.Watch(root, *ClientFingerprint::Read(system, root));

    WriteFile(root / "Bin" / "WizardGraphicalClient.exe", "MZ-1");
    EXPECT_FALSE(watch.Poll(system));
    WriteFile(root / "Bin" / "WizardGraphicalClient.exe", "MZ-12");
    EXPECT_FALSE(watch.Poll(system)) << "a program still changing between polls is not yet an update";
    std::optional<ClientFingerprint> const settled = watch.Poll(system);
    ASSERT_TRUE(settled);
    EXPECT_EQ(settled->Revision, "r806919");
    EXPECT_EQ(settled->ExecutableSize, 5u);

    std::filesystem::remove(root / "Bin" / "revision.dat");
    EXPECT_FALSE(ClientFingerprint::Read(system, root)) << "an install whose revision cannot be read has no fingerprint";
    EXPECT_FALSE(watch.Poll(system));
    EXPECT_FALSE(watch.Poll(system));
}

TEST(ClientRevisionWatchTest, ARecordedExtractionIsCurrentOnlyForItsRevisionAndProgram)
{
    ClientExtractionRecord record;
    record.Kind = std::string(ClientExtractionScript::Zones);
    record.Revision = "r806919.Wizard_1_610";
    record.ExecutableSha256 = std::string(64, 'a');
    EXPECT_TRUE(record.IsFrom("r806919.Wizard_1_610", std::string(64, 'a')));
    EXPECT_TRUE(record.IsFrom("r806919.Wizard_1_610", "")) << "a program that cannot be hashed does not make the tables stale";
    EXPECT_FALSE(record.IsFrom("r806919.Wizard_1_610", std::string(64, 'b')));
    EXPECT_FALSE(record.IsFrom("r807500.Wizard_1_620", std::string(64, 'a')));

    WorldSqlScript const script = ClientExtractionScript::Build(ClientExtractionScript::Names, "r807500.Wizard_1_620", std::string(64, 'c'));
    std::string const text = script.ToText();
    EXPECT_NE(text.find("`client_extraction`"), std::string::npos) << text;
    EXPECT_NE(text.find(WorldSqlScript::Literal(std::string("names"))), std::string::npos) << text;
    EXPECT_NE(text.find(WorldSqlScript::Literal(std::string("r807500.Wizard_1_620"))), std::string::npos) << text;
}
