/*
 * Project Ambrose by Imjustchico
 * Reads the plain-XML ObjectProperty files of the user's own r806919 install, when AMBROSE_CLIENT_DIR names it and AMBROSE_TYPE_DUMP_PATH its type dump: ActionList.xml and InputBindings.xml read with no issue into their action and binding lists, and CharacterCreationConfig.xml, Chatter.xml and Colors.xml read as well-formed documents whose root classes the r806919 dump does not list, which is reported rather than failing.
 */

#include "Environment.h"
#include "KiwadArchive.h"
#include "LogConfig.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "XmlObjectReader.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

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
                GTEST_SKIP() << "set AMBROSE_CLIENT_DIR to your own r806919 install and AMBROSE_TYPE_DUMP_PATH to its type dump to run this test";
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
