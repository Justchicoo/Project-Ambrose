/*
 * Project Ambrose by Imjustchico
 * Tests message XML parsing on Ambrose-authored fixtures: explicit and tag-sorted orders, typos, validation errors, and archive loading.
 */

#include "KiwadArchive.h"
#include "KiwadBuilder.h"
#include "LogTestDirectory.h"
#include "MessageDefinitionSet.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <string>

namespace
{
    std::string Document(std::string_view body, std::string_view serviceId = "5", std::string_view protocolType = "TEST_MESSAGES")
    {
        return fmt::format("<?xml version=\"1.0\" ?>\n<TestMessages>\n<_ProtocolInfo><RECORD><ServiceID TYPE=\"UBYT\">{}</ServiceID><ProtocolType TYPE=\"STR\">{}</ProtocolType>"
                           "<ProtocolVersion TYPE=\"INT\">3</ProtocolVersion><ProtocolDescription TYPE=\"STR\">Fixture protocol</ProtocolDescription></RECORD></_ProtocolInfo>\n{}</TestMessages>\n",
            serviceId, protocolType, body);
    }

    std::string Message(std::string_view tag, std::string_view fields = "", std::string_view metadata = "")
    {
        return fmt::format("<{0}><RECORD>{2}{1}</RECORD></{0}>\n", tag, fields, metadata);
    }

    std::string Order(std::string_view element, int value)
    {
        return fmt::format("<{0} TYPE=\"UBYT\" NOXFER=\"TRUE\">{1}</{0}>", element, value);
    }

    std::vector<std::string> Tags(ProtocolDef const& protocol)
    {
        std::vector<std::string> tags;
        for (MessageDef const& message : protocol.Messages)
            tags.push_back(message.Tag);
        return tags;
    }

    bool AnyIssueContains(std::vector<MessageIssue> const& issues, std::string_view text)
    {
        return std::any_of(issues.begin(), issues.end(), [text](MessageIssue const& issue) { return issue.Message.find(text) != std::string::npos; });
    }

    std::string JoinIssues(std::vector<MessageIssue> const& issues)
    {
        std::string text;
        for (MessageIssue const& issue : issues)
            text += issue.ToString() + "\n";
        return text;
    }
}

TEST(MessageDefinitionParserTest, ExplicitMsgOrderKeepsDeclaredIds)
{
    std::string const body =
        Message("MSG_THIRD", "<Value TYPE=\"UINT\"></Value>", Order("_MsgOrder", 3) + "<_MsgName TYPE=\"STR\" NOXFER=\"TRUE\">MSG_THIRD</_MsgName>") +
        Message("MSG_FIRST", "<Text TYPE=\"STR\"></Text>", Order("_MsgOrder", 1) + "<_MsgAccessLvl TYPE=\"UBYT\" NOXFER=\"TRUE\">4</_MsgAccessLvl>") +
        Message("MSG_SECOND", "", Order("_MsgOrder", 2) + "<_MsgHandler TYPE=\"STR\" NOXFER=\"TRUE\">MSG_Second</_MsgHandler><_MsgDescription TYPE=\"STR\" NOXFER=\"TRUE\">Second message</_MsgDescription>");
    MessageParseResult const result = MessageDefinitionParser::Parse(Document(body, "7", "LOGIN_MESSAGES"), "LoginFixture.xml");
    ASSERT_TRUE(result.Succeeded()) << JoinIssues(result.Errors);
    EXPECT_TRUE(result.Warnings.empty()) << JoinIssues(result.Warnings);

    ProtocolDef const& protocol = *result.Protocol;
    EXPECT_EQ(protocol.ServiceId, 7);
    EXPECT_EQ(protocol.ProtocolType, "LOGIN_MESSAGES");
    EXPECT_EQ(protocol.Version, 3);
    EXPECT_EQ(protocol.Description, "Fixture protocol");
    EXPECT_EQ(protocol.SourceFile, "LoginFixture.xml");
    EXPECT_EQ(protocol.RootElement, "TestMessages");
    EXPECT_EQ(protocol.Ordering, MessageOrdering::Explicit);
    EXPECT_EQ(protocol.RecordCount, 3u);
    EXPECT_EQ(Tags(protocol), (std::vector<std::string>{ "MSG_FIRST", "MSG_SECOND", "MSG_THIRD" }));

    MessageDef const* const first = protocol.FindByOrder(1);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->Tag, "MSG_FIRST");
    EXPECT_EQ(first->AccessLevel, std::optional<uint8>(4));
    EXPECT_TRUE(first->HasVariableSize());
    MessageDef const* const second = protocol.FindByTag("MSG_SECOND");
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->Order, 2);
    EXPECT_EQ(second->Handler, "MSG_Second");
    EXPECT_EQ(second->Description, "Second message");
    EXPECT_FALSE(second->AccessLevel.has_value());
    EXPECT_TRUE(second->Fields.empty());
    EXPECT_EQ(second->GetFixedSize(), 0u);
    EXPECT_FALSE(second->HasVariableSize());
    EXPECT_EQ(protocol.FindByOrder(3)->Name, "MSG_THIRD");
    EXPECT_EQ(protocol.FindByOrder(3)->GetFixedSize(), 4u);
    EXPECT_EQ(protocol.FindByOrder(4), nullptr);
    EXPECT_EQ(protocol.FindByOrder(0), nullptr);
    EXPECT_EQ(protocol.FindByTag("MSG_FOURTH"), nullptr);
}

