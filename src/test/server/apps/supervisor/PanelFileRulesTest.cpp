/*
 * Project Ambrose by Imjustchico
 * Tests an owner's protected patterns in the panel store: each root keeps its own in the order they were written, a replace takes the whole list and lands with its audit row or not at all, blank lines and comments are dropped before anything is kept, a pattern that could never match is refused naming its line, and a list longer than a root may hold is refused.
 */

#include "LogTestDirectory.h"
#include "PanelAudit.h"
#include "PanelFileRules.h"
#include "PanelStore.h"
#include "SourceFolder.h"

#include <gtest/gtest.h>

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace
{
    class PanelFileRulesTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::vector<std::string> warnings;
            std::string error;
            ASSERT_TRUE(_store.Open(_directory.Path() / "panel.sqlite3", Ambrose::FindSourceFolder(), warnings, error)) << error;
        }

        AuditEvent Changed(std::string const& root) const
        {
            AuditEvent event;
            event.Name = "file:rules.changed";
            event.Actor = AuditActor::User;
            event.ActorId = "user:1";
            event.Properties = "{\"root\":\"" + root + "\"}";
            event.On("file_root", root, root);
            return event;
        }

        LogTestDirectory _directory;
        PanelStore _store;
    };
}

TEST_F(PanelFileRulesTest, KeepsEachRootsPatternsInOrderWithTheirAuditRow)
{
    PanelFileRules rules(_store);
    std::string error;
    ASSERT_TRUE(PanelAudit::Record(_store, Changed("data"), [&rules](std::string& failure) { return rules.Replace("data", { "notes/", "*.bak", "!keep.bak" }, std::nullopt, failure); }, error))
        << error;
    ASSERT_TRUE(PanelAudit::Record(_store, Changed("logs"), [&rules](std::string& failure) { return rules.Replace("logs", { "old/" }, std::nullopt, failure); }, error)) << error;
    EXPECT_EQ(PanelAudit::Count(_store, "file:rules.changed"), 2);

    std::vector<std::string> data;
    ASSERT_TRUE(rules.ReadRoot("data", data, error)) << error;
    EXPECT_EQ(data, (std::vector<std::string>{ "notes/", "*.bak", "!keep.bak" }));
    std::map<std::string, std::vector<std::string>, std::less<>> every;
    ASSERT_TRUE(rules.Read(every, error)) << error;
    EXPECT_EQ(every.size(), 2u);
    EXPECT_EQ(every["logs"], (std::vector<std::string>{ "old/" }));

    ASSERT_TRUE(PanelAudit::Record(_store, Changed("data"), [&rules](std::string& failure) { return rules.Replace("data", { "private/" }, std::nullopt, failure); }, error)) << error;
    ASSERT_TRUE(rules.ReadRoot("data", data, error)) << error;
    EXPECT_EQ(data, (std::vector<std::string>{ "private/" })) << "a replace takes the whole list";

    EXPECT_FALSE(PanelAudit::Record(_store, Changed("data"), [&rules](std::string& failure)
    {
        if (!rules.Replace("data", { "gone/" }, std::nullopt, failure))
            return false;
        failure = "the change after the patterns failed";
        return false;
    }, error));
    ASSERT_TRUE(rules.ReadRoot("data", data, error)) << error;
    EXPECT_EQ(data, (std::vector<std::string>{ "private/" })) << "patterns whose audit transaction failed are not kept";
    EXPECT_EQ(PanelAudit::Count(_store, "file:rules.changed"), 3);
}

TEST_F(PanelFileRulesTest, AnInvalidPatternIsRefusedNamingItsLine)
{
    std::vector<std::string> const lines{ "# the notes the operators keep", "", "notes/", "[unclosed", "a//b", "  " };
    std::vector<PanelFileRuleProblem> const problems = PanelFileRules::Validate(lines);
    ASSERT_EQ(problems.size(), 2u);
    EXPECT_EQ(problems[0].Line, 4u);
    EXPECT_NE(problems[0].Message.find("["), std::string::npos) << problems[0].Message;
    EXPECT_EQ(problems[1].Line, 5u);
    EXPECT_EQ(PanelFileRules::Normalise(lines), (std::vector<std::string>{ "notes/", "[unclosed", "a//b" }));
    EXPECT_TRUE(PanelFileRules::Validate({ "notes/\r", "*.bak" }).empty()) << "a line ending carried in is not part of the pattern";

    std::vector<std::string> const many(PanelFileRules::MaxPatterns + 1, "x/");
    std::vector<PanelFileRuleProblem> const tooMany = PanelFileRules::Validate(many);
    ASSERT_EQ(tooMany.size(), 1u);
    EXPECT_EQ(tooMany[0].Line, 0u);
}
