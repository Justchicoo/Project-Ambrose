/*
 * Project Ambrose by Imjustchico
 * Reads the plain-XML ObjectProperty files of the user's own install, when AMBROSE_CLIENT_DIR names it and AMBROSE_TYPE_DUMP_PATH its type dump: ActionList.xml and InputBindings.xml read with no issue into their action and binding lists, and CharacterCreationConfig.xml, Chatter.xml and Colors.xml read as well-formed documents whose root classes the r806919 dump does not list, which is reported rather than failing; with the classes the install holds that its dump does not describe, from the class file the extractor or the game server builds for it, all five read with no issue, the creation config with its creation options and seven schools, the chatter with its lists and the colors with their choices, as many as recorded for the installed revision, r806919's 24 and 14.
 */

#include "ClientLocator.h"
#include "ClientSystem.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "InstalledRevision.h"
#include "KiwadArchive.h"
#include "LogConfig.h"
#include "ServerClassCache.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "XmlObjectReader.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    class XmlObjectReaderClientTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
            if (!client || client->empty() || !dump || dump->empty())
                GTEST_SKIP() << "set AMBROSE_CLIENT_DIR to your own install and AMBROSE_TYPE_DUMP_PATH to its type dump to run this test";
            ASSERT_TRUE(_registry.LoadFromFile(LogConfig::Utf8Path(*dump))) << (_registry.GetErrors().empty() ? std::string() : _registry.GetErrors().front());
            std::string error;
            _archive = KiwadArchive::Open(LogConfig::Utf8Path(*client) / "Data" / "GameData" / "Root.wad", error);
            ASSERT_TRUE(_archive) << error;
        }

        XmlReadResult ReadEntry(std::string_view name)
        {
            KiwadReadResult const read = _archive->Read(name);
            EXPECT_TRUE(read.Succeeded()) << name << ": " << read.Error;
            return XmlObjectReader::Read(_registry.GetCatalog(), std::string_view(reinterpret_cast<char const*>(read.Data.data()), read.Data.size()));
        }

        TypeRegistry _registry;
        std::unique_ptr<KiwadArchive> _archive;
    };
}

TEST_F(XmlObjectReaderClientTest, ActionListAndInputBindingsReadWithNoIssue)
{
    for (auto const& [entry, rootClass, listProperty] : { std::tuple{ "ActionList.xml", "class ActionList", "m_actionList" }, std::tuple{ "InputBindings.xml", "class InputBindingList", "m_inputs" } })
    {
        XmlReadResult const read = ReadEntry(entry);
        ASSERT_TRUE(read.Ok()) << entry << ": " << read.Detail;
        for (DecodeIssue const& issue : read.Issues)
            ADD_FAILURE() << entry << ": " << ObjectSerializer::GetIssueName(issue.Kind) << " at " << issue.Path << ": " << issue.Detail;
        ASSERT_EQ(read.Objects.size(), 1u) << entry;
        EXPECT_EQ(read.Objects.front()->GetClass().Name, rootClass);
        PropertyValue const* const list = read.Objects.front()->Get(listProperty);
        ASSERT_NE(list, nullptr) << entry;
        EXPECT_GT(list->GetList()->size(), 50u) << entry;
    }
}

TEST_F(XmlObjectReaderClientTest, DocumentsWhoseRootClassTheDumpLacksAreReported)
{
    for (auto const& [entry, rootClass] : { std::pair{ "CharacterCreation/CharacterCreationConfig.xml", "class WizCharacterCreationConfig" }, std::pair{ "Chatter.xml", "class ChatterManager" }, std::pair{ "Colors.xml", "class ShoppingColors" } })
    {
        XmlReadResult const read = ReadEntry(entry);
        ASSERT_TRUE(read.Ok()) << entry << ": " << read.Detail;
        EXPECT_TRUE(read.Objects.empty()) << entry;
        ASSERT_EQ(read.Issues.size(), 1u) << entry;
        EXPECT_EQ(read.Issues.front().Kind, DecodeIssueKind::UnknownClass) << entry;
        EXPECT_EQ(read.Issues.front().Hash, StringHash::KiStringHash(rootClass)) << entry;
    }
}

TEST_F(XmlObjectReaderClientTest, WithTheClassesTheInstallHoldsCreationChatterAndColorsReadAsObjects)
{
    LocalClientSystem const system;
    std::optional<ClientInstall> const install = ClientInstall::Inspect(system, LogConfig::Utf8Path(*Ambrose::GetEnv("AMBROSE_CLIENT_DIR")));
    ASSERT_TRUE(install);
    std::optional<std::filesystem::path> const classes = ServerClassCache::PathFor(ClientLocator::GetDataFolder(system), install->Revision);
    if (!classes || !ServerClassCache::IsCurrent(*classes, LogConfig::Utf8Path(*Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH"))))
        GTEST_SKIP() << "run the extractor's classes command, or start the game server once, to build the class file for " << install->Describe();
    TypeDumpLoader::RawDump found;
    std::string error;
    ASSERT_TRUE(ServerClassCache::Read(*classes, found, error)) << error;
    std::vector<std::string> errors;
    ASSERT_TRUE(_registry.SetSupplement(std::move(found), ConfigMgr::PathToUtf8(*classes), errors)) << (errors.empty() ? std::string() : errors.front());

    for (std::string_view const entry : { "CharacterCreation/CharacterCreationConfig.xml", "Chatter.xml", "Colors.xml", "ActionList.xml", "InputBindings.xml" })
    {
        XmlReadResult const read = ReadEntry(entry);
        ASSERT_TRUE(read.Ok()) << entry << ": " << read.Detail;
        for (DecodeIssue const& issue : read.Issues)
            ADD_FAILURE() << entry << ": " << ObjectSerializer::GetIssueName(issue.Kind) << " at " << issue.Path << ": " << issue.Detail;
        ASSERT_EQ(read.Objects.size(), 1u) << entry;
    }

    XmlReadResult const creation = ReadEntry("CharacterCreation/CharacterCreationConfig.xml");
    ASSERT_EQ(creation.Objects.size(), 1u);
    PropertyObject const& config = *creation.Objects.front();
    EXPECT_EQ(config.GetClass().Name, "class WizCharacterCreationConfig");
    ASSERT_NE(config.Get("m_creationOptions")->GetList(), nullptr);
    EXPECT_FALSE(config.Get("m_creationOptions")->GetList()->empty());
    PropertyValue::List const* const schools = config.Get("m_schoolOptions")->GetList();
    ASSERT_NE(schools, nullptr);
    ASSERT_EQ(schools->size(), 7u);
    PropertyObject const* const first = (*schools)[0].AsObject();
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(*first->Get("m_schoolName")->GetIf<std::string>(), "Fire");

    XmlReadResult const chatter = ReadEntry("Chatter.xml");
    EXPECT_EQ(chatter.Objects.front()->GetClass().Name, "class ChatterManager");
    InstalledRevision::Expect(chatter.Objects.front()->Get("m_loadItems")->GetList()->size(), { { "r806919", 24u } }, "chatter lists");
    XmlReadResult const colors = ReadEntry("Colors.xml");
    EXPECT_EQ(colors.Objects.front()->GetClass().Name, "class ShoppingColors");
    InstalledRevision::Expect(colors.Objects.front()->Get("m_boysPrimary")->GetList()->size(), { { "r806919", 14u } }, "color choices");
}