TEST(MessageDefinitionParserTest, ExplicitMsgTypeAlsoNumbersMessages)
{
    std::string const body = Message("MSG_PONG", "", Order("_MsgType", 2)) + Message("MSG_PING", "", Order("_MsgType", 1)) + Message("MSG_REPORT", "", Order("_MsgType", 100));
    MessageParseResult const result = MessageDefinitionParser::Parse(Document(body, "1", "SYSTEM"), "SystemFixture.xml");
    ASSERT_TRUE(result.Succeeded()) << JoinIssues(result.Errors);
    EXPECT_EQ(result.Protocol->Ordering, MessageOrdering::Explicit);
    EXPECT_EQ(result.Protocol->FindByOrder(1)->Tag, "MSG_PING");
    EXPECT_EQ(result.Protocol->FindByOrder(2)->Tag, "MSG_PONG");
    EXPECT_EQ(result.Protocol->FindByOrder(100)->Tag, "MSG_REPORT");
    EXPECT_EQ(result.Protocol->FindByOrder(3), nullptr);
}

TEST(MessageDefinitionParserTest, SortedFileNumbersByteOrdinalTagsAndMergesDuplicates)
{
    std::string const body =
        Message("MSG_ZED", "<Id TYPE=\"GID\"></Id>", "<_MsgName TYPE=\"STR\" NOXFER=\"TRUE\">MSG_AAA</_MsgName>") +
        Message("MSG_DailyQuestUpdate", "<Count TYPE=\"INT\">0</Count>") +
        Message("MSG_REMOVE", "<Target TYPE=\"GID\"></Target>", "<_MsgDescription TYPE=\"STR\" NOXFER=\"TRUE\">First copy</_MsgDescription>") +
        Message("MSG_DUEL") +
        Message("MSG_A_B") +
        Message("MSG_AB", "", "<_MsgName TYPE=\"STR\" NOXFER=\"TRUE\">MSG_ZZZ</_MsgName>") +
        Message("MSG_REMOVE", "<Target TYPE=\"GID\"></Target>", "<_MsgDescription TYPE=\"STR\" NOXFER=\"TRUE\">Second copy</_MsgDescription>");
    MessageParseResult const result = MessageDefinitionParser::Parse(Document(body), "GameFixture.xml");
    ASSERT_TRUE(result.Succeeded()) << JoinIssues(result.Errors);
    EXPECT_TRUE(result.Warnings.empty()) << JoinIssues(result.Warnings);

    ProtocolDef const& protocol = *result.Protocol;
    EXPECT_EQ(protocol.Ordering, MessageOrdering::SortedByTag);
    EXPECT_EQ(protocol.RecordCount, 7u);
    EXPECT_EQ(Tags(protocol), (std::vector<std::string>{ "MSG_AB", "MSG_A_B", "MSG_DUEL", "MSG_DailyQuestUpdate", "MSG_REMOVE", "MSG_ZED" }));
    for (std::size_t i = 0; i < protocol.Messages.size(); ++i)
        EXPECT_EQ(static_cast<std::size_t>(protocol.Messages[i].Order), i + 1) << protocol.Messages[i].Tag;

    MessageDef const* const removed = protocol.FindByTag("MSG_REMOVE");
    ASSERT_NE(removed, nullptr);
    EXPECT_EQ(removed->RecordCount, 2u);
    EXPECT_EQ(removed->Description, "First copy");
    EXPECT_EQ(protocol.FindByTag("MSG_ZED")->Name, "MSG_AAA");
    EXPECT_EQ(protocol.FindByTag("MSG_ZED")->Order, 6);
    FieldDef const* const count = protocol.FindByTag("MSG_DailyQuestUpdate")->FindField("Count");
    ASSERT_NE(count, nullptr);
    EXPECT_EQ(count->DefaultValue, std::optional<std::string>("0"));
    EXPECT_EQ(protocol.FindByTag("MSG_DailyQuestUpdate")->FindField("Missing"), nullptr);
    EXPECT_FALSE(protocol.FindByTag("MSG_ZED")->FindField("Id")->DefaultValue.has_value());
}

TEST(MessageDefinitionParserTest, TagOrderIsNotMsgNameOrder)
{
    std::string const body =
        Message("MSG_ALPHA", "", "<_MsgName TYPE=\"STR\" NOXFER=\"TRUE\">MSG_ZULU</_MsgName>") +
        Message("MSG_BRAVO", "", "<_MsgName TYPE=\"STR\" NOXFER=\"TRUE\">MSG_YANKEE</_MsgName>") +
        Message("MSG_CHARLIE", "", "<_MsgName TYPE=\"STR\" NOXFER=\"TRUE\">MSG_XRAY</_MsgName>");
    MessageParseResult const result = MessageDefinitionParser::Parse(Document(body), "NameFixture.xml");
    ASSERT_TRUE(result.Succeeded()) << JoinIssues(result.Errors);
    EXPECT_EQ(result.Protocol->FindByOrder(1)->Tag, "MSG_ALPHA");
    EXPECT_EQ(result.Protocol->FindByOrder(1)->Name, "MSG_ZULU");
    EXPECT_EQ(result.Protocol->FindByOrder(3)->Tag, "MSG_CHARLIE");
}

