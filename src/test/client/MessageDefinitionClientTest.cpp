/*
 * Project Ambrose by Imjustchico
 * Tests every message protocol in the user's own Root.wad (r806919): counts, wire ids, type census, warnings, and a digest of the whole model.
 */

#include "Environment.h"
#include "Hex.h"
#include "KiwadArchive.h"
#include "LogConfig.h"
#include "MessageDefinitionSet.h"
#include "SHA256.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <stdexcept>

namespace
{
    class MessageDefinitionClientTest : public testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            std::optional<std::string> const directory = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            if (!directory || directory->empty())
                return;
            std::string error;
            std::unique_ptr<KiwadArchive> const root = KiwadArchive::Open(LogConfig::Utf8Path(*directory) / "Data" / "GameData" / "Root.wad", error);
            _loadError = error;
            if (!root)
                return;
            _set = std::make_unique<MessageDefinitionSet>();
            _loaded = _set->LoadFromArchive(*root);
        }

        static void TearDownTestSuite()
        {
            _set.reset();
        }

        void SetUp() override
        {
            std::optional<std::string> const directory = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            if (!directory || directory->empty())
                GTEST_SKIP() << "set AMBROSE_CLIENT_DIR to a Wizard101 install folder to run client data tests";
            ASSERT_NE(_set, nullptr) << _loadError;
        }

        static ProtocolDef const& Service(uint8 serviceId)
        {
            ProtocolDef const* const protocol = _set->FindService(serviceId);
            if (!protocol)
                throw std::runtime_error(fmt::format("service {} is missing", serviceId));
            return *protocol;
        }

        static MessageDef const& Get(uint8 serviceId, std::string_view tag)
        {
            MessageDef const* const message = _set->FindByTag(serviceId, tag);
            if (!message)
                throw std::runtime_error(fmt::format("service {} has no {}", serviceId, tag));
            return *message;
        }

        static inline std::unique_ptr<MessageDefinitionSet> _set;
        static inline std::string _loadError;
        static inline bool _loaded = false;
    };

    std::string Join(std::vector<MessageIssue> const& issues)
    {
        std::string text;
        for (MessageIssue const& issue : issues)
            text += issue.ToString() + "\n";
        return text;
    }
}

TEST_F(MessageDefinitionClientTest, LoadsEveryProtocolWithoutErrors)
{
    EXPECT_TRUE(_loaded);
    EXPECT_TRUE(_set->GetErrors().empty()) << Join(_set->GetErrors());
    EXPECT_EQ(_set->GetProtocols().size(), 29u);
    EXPECT_EQ(_set->GetRecordCount(), 1448u);
    EXPECT_EQ(_set->GetMessageCount(), 1446u);
}

TEST_F(MessageDefinitionClientTest, IdsPerServiceMatchTheClient)
{
    std::map<uint8, std::size_t> const expected{
        { 1, 2 }, { 2, 6 }, { 5, 253 }, { 7, 29 }, { 8, 3 }, { 9, 55 }, { 10, 20 }, { 11, 2 }, { 12, 253 }, { 15, 4 },
        { 16, 6 }, { 19, 1 }, { 25, 23 }, { 40, 3 }, { 41, 3 }, { 42, 3 }, { 43, 3 }, { 44, 3 }, { 45, 3 }, { 46, 3 },
        { 47, 3 }, { 50, 212 }, { 51, 36 }, { 52, 19 }, { 53, 254 }, { 54, 3 }, { 55, 10 }, { 56, 213 }, { 57, 18 }
    };
    std::map<uint8, std::size_t> actual;
    for (auto const& [serviceId, protocol] : _set->GetProtocols())
    {
        actual[serviceId] = protocol.Messages.size();
        bool const explicitIds = serviceId == 1 || serviceId == 7 || serviceId == 8 || serviceId == 11;
        EXPECT_EQ(protocol.Ordering, explicitIds ? MessageOrdering::Explicit : MessageOrdering::SortedByTag) << protocol.SourceFile;
        EXPECT_EQ(protocol.Version, 1) << protocol.SourceFile;
        EXPECT_EQ(protocol.RecordCount, protocol.Messages.size() + ((serviceId == 5 || serviceId == 12) ? 1u : 0u)) << protocol.SourceFile;
    }
    EXPECT_EQ(actual, expected);
    EXPECT_EQ(Service(5).SourceFile, "GameMessages.xml");
    EXPECT_EQ(Service(51).ProtocolType, "DOODLEDOUG_MESSAGES");
    EXPECT_EQ(Service(41).ProtocolType, "DOODLEDOUG_MESSAGES");
    EXPECT_EQ(Service(44).ProtocolType, "MG3_MESSAGES");
    EXPECT_EQ(Service(54).ProtocolType, "MG3_MESSAGES");
    EXPECT_EQ(Service(56).SourceFile, "WizardMessages3.xml");
    EXPECT_EQ(_set->FindService(3), nullptr);
}