TEST(MessageDefinitionParserTest, TypoAttributesAndUntypedGlobalIdWarn)
{
    std::string const body =
        Message("MSG_GRAB", "<Target TYPE=\"GID\"></Target><Force TPYE=\"FLT\"></Force>") +
        Message("MSG_QUEUE", "<Kicked TYP=\"STR\"></Kicked>") +
        Message("MSG_REWARDS", "<GlobalID></GlobalID><Gold TYPE=\"INT\"></Gold>") +
        Message("MSG_FLAGS", "<Flag TYPE=\"BOOL\"></Flag>");
    MessageParseResult const result = MessageDefinitionParser::Parse(Document(body), "TypoFixture.xml");
    ASSERT_TRUE(result.Succeeded()) << JoinIssues(result.Errors);
    ASSERT_EQ(result.Warnings.size(), 4u) << JoinIssues(result.Warnings);
    EXPECT_TRUE(AnyIssueContains(result.Warnings, "MSG_GRAB.Force spells TYPE as TPYE"));
    EXPECT_TRUE(AnyIssueContains(result.Warnings, "MSG_QUEUE.Kicked spells TYPE as TYP"));
    EXPECT_TRUE(AnyIssueContains(result.Warnings, "MSG_REWARDS.GlobalID has no TYPE"));
    EXPECT_TRUE(AnyIssueContains(result.Warnings, "MSG_FLAGS.Flag uses the type alias 'BOOL'"));
    EXPECT_EQ(result.Warnings.front().SourceFile, "TypoFixture.xml");
    EXPECT_EQ(result.Warnings.front().Line, 4u);

    ProtocolDef const& protocol = *result.Protocol;
    FieldDef const* const force = protocol.FindByTag("MSG_GRAB")->FindField("Force");
    EXPECT_EQ(force->Type, DmlType::Flt);
    EXPECT_EQ(force->TypeSource, FieldTypeSource::MisspelledTpye);
    EXPECT_EQ(protocol.FindByTag("MSG_GRAB")->GetFixedSize(), 12u);
    FieldDef const* const kicked = protocol.FindByTag("MSG_QUEUE")->FindField("Kicked");
    EXPECT_EQ(kicked->Type, DmlType::Str);
    EXPECT_EQ(kicked->TypeSource, FieldTypeSource::MisspelledTyp);
    FieldDef const* const globalId = protocol.FindByTag("MSG_REWARDS")->FindField("GlobalID");
    EXPECT_EQ(globalId->Type, DmlType::Gid);
    EXPECT_EQ(globalId->TypeSource, FieldTypeSource::InferredGlobalId);
    EXPECT_EQ(protocol.FindByTag("MSG_REWARDS")->Fields.front().Name, "GlobalID");
    FieldDef const* const flag = protocol.FindByTag("MSG_FLAGS")->FindField("Flag");
    EXPECT_EQ(flag->Type, DmlType::Ubyt);
    EXPECT_EQ(flag->TypeSource, FieldTypeSource::Alias);
    EXPECT_EQ(protocol.FindByTag("MSG_GRAB")->FindField("Target")->TypeSource, FieldTypeSource::Declared);
}

TEST(MessageDefinitionParserTest, MissingOrUnknownTypesAreErrors)
{
    MessageParseResult const untyped = MessageDefinitionParser::Parse(Document(Message("MSG_BAD", "<Value></Value>")), "Untyped.xml");
    EXPECT_FALSE(untyped.Succeeded());
    EXPECT_FALSE(untyped.Protocol.has_value());
    EXPECT_TRUE(AnyIssueContains(untyped.Errors, "MSG_BAD.Value has no TYPE attribute")) << JoinIssues(untyped.Errors);

    MessageParseResult const unknown = MessageDefinitionParser::Parse(Document(Message("MSG_BAD", "<Value TYPE=\"QWORD\"></Value>")), "Unknown.xml");
    EXPECT_FALSE(unknown.Succeeded());
    EXPECT_TRUE(AnyIssueContains(unknown.Errors, "unknown type 'QWORD'")) << JoinIssues(unknown.Errors);
    EXPECT_EQ(unknown.Errors.front().Line, 4u);

    MessageParseResult const lowercase = MessageDefinitionParser::Parse(Document(Message("MSG_BAD", "<Value TYPE=\"int\"></Value>")), "Lowercase.xml");
    EXPECT_FALSE(lowercase.Succeeded());

    MessageParseResult const duplicateField = MessageDefinitionParser::Parse(Document(Message("MSG_BAD", "<Value TYPE=\"INT\"></Value><Value TYPE=\"INT\"></Value>")), "DuplicateField.xml");
    EXPECT_FALSE(duplicateField.Succeeded());
    EXPECT_TRUE(AnyIssueContains(duplicateField.Errors, "declares field Value more than once")) << JoinIssues(duplicateField.Errors);

    MessageParseResult const nested = MessageDefinitionParser::Parse(Document(Message("MSG_BAD", "<Value TYPE=\"INT\"><Inner/></Value>")), "Nested.xml");
    EXPECT_FALSE(nested.Succeeded());
    EXPECT_TRUE(AnyIssueContains(nested.Errors, "nested element <Inner>")) << JoinIssues(nested.Errors);
}

TEST(MessageDefinitionParserTest, ExplicitOrderErrors)
{
    MessageParseResult const duplicate = MessageDefinitionParser::Parse(Document(Message("MSG_ONE", "", Order("_MsgOrder", 1)) + Message("MSG_TWO", "", Order("_MsgOrder", 1))), "Duplicate.xml");
    EXPECT_FALSE(duplicate.Succeeded());
    EXPECT_TRUE(AnyIssueContains(duplicate.Errors, "both use order 1")) << JoinIssues(duplicate.Errors);

    MessageParseResult const tooLarge = MessageDefinitionParser::Parse(Document(Message("MSG_ONE", "", Order("_MsgOrder", 256))), "TooLarge.xml");
    EXPECT_FALSE(tooLarge.Succeeded());
    EXPECT_TRUE(AnyIssueContains(tooLarge.Errors, "order 256")) << JoinIssues(tooLarge.Errors);

    MessageParseResult const zero = MessageDefinitionParser::Parse(Document(Message("MSG_ONE", "", Order("_MsgOrder", 0))), "Zero.xml");
    EXPECT_FALSE(zero.Succeeded());

    MessageParseResult const missing = MessageDefinitionParser::Parse(Document(Message("MSG_ONE", "", Order("_MsgOrder", 1)) + Message("MSG_TWO")), "Missing.xml");
    EXPECT_FALSE(missing.Succeeded());
    EXPECT_TRUE(AnyIssueContains(missing.Errors, "MSG_TWO has no _MsgOrder or _MsgType")) << JoinIssues(missing.Errors);

    MessageParseResult const notNumber = MessageDefinitionParser::Parse(Document(Message("MSG_ONE", "", "<_MsgOrder TYPE=\"UBYT\">first</_MsgOrder>")), "NotNumber.xml");
    EXPECT_FALSE(notNumber.Succeeded());
    EXPECT_TRUE(AnyIssueContains(notNumber.Errors, "is not a number")) << JoinIssues(notNumber.Errors);

    MessageParseResult const reordered = MessageDefinitionParser::Parse(Document(Message("MSG_ONE", "", Order("_MsgOrder", 1)) + Message("MSG_ONE", "", Order("_MsgOrder", 2))), "Reordered.xml");
    EXPECT_FALSE(reordered.Succeeded());
    EXPECT_TRUE(AnyIssueContains(reordered.Errors, "with a different order")) << JoinIssues(reordered.Errors);

    MessageParseResult const repeated = MessageDefinitionParser::Parse(Document(Message("MSG_ONE", "", Order("_MsgOrder", 1)) + Message("MSG_ONE", "", Order("_MsgOrder", 1))), "Repeated.xml");
    ASSERT_TRUE(repeated.Succeeded()) << JoinIssues(repeated.Errors);
    EXPECT_EQ(repeated.Protocol->Messages.size(), 1u);
    EXPECT_EQ(repeated.Protocol->Messages.front().RecordCount, 2u);
}

TEST(MessageDefinitionParserTest, SortedFileLimitsAndLayoutConflicts)
{
    std::string body;
    for (int i = 0; i < 255; ++i)
        body += Message(fmt::format("MSG_{:03}", i));
    MessageParseResult const full = MessageDefinitionParser::Parse(Document(body), "Full.xml");
    ASSERT_TRUE(full.Succeeded()) << JoinIssues(full.Errors);
    EXPECT_EQ(full.Protocol->Messages.size(), 255u);
    EXPECT_EQ(full.Protocol->FindByOrder(255)->Tag, "MSG_254");

    MessageParseResult const over = MessageDefinitionParser::Parse(Document(body + Message("MSG_255")), "Over.xml");
    EXPECT_FALSE(over.Succeeded());
    EXPECT_TRUE(AnyIssueContains(over.Errors, "256 distinct messages exceed the 255")) << JoinIssues(over.Errors);

    MessageParseResult const duplicateOnly = MessageDefinitionParser::Parse(Document(body + Message("MSG_000")), "DuplicateOnly.xml");
    ASSERT_TRUE(duplicateOnly.Succeeded()) << JoinIssues(duplicateOnly.Errors);
    EXPECT_EQ(duplicateOnly.Protocol->RecordCount, 256u);

    MessageParseResult const conflict = MessageDefinitionParser::Parse(Document(Message("MSG_ONE", "<A TYPE=\"INT\"></A>") + Message("MSG_ONE", "<A TYPE=\"UINT\"></A>")), "Conflict.xml");
    EXPECT_FALSE(conflict.Succeeded());
    EXPECT_TRUE(AnyIssueContains(conflict.Errors, "was first defined on line 4 with different fields")) << JoinIssues(conflict.Errors);
    EXPECT_EQ(conflict.Errors.front().Line, 5u);

    MessageParseResult const stray = MessageDefinitionParser::Parse(Document(Message("MSG_ONE") + Message("MSG_TWO", "", Order("_MsgOrder", 9))), "Stray.xml");
    ASSERT_TRUE(stray.Succeeded()) << JoinIssues(stray.Errors);
    EXPECT_EQ(stray.Protocol->FindByTag("MSG_TWO")->Order, 2);
    EXPECT_TRUE(AnyIssueContains(stray.Warnings, "MSG_TWO has an explicit order")) << JoinIssues(stray.Warnings);
}