TEST_F(MessageDefinitionClientTest, OrdinalSpotChecks)
{
    EXPECT_EQ(Get(1, "MSG_PING").Order, 1);
    EXPECT_EQ(Get(1, "MSG_PING_RSP").Order, 2);
    EXPECT_EQ(Get(2, "MSG_CUSTOMDICT").Order, 1);
    EXPECT_EQ(Get(2, "MSG_CUSTOMRECORD").Order, 2);
    EXPECT_EQ(Get(2, "MSG_FORCE_DISCONNECT").Order, 3);
    EXPECT_EQ(Get(2, "MSG_RAWRECORD").Order, 4);
    EXPECT_EQ(Get(2, "MSG_RAW_TEXT").Order, 5);
    EXPECT_EQ(Get(2, "MSG_SERVERMESSAGE").Order, 6);
    EXPECT_EQ(Get(7, "MSG_CHARACTERSELECTED").Order, 3);
    EXPECT_EQ(Get(7, "MSG_SELECTCHARACTER").Order, 10);
    EXPECT_EQ(Get(7, "MSG_USER_AUTHEN_V3").Order, 27);
    EXPECT_EQ(Get(8, "MSG_LATEST_FILE_LIST_V2").Order, 2);
    EXPECT_EQ(Get(5, "MSG_ATTACH").Order, 7);
    EXPECT_EQ(Get(5, "MSG_ATTACHFAILED").Order, 8);
    EXPECT_EQ(Get(5, "MSG_BADGES").Order, 10);
    EXPECT_EQ(Get(5, "MSG_CLIENTMOVE").Order, 36);
    EXPECT_EQ(Get(5, "MSG_LOGINCOMPLETE").Order, 108);
    EXPECT_EQ(Get(5, "MSG_NEWOBJECT").Order, 122);
    EXPECT_EQ(Get(5, "MSG_REMOVEOBJECT").Order, 182);
    EXPECT_EQ(Get(5, "MSG_REMOVEOBJECT").RecordCount, 2u);
    EXPECT_EQ(Get(5, "MSG_SERVER_ERROR").Order, 223);
    EXPECT_EQ(Get(5, "MSG_SERVER_ERROR").Name, "MSG_SERVERERROR");
    EXPECT_EQ(Get(12, "MSG_UPDATEGOLD").Order, 231);
    EXPECT_EQ(Get(12, "MSG_UPDATEMANA").Order, 233);
    EXPECT_EQ(Get(12, "MSG_MINIGAMEREWARDS").Order, 92);
    EXPECT_EQ(Get(12, "MSG_PETHATCHREADYSTATUS").Order, 122);
    EXPECT_EQ(Get(12, "MSG_PETHATCHREADYSTATUS").RecordCount, 2u);
    EXPECT_EQ(Get(16, "MSG_PHYSICS_GRAB").Order, 3);
    EXPECT_EQ(Get(53, "MSG_CLIENTZONED").Order, 64);
    EXPECT_EQ(Get(52, "MSG_ACCEPTQUEST").Order, 1);
    EXPECT_EQ(_set->Find(5, 7)->Tag, "MSG_ATTACH");
    EXPECT_EQ(_set->Find(7, 27)->Tag, "MSG_USER_AUTHEN_V3");
}

TEST_F(MessageDefinitionClientTest, FieldTypeCensusHasExactlyNineTypes)
{
    std::map<DmlType, std::size_t> const expected{
        { DmlType::Gid, 1193 }, { DmlType::Str, 911 }, { DmlType::Uint, 740 }, { DmlType::Int, 530 }, { DmlType::Ubyt, 459 },
        { DmlType::Flt, 241 }, { DmlType::Byt, 172 }, { DmlType::Wstr, 38 }, { DmlType::Ushrt, 27 }
    };
    EXPECT_EQ(_set->GetTypeCensus(), expected);
    EXPECT_EQ(_set->GetFieldCount(), 4311u);

    std::size_t recordFields = 0;
    std::size_t defaults = 0;
    std::size_t accessLevels = 0;
    std::size_t empty = 0;
    std::size_t renamed = 0;
    for (auto const& [serviceId, protocol] : _set->GetProtocols())
    {
        for (MessageDef const& message : protocol.Messages)
        {
            recordFields += message.Fields.size() * message.RecordCount;
            accessLevels += message.AccessLevel.has_value() ? 1 : 0;
            empty += message.Fields.empty() ? 1 : 0;
            renamed += message.Name != message.Tag ? 1 : 0;
            for (FieldDef const& field : message.Fields)
                defaults += field.DefaultValue.has_value() ? 1 : 0;
        }
    }
    EXPECT_EQ(recordFields, 4315u);
    EXPECT_EQ(defaults, 10u);
    EXPECT_EQ(accessLevels, 14u);
    EXPECT_EQ(empty, 130u);
    EXPECT_EQ(renamed, 9u);
    FieldDef const* const crowns = Get(12, "MSG_CROWNBALANCE").FindField("TotalCrowns");
    ASSERT_NE(crowns, nullptr);
    EXPECT_EQ(crowns->DefaultValue, std::optional<std::string>("0"));
}