TEST(MessageDefinitionParserTest, ProtocolInfoAndStructureErrors)
{
    std::string const noInfo = "<?xml version=\"1.0\" ?>\n<TestMessages>\n" + Message("MSG_ONE") + "</TestMessages>\n";
    MessageParseResult const missingInfo = MessageDefinitionParser::Parse(noInfo, "NoInfo.xml");
    EXPECT_FALSE(missingInfo.Succeeded());
    EXPECT_TRUE(AnyIssueContains(missingInfo.Errors, "no _ProtocolInfo RECORD")) << JoinIssues(missingInfo.Errors);

    EXPECT_FALSE(MessageDefinitionParser::Parse(Document(Message("MSG_ONE"), "256"), "BigService.xml").Succeeded());
    EXPECT_FALSE(MessageDefinitionParser::Parse(Document(Message("MSG_ONE"), "-1"), "NegativeService.xml").Succeeded());
    EXPECT_FALSE(MessageDefinitionParser::Parse(Document(Message("MSG_ONE"), "five"), "WordService.xml").Succeeded());
    MessageParseResult const trimmed = MessageDefinitionParser::Parse(Document(Message("MSG_ONE"), " 255 "), "Trimmed.xml");
    ASSERT_TRUE(trimmed.Succeeded()) << JoinIssues(trimmed.Errors);
    EXPECT_EQ(trimmed.Protocol->ServiceId, 255);

    std::string const badVersion = "<Root><_ProtocolInfo><RECORD><ServiceID>9</ServiceID><ProtocolVersion>v2</ProtocolVersion></RECORD></_ProtocolInfo>" + Message("MSG_ONE") + "</Root>";
    MessageParseResult const version = MessageDefinitionParser::Parse(badVersion, "BadVersion.xml");
    EXPECT_FALSE(version.Succeeded());
    EXPECT_TRUE(AnyIssueContains(version.Errors, "ProtocolVersion 'v2'")) << JoinIssues(version.Errors);

    std::string const minimal = "<Anything><_ProtocolInfo><RECORD><ServiceID>9</ServiceID></RECORD></_ProtocolInfo>" + Message("MSG_ONE") + "</Anything>";
    MessageParseResult const bare = MessageDefinitionParser::Parse(minimal, "Minimal.xml");
    ASSERT_TRUE(bare.Succeeded()) << JoinIssues(bare.Errors);
    EXPECT_EQ(bare.Protocol->RootElement, "Anything");
    EXPECT_EQ(bare.Protocol->Version, 0);
    EXPECT_TRUE(bare.Protocol->ProtocolType.empty());

    EXPECT_FALSE(MessageDefinitionParser::Parse(Document(""), "Empty.xml").Succeeded());
    EXPECT_FALSE(MessageDefinitionParser::Parse(Document("<MSG_ONE></MSG_ONE>\n"), "NoRecord.xml").Succeeded());
    EXPECT_FALSE(MessageDefinitionParser::Parse(Document("<MSG_ONE><RECORD></RECORD><RECORD></RECORD></MSG_ONE>\n"), "TwoRecords.xml").Succeeded());
    EXPECT_FALSE(MessageDefinitionParser::Parse(Document("<MSG_ONE><FIELDS></FIELDS></MSG_ONE>\n"), "WrongChild.xml").Succeeded());
    EXPECT_FALSE(MessageDefinitionParser::Parse(Document(Message("MSG_ONE", "", "<_MsgAccessLvl>300</_MsgAccessLvl>")), "BadAccess.xml").Succeeded());

    MessageParseResult const unknownMetadata = MessageDefinitionParser::Parse(Document(Message("MSG_ONE", "", "<_MsgColor>blue</_MsgColor>")), "UnknownMetadata.xml");
    ASSERT_TRUE(unknownMetadata.Succeeded()) << JoinIssues(unknownMetadata.Errors);
    EXPECT_TRUE(AnyIssueContains(unknownMetadata.Warnings, "unknown metadata <_MsgColor>")) << JoinIssues(unknownMetadata.Warnings);

    MessageParseResult const noTransfer = MessageDefinitionParser::Parse(Document(Message("MSG_ONE", "<Value TYPE=\"INT\" NOXFER=\"TRUE\"></Value>")), "NoTransfer.xml");
    ASSERT_TRUE(noTransfer.Succeeded()) << JoinIssues(noTransfer.Errors);
    EXPECT_TRUE(AnyIssueContains(noTransfer.Warnings, "marked NOXFER")) << JoinIssues(noTransfer.Warnings);
}

TEST(MessageDefinitionParserTest, MalformedXmlReportsItsLine)
{
    std::string const text = Document(Message("MSG_ONE") + "<MSG_TWO><RECORD></MSG_TWO>\n");
    MessageParseResult const result = MessageDefinitionParser::Parse(text, "Broken.xml");
    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.Errors.size(), 1u);
    EXPECT_EQ(result.Errors.front().Line, 5u);
    EXPECT_NE(result.Errors.front().ToString().find("Broken.xml:5: the XML is malformed"), std::string::npos) << result.Errors.front().ToString();
    EXPECT_FALSE(MessageDefinitionParser::Parse("", "Blank.xml").Succeeded());
    EXPECT_FALSE(MessageDefinitionParser::Parse("<?xml version=\"1.0\" ?>", "DeclarationOnly.xml").Succeeded());
    EXPECT_EQ((MessageIssue{ "File.xml", 0, "whole file" }).ToString(), "File.xml: whole file");
}

TEST(MessageDefinitionParserTest, MalformedXmlLineIsNotPastANewline)
{
    MessageParseResult const attribute = MessageDefinitionParser::Parse("<Root>\n<MSG_ONE><RECORD><Value TYPE>\n</Value></RECORD></MSG_ONE>\n</Root>\n", "BadAttribute.xml");
    ASSERT_EQ(attribute.Errors.size(), 1u);
    EXPECT_EQ(attribute.Errors.front().Line, 2u) << attribute.Errors.front().ToString();
    MessageParseResult const unclosed = MessageDefinitionParser::Parse("<Root>\n<A>\n", "Unclosed.xml");
    ASSERT_EQ(unclosed.Errors.size(), 1u);
    EXPECT_EQ(unclosed.Errors.front().Line, 2u) << unclosed.Errors.front().ToString();
}

TEST(MessageDefinitionParserTest, ContentOutsideTheRootIsAnError)
{
    std::string const valid = Document(Message("MSG_ONE"));
    MessageParseResult const whitespace = MessageDefinitionParser::Parse(valid + "\n  \n", "Whitespace.xml");
    EXPECT_TRUE(whitespace.Succeeded()) << JoinIssues(whitespace.Errors);

    MessageParseResult const secondRoot = MessageDefinitionParser::Parse(valid + "<Extra><MSG_TWO><RECORD></RECORD></MSG_TWO></Extra>\n", "SecondRoot.xml");
    EXPECT_FALSE(secondRoot.Succeeded());
    EXPECT_TRUE(AnyIssueContains(secondRoot.Errors, "content outside the root element <TestMessages>")) << JoinIssues(secondRoot.Errors);
    ASSERT_FALSE(secondRoot.Errors.empty());
    EXPECT_EQ(secondRoot.Errors.front().Line, 6u);

    MessageParseResult const trailingText = MessageDefinitionParser::Parse(valid + "junk\n", "TrailingText.xml");
    EXPECT_FALSE(trailingText.Succeeded());
    EXPECT_TRUE(AnyIssueContains(trailingText.Errors, "content outside the root element")) << JoinIssues(trailingText.Errors);

    MessageParseResult const leadingText = MessageDefinitionParser::Parse("junk<TestMessages><_ProtocolInfo><RECORD><ServiceID>5</ServiceID></RECORD></_ProtocolInfo><MSG_ONE><RECORD></RECORD></MSG_ONE></TestMessages>", "LeadingText.xml");
    EXPECT_FALSE(leadingText.Succeeded());
}

TEST(MessageDefinitionParserTest, NulBytesAndNulReferencesAreErrors)
{
    std::string withNul = Document(Message("MSG_ONE"));
    withNul += '\0';
    withNul += "<Extra/>";
    MessageParseResult const raw = MessageDefinitionParser::Parse(withNul, "RawNul.xml");
    EXPECT_FALSE(raw.Succeeded());
    EXPECT_TRUE(AnyIssueContains(raw.Errors, "contains a NUL byte")) << JoinIssues(raw.Errors);
    ASSERT_FALSE(raw.Errors.empty());
    EXPECT_EQ(raw.Errors.front().Line, 6u);

    for (char const* reference : { "&#0;", "&#000;", "&#x0;", "&#X00;" })
    {
        MessageParseResult const result = MessageDefinitionParser::Parse(Document(Message("MSG_ONE"), std::string("5") + reference + "9"), "NulReference.xml");
        EXPECT_FALSE(result.Succeeded()) << reference;
        EXPECT_TRUE(AnyIssueContains(result.Errors, "character reference to NUL")) << reference << ": " << JoinIssues(result.Errors);
    }
    MessageParseResult const typeReference = MessageDefinitionParser::Parse(Document(Message("MSG_ONE", "<Force TYPE=\"INT&#0;junk\"></Force>")), "TypeReference.xml");
    EXPECT_FALSE(typeReference.Succeeded());

    MessageParseResult const digit = MessageDefinitionParser::Parse(Document(Message("MSG_ONE"), "&#53;"), "DigitReference.xml");
    ASSERT_TRUE(digit.Succeeded()) << JoinIssues(digit.Errors);
    EXPECT_EQ(digit.Protocol->ServiceId, 5);

    MessageParseResult const others = MessageDefinitionParser::Parse(Document(Message("MSG_ONE", "<Text TYPE=\"STR\">a&#10;b &amp;#0; &#x10;</Text>")), "OtherReferences.xml");
    ASSERT_TRUE(others.Succeeded()) << JoinIssues(others.Errors);
    FieldDef const* const text = others.Protocol->FindByTag("MSG_ONE")->FindField("Text");
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->DefaultValue, std::optional<std::string>("a\nb &#0; \x10"));
}