TEST_F(MessageDefinitionClientTest, ExactlyThreeWarnings)
{
    std::vector<MessageIssue> const& warnings = _set->GetWarnings();
    ASSERT_EQ(warnings.size(), 3u) << Join(warnings);
    EXPECT_EQ(warnings[0].SourceFile, "Messages/PhysicsBehaviorMessages.xml");
    EXPECT_NE(warnings[0].Message.find("MSG_PHYSICS_GRAB.Force spells TYPE as TPYE"), std::string::npos) << warnings[0].ToString();
    EXPECT_EQ(warnings[1].SourceFile, "WizardMessages.xml");
    EXPECT_NE(warnings[1].Message.find("MSG_MINIGAMEREWARDS.GlobalID has no TYPE"), std::string::npos) << warnings[1].ToString();
    EXPECT_EQ(warnings[2].SourceFile, "WizardMessages2.xml");
    EXPECT_NE(warnings[2].Message.find("MSG_BATTLEGROUNDQUEUEUPDATE.Kicked spells TYPE as TYP"), std::string::npos) << warnings[2].ToString();
    for (MessageIssue const& warning : warnings)
        EXPECT_GT(warning.Line, 0u) << warning.ToString();

    FieldDef const* const force = Get(16, "MSG_PHYSICS_GRAB").FindField("Force");
    ASSERT_NE(force, nullptr);
    EXPECT_EQ(force->Type, DmlType::Flt);
    EXPECT_EQ(force->TypeSource, FieldTypeSource::MisspelledTpye);
    FieldDef const* const globalId = Get(12, "MSG_MINIGAMEREWARDS").FindField("GlobalID");
    ASSERT_NE(globalId, nullptr);
    EXPECT_EQ(globalId->Type, DmlType::Gid);
    EXPECT_EQ(globalId->TypeSource, FieldTypeSource::InferredGlobalId);
    FieldDef const* const kicked = Get(53, "MSG_BATTLEGROUNDQUEUEUPDATE").FindField("Kicked");
    ASSERT_NE(kicked, nullptr);
    EXPECT_EQ(kicked->Type, DmlType::Str);
    EXPECT_EQ(kicked->TypeSource, FieldTypeSource::MisspelledTyp);
}

TEST_F(MessageDefinitionClientTest, SortingByMsgNameWouldBreakGameIds)
{
    ProtocolDef const* const game = _set->FindService(5);
    ASSERT_NE(game, nullptr);
    std::vector<MessageDef const*> byName;
    for (MessageDef const& message : game->Messages)
        byName.push_back(&message);
    std::sort(byName.begin(), byName.end(), [](MessageDef const* left, MessageDef const* right) { return MessageDefinitionParser::TagLess(left->Name, right->Name); });
    std::size_t differing = 0;
    for (std::size_t i = 0; i < byName.size(); ++i)
        differing += byName[i]->Tag != game->Messages[i].Tag ? 1 : 0;
    EXPECT_EQ(differing, 191u);
}

TEST_F(MessageDefinitionClientTest, WholeModelDigestMatchesTheReference)
{
    SHA256 hash;
    for (auto const& [serviceId, protocol] : _set->GetProtocols())
    {
        for (MessageDef const& message : protocol.Messages)
        {
            std::string line = fmt::format("{}:{}:{}:", serviceId, message.Order, message.Tag);
            for (std::size_t i = 0; i < message.Fields.size(); ++i)
                line += fmt::format("{}{}={}", i == 0 ? "" : ",", message.Fields[i].Name, Dml::GetTypeName(message.Fields[i].Type));
            hash.Update(line + ";");
        }
    }
    SHA256::Digest const digest = hash.Finalize();
    EXPECT_EQ(Hex::Encode(digest), "c60bc78799a16f2c4ca2537b2e4482166364989b25061659b13667a94f0a5211");
}