TEST(MessageDefinitionParserTest, RepeatedMetadataIsAnError)
{
    MessageParseResult const bothOrders = MessageDefinitionParser::Parse(Document(Message("MSG_ONE", "", Order("_MsgOrder", 1) + Order("_MsgType", 2))), "BothOrders.xml");
    EXPECT_FALSE(bothOrders.Succeeded());
    EXPECT_TRUE(AnyIssueContains(bothOrders.Errors, "has both _MsgOrder and _MsgType")) << JoinIssues(bothOrders.Errors);

    MessageParseResult const twice = MessageDefinitionParser::Parse(Document(Message("MSG_ONE", "", Order("_MsgOrder", 1) + Order("_MsgOrder", 2))), "TwiceOrdered.xml");
    EXPECT_FALSE(twice.Succeeded());
    EXPECT_TRUE(AnyIssueContains(twice.Errors, "repeats <_MsgOrder>")) << JoinIssues(twice.Errors);

    MessageParseResult const names = MessageDefinitionParser::Parse(Document(Message("MSG_ONE", "", "<_MsgName>A</_MsgName><_MsgName>B</_MsgName>")), "TwoNames.xml");
    EXPECT_FALSE(names.Succeeded());
    EXPECT_TRUE(AnyIssueContains(names.Errors, "repeats <_MsgName>")) << JoinIssues(names.Errors);
}

TEST(MessageDefinitionParserTest, CrlfInputKeepsLineNumbersAndDefaults)
{
    std::string text = Document(Message("MSG_ONE", "<Value TYPE=\"INT\">\r\n  42\r\n</Value>") + Message("MSG_TWO", "<Other TPYE=\"UINT\"></Other>"));
    std::string crlf;
    for (char const character : text)
    {
        if (character == '\n' && (crlf.empty() || crlf.back() != '\r'))
            crlf += '\r';
        crlf += character;
    }
    MessageParseResult const result = MessageDefinitionParser::Parse(crlf, "Crlf.xml");
    ASSERT_TRUE(result.Succeeded()) << JoinIssues(result.Errors);
    EXPECT_EQ(result.Protocol->FindByTag("MSG_ONE")->FindField("Value")->DefaultValue, std::optional<std::string>("42"));
    ASSERT_EQ(result.Warnings.size(), 1u);
    EXPECT_EQ(result.Warnings.front().Line, 7u);
    EXPECT_EQ(result.Protocol->FindByTag("MSG_TWO")->Line, 7u);
    EXPECT_EQ(result.Protocol->FindByTag("MSG_ONE")->FindField("Value")->Line, 4u);
}

TEST(MessageDefinitionParserTest, TagLessComparesUnsignedBytes)
{
    EXPECT_TRUE(MessageDefinitionParser::TagLess("MSG_DUEL", "MSG_Daily"));
    EXPECT_TRUE(MessageDefinitionParser::TagLess("MSG_Z", "MSG_a"));
    EXPECT_TRUE(MessageDefinitionParser::TagLess("MSG_AB", "MSG_A_B"));
    EXPECT_TRUE(MessageDefinitionParser::TagLess("MSG", "MSG_A"));
    EXPECT_TRUE(MessageDefinitionParser::TagLess("MSG_z", "MSG_\xC3"));
    EXPECT_FALSE(MessageDefinitionParser::TagLess("MSG_A", "MSG_A"));
    EXPECT_TRUE(MessageDefinitionParser::IsMetadataName("_MsgName"));
    EXPECT_FALSE(MessageDefinitionParser::IsMetadataName("MsgName"));
    EXPECT_FALSE(MessageDefinitionParser::IsMetadataName(""));
}

TEST(MessageDefinitionSetTest, KeysProtocolsByServiceIdNotProtocolType)
{
    MessageDefinitionSet set;
    EXPECT_TRUE(set.Add(Document(Message("MSG_START") + Message("MSG_STOP"), "44", "MG3_MESSAGES"), "ShockFixtureMessages.xml"));
    EXPECT_TRUE(set.Add(Document(Message("MSG_START", "<Score TYPE=\"UINT\"></Score>") + Message("MSG_STOP") + Message("MSG_STOP"), "54", "MG3_MESSAGES"), "CatchFixtureMessages.xml"));
    EXPECT_FALSE(set.HasErrors()) << JoinIssues(set.GetErrors());
    EXPECT_EQ(set.GetProtocols().size(), 2u);
    EXPECT_EQ(set.GetRecordCount(), 5u);
    EXPECT_EQ(set.GetMessageCount(), 4u);
    EXPECT_EQ(set.GetFieldCount(), 1u);
    EXPECT_EQ(set.GetTypeCensus(), (std::map<DmlType, std::size_t>{ { DmlType::Uint, 1 } }));
    ASSERT_NE(set.Find(54, 1), nullptr);
    EXPECT_EQ(set.Find(54, 1)->FindField("Score")->Type, DmlType::Uint);
    EXPECT_EQ(set.Find(44, 1)->Fields.size(), 0u);
    EXPECT_EQ(set.FindByTag(54, "MSG_STOP")->Order, 2);
    EXPECT_EQ(set.Find(54, 3), nullptr);
    EXPECT_EQ(set.Find(45, 1), nullptr);
    EXPECT_EQ(set.FindByTag(45, "MSG_STOP"), nullptr);
    EXPECT_EQ(set.FindService(44)->SourceFile, "ShockFixtureMessages.xml");

    EXPECT_FALSE(set.Add(Document(Message("MSG_OTHER"), "44", "OTHER_MESSAGES"), "DuplicateServiceMessages.xml"));
    ASSERT_EQ(set.GetErrors().size(), 1u);
    EXPECT_EQ(set.GetErrors().front().ToString(), "DuplicateServiceMessages.xml: ServiceID 44 is already used by ShockFixtureMessages.xml");
    EXPECT_EQ(set.FindService(44)->SourceFile, "ShockFixtureMessages.xml");

    EXPECT_FALSE(set.Add(Document(Message("MSG_BAD", "<Value></Value>"), "60"), "BrokenMessages.xml"));
    EXPECT_EQ(set.GetErrors().size(), 2u);
    EXPECT_EQ(set.FindService(60), nullptr);
}

TEST(MessageDefinitionSetTest, RecognizesMessageFileNames)
{
    EXPECT_TRUE(MessageDefinitionSet::IsMessageFileName("GameMessages.xml"));
    EXPECT_TRUE(MessageDefinitionSet::IsMessageFileName("WizardMessages3.xml"));
    EXPECT_TRUE(MessageDefinitionSet::IsMessageFileName("Messages/HousingMessages.xml"));
    EXPECT_TRUE(MessageDefinitionSet::IsMessageFileName("loginmessages.XML"));
    EXPECT_TRUE(MessageDefinitionSet::IsMessageFileName("GameMessages22.xml"));
    EXPECT_FALSE(MessageDefinitionSet::IsMessageFileName("GameMessage.xml"));
    EXPECT_FALSE(MessageDefinitionSet::IsMessageFileName("GameMessages.xml.bak"));
    EXPECT_FALSE(MessageDefinitionSet::IsMessageFileName("GameMessages_2.xml"));
    EXPECT_FALSE(MessageDefinitionSet::IsMessageFileName("Messages/Readme.xml"));
    EXPECT_FALSE(MessageDefinitionSet::IsMessageFileName(".xml"));
}

TEST(MessageDefinitionSetTest, LoadsMessageFilesFromAnArchive)
{
    LogTestDirectory directory;
    std::vector<uint8> const bytes = KiwadBuilder(2, 1)
        .Add("LoginFixtureMessages.xml", Document(Message("MSG_B", "", Order("_MsgOrder", 2)) + Message("MSG_A", "", Order("_MsgOrder", 1)), "7"), true)
        .Add("Messages/GameFixtureMessages2.xml", Document(Message("MSG_Z") + Message("MSG_Y", "<GlobalID></GlobalID>"), "55"), false)
        .Add("Locale/en/Fixture.xml", "<NotAProtocol/>", false)
        .Build();
    std::filesystem::path const path = directory.Path() / "Fixture.wad";
    {
        std::ofstream stream(path, std::ios::binary);
        stream.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    std::string error;
    std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(path, error);
    ASSERT_NE(archive, nullptr) << error;

    MessageDefinitionSet set;
    ASSERT_TRUE(set.LoadFromArchive(*archive)) << JoinIssues(set.GetErrors());
    EXPECT_EQ(set.GetProtocols().size(), 2u);
    EXPECT_EQ(set.FindByTag(7, "MSG_A")->Order, 1);
    EXPECT_EQ(set.FindByTag(55, "MSG_Y")->Order, 1);
    ASSERT_EQ(set.GetWarnings().size(), 1u);
    EXPECT_EQ(set.GetWarnings().front().SourceFile, "Messages/GameFixtureMessages2.xml");

    MessageDefinitionSet again;
    EXPECT_TRUE(again.LoadFromArchive(*archive));
    EXPECT_FALSE(again.LoadFromArchive(*archive));
    EXPECT_EQ(again.GetErrors().size(), 2u);
    EXPECT_EQ(again.GetProtocols().size(), 2u);
}

TEST(MessageDefinitionSetTest, ArchiveProblemsAreErrors)
{
    LogTestDirectory directory;
    std::string const bind = std::string("BINd") + std::string(9, '\0') + "compressed payload";
    std::vector<uint8> const bytes = KiwadBuilder(1)
        .Add("BinaryMessages.xml", bind, false)
        .Add("GoodMessages.xml", Document(Message("MSG_ONE"), "3"), false)
        .Build();
    std::filesystem::path const path = directory.Path() / "Problems.wad";
    {
        std::ofstream stream(path, std::ios::binary);
        stream.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    std::string error;
    std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(path, error);
    ASSERT_NE(archive, nullptr) << error;

    MessageDefinitionSet set;
    EXPECT_FALSE(set.LoadFromArchive(*archive));
    ASSERT_EQ(set.GetErrors().size(), 1u);
    EXPECT_EQ(set.GetErrors().front().ToString(), "BinaryMessages.xml: it is a BINd container, not message XML text");
    EXPECT_NE(set.FindService(3), nullptr);

    std::vector<uint8> const empty = KiwadBuilder(1).Add("Readme.txt", "nothing", false).Build();
    std::filesystem::path const emptyPath = directory.Path() / "Empty.wad";
    {
        std::ofstream stream(emptyPath, std::ios::binary);
        stream.write(reinterpret_cast<char const*>(empty.data()), static_cast<std::streamsize>(empty.size()));
    }
    std::unique_ptr<KiwadArchive> const emptyArchive = KiwadArchive::Open(emptyPath, error);
    ASSERT_NE(emptyArchive, nullptr) << error;
    MessageDefinitionSet none;
    EXPECT_FALSE(none.LoadFromArchive(*emptyArchive));
    EXPECT_EQ(none.GetErrors().front().ToString(), "Empty.wad: the archive has no message definition files");
}
