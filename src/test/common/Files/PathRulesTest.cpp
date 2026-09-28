/*
 * Project Ambrose by Imjustchico
 * Tests protected path patterns in gitignore syntax: a name with no slash matches at any depth, a leading or inner slash anchors to the root, a trailing slash matches only folders and everything beneath them, * and ? stay inside one name, ** crosses folders, classes take ranges and negation, escapes make a special character plain, a negation undoes only an earlier rule of its own layer, so an operator's cannot undo a built-in one, a carve-out and the built-in protections come before the operator's and those before client-derived and read-only, case folds where the file system does, a pattern of many ** is answered in time bounded by its segments and the path's depth, a match names the rule and why it exists, and a pattern that could never match is refused with the reason.
 */

#include "FilePolicy.h"
#include "PathRules.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>

namespace
{
    Ambrose::PathRules With(std::initializer_list<std::string_view> patterns, Ambrose::RuleEffect effect = Ambrose::RuleEffect::Hide, Ambrose::RuleOrigin origin = Ambrose::RuleOrigin::BuiltIn,
        Ambrose::CaseMode mode = Ambrose::CaseMode::Sensitive)
    {
        Ambrose::PathRules rules(mode);
        for (std::string_view const pattern : patterns)
        {
            std::string error;
            EXPECT_TRUE(rules.Add(pattern, effect, origin, "a test rule", error)) << pattern << ": " << error;
        }
        return rules;
    }

    bool Hits(Ambrose::PathRules const& rules, std::string_view path, bool folder = false)
    {
        return rules.Match(path, folder).has_value();
    }
}

TEST(PathRulesTest, MatchesGitignoreSyntax)
{
    Ambrose::PathRules const anywhere = With({ "*.lock" });
    EXPECT_TRUE(Hits(anywhere, "types/r1.lock"));
    EXPECT_TRUE(Hits(anywhere, "r1.lock"));
    EXPECT_FALSE(Hits(anywhere, "r1.locked"));

    Ambrose::PathRules const anchored = With({ "/panel/" });
    EXPECT_TRUE(Hits(anchored, "panel", true));
    EXPECT_FALSE(Hits(anchored, "panel", false)) << "a trailing slash matches only a folder";
    EXPECT_TRUE(Hits(anchored, "panel/panel.sqlite3")) << "everything beneath a matched folder is matched";
    EXPECT_FALSE(Hits(anchored, "other/panel/x")) << "a leading slash anchors to the root";

    Ambrose::PathRules const inner = With({ "logs/*.log" });
    EXPECT_TRUE(Hits(inner, "logs/a.log"));
    EXPECT_FALSE(Hits(inner, "logs/deeper/a.log")) << "* stays inside one name";
    EXPECT_FALSE(Hits(inner, "x/logs/a.log")) << "an inner slash anchors too";

    Ambrose::PathRules const deep = With({ "**/cache/", "build/**", "a/**/z.txt" });
    EXPECT_TRUE(Hits(deep, "cache", true));
    EXPECT_TRUE(Hits(deep, "x/y/cache/file"));
    EXPECT_TRUE(Hits(deep, "build/one"));
    EXPECT_FALSE(Hits(deep, "build", true)) << "a trailing ** needs at least one name beneath";
    EXPECT_TRUE(Hits(deep, "a/z.txt"));
    EXPECT_TRUE(Hits(deep, "a/b/c/z.txt"));

    Ambrose::PathRules const classes = With({ "r[0-9]?.txt", "[!a-c]*.bin", "file\\*.txt", "\\#hash" });
    EXPECT_TRUE(Hits(classes, "r1a.txt"));
    EXPECT_FALSE(Hits(classes, "rxa.txt"));
    EXPECT_TRUE(Hits(classes, "d.bin"));
    EXPECT_FALSE(Hits(classes, "b.bin"));
    EXPECT_TRUE(Hits(classes, "file*.txt"));
    EXPECT_FALSE(Hits(classes, "fileX.txt")) << "an escaped star is a plain star";
    EXPECT_TRUE(Hits(classes, "#hash"));
}

TEST(PathRulesTest, ANegationUndoesOnlyAnEarlierRuleOfItsOwnLayer)
{
    Ambrose::PathRules rules = With({ "*", "!/supervisor/", "!/supervisor/**", "!/launcher-window.json" }, Ambrose::RuleEffect::ClientDerived);
    EXPECT_TRUE(Hits(rules, "types/r1.json"));
    EXPECT_FALSE(Hits(rules, "supervisor/state.json"));
    EXPECT_FALSE(Hits(rules, "launcher-window.json"));
    EXPECT_TRUE(Hits(rules, "deep/launcher-window.json"));
}

TEST(PathRulesTest, ABuiltInRuleCannotBeUndoneByAnOperatorNegation)
{
    Ambrose::PathRules rules(Ambrose::CaseMode::Sensitive);
    std::string error;
    ASSERT_TRUE(rules.Add("/panel/", Ambrose::RuleEffect::Hide, Ambrose::RuleOrigin::BuiltIn, "the panel's store", error)) << error;
    ASSERT_TRUE(rules.Add("!/panel/", Ambrose::RuleEffect::Hide, Ambrose::RuleOrigin::Operator, "", error)) << error;
    ASSERT_TRUE(rules.Add("!/panel/**", Ambrose::RuleEffect::Hide, Ambrose::RuleOrigin::Operator, "", error)) << error;
    std::optional<Ambrose::RuleHit> const hit = rules.Match("panel/panel.sqlite3", false);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->Origin, Ambrose::RuleOrigin::BuiltIn);

    ASSERT_TRUE(rules.Add("/private/", Ambrose::RuleEffect::Hide, Ambrose::RuleOrigin::Operator, "", error)) << error;
    ASSERT_TRUE(rules.Add("!/private/open.txt", Ambrose::RuleEffect::Hide, Ambrose::RuleOrigin::Operator, "", error)) << error;
    EXPECT_TRUE(Hits(rules, "private/open.txt")) << "nothing beneath an excluded folder comes back, as in git";
    ASSERT_TRUE(rules.Add("*.key", Ambrose::RuleEffect::Hide, Ambrose::RuleOrigin::Operator, "", error)) << error;
    ASSERT_TRUE(rules.Add("!public.key", Ambrose::RuleEffect::Hide, Ambrose::RuleOrigin::Operator, "", error)) << error;
    EXPECT_TRUE(Hits(rules, "secret.key"));
    EXPECT_FALSE(Hits(rules, "public.key")) << "an operator's negation undoes the operator's own rule";
}

TEST(PathRulesTest, NamesTheRuleThatRefused)
{
    Ambrose::PathRules rules(Ambrose::CaseMode::Sensitive);
    std::string error;
    ASSERT_TRUE(rules.Add("/backups/", Ambrose::RuleEffect::Elsewhere, Ambrose::RuleOrigin::BuiltIn, "This folder is the backups root; open it there", error));
    ASSERT_TRUE(rules.Add("/admin/", Ambrose::RuleEffect::Hide, Ambrose::RuleOrigin::BuiltIn, "Admin token files", error));
    ASSERT_TRUE(rules.Add("*", Ambrose::RuleEffect::ClientDerived, Ambrose::RuleOrigin::BuiltIn, "Built from your own install", error));
    ASSERT_TRUE(rules.Add("notes/", Ambrose::RuleEffect::Hide, Ambrose::RuleOrigin::Operator, "An operator protected this path", error));

    std::optional<Ambrose::RuleHit> const admin = rules.Match("admin/gameserver.token", false);
    ASSERT_TRUE(admin.has_value());
    EXPECT_EQ(admin->Pattern, "/admin/");
    EXPECT_EQ(admin->Effect, Ambrose::RuleEffect::Hide);
    EXPECT_EQ(admin->Why, "Admin token files");

    std::optional<Ambrose::RuleHit> const backups = rules.Match("backups/x.tar", false);
    ASSERT_TRUE(backups.has_value());
    EXPECT_EQ(backups->Effect, Ambrose::RuleEffect::Elsewhere) << "a carve-out comes before every other layer";

    std::optional<Ambrose::RuleHit> const notes = rules.Match("notes/today.txt", false);
    ASSERT_TRUE(notes.has_value());
    EXPECT_EQ(notes->Origin, Ambrose::RuleOrigin::Operator) << "an operator's protection comes before client-derived data";

    std::optional<Ambrose::RuleHit> const types = rules.Match("types/r1.json", false);
    ASSERT_TRUE(types.has_value());
    EXPECT_EQ(types->Effect, Ambrose::RuleEffect::ClientDerived);

    Ambrose::FilePolicy policy;
    Ambrose::OperationVerdict const refused = policy.DecideAt(admin, Ambrose::FileOperation::Read);
    EXPECT_FALSE(refused.Allowed);
    EXPECT_EQ(refused.Code, "protected");
    EXPECT_EQ(refused.Rule, "/admin/");
    EXPECT_NE(refused.Reason.find("/admin/"), std::string::npos);
    EXPECT_TRUE(policy.DecideAt(types, Ambrose::FileOperation::List).Allowed) << "client-derived data is still listed";
    EXPECT_EQ(policy.DecideAt(types, Ambrose::FileOperation::Download).Code, "client_derived");
    EXPECT_EQ(policy.DecideAt(backups, Ambrose::FileOperation::List).Code, "elsewhere");
}

TEST(PathRulesTest, FoldsCaseOnlyWhereTheFileSystemDoes)
{
    Ambrose::PathRules const folding = With({ "/Panel/", "*.LOCK" }, Ambrose::RuleEffect::Hide, Ambrose::RuleOrigin::BuiltIn, Ambrose::CaseMode::Insensitive);
    EXPECT_TRUE(Hits(folding, "PANEL/x"));
    EXPECT_TRUE(Hits(folding, "a.lock"));
    Ambrose::PathRules const exact = With({ "/Panel/", "*.LOCK" });
    EXPECT_FALSE(Hits(exact, "PANEL/x"));
    EXPECT_FALSE(Hits(exact, "a.lock"));
#if defined(_WIN32)
    EXPECT_EQ(Ambrose::PathRules::PlatformCase(), Ambrose::CaseMode::Insensitive);
#else
    EXPECT_EQ(Ambrose::PathRules::PlatformCase(), Ambrose::CaseMode::Sensitive);
#endif
}

TEST(PathRulesTest, ManyDoubleStarsCostNoMoreThanTheirSegmentsTimesTheDepth)
{
    std::string pattern;
    for (int index = 0; index < 300; ++index)
        pattern += "**/";
    pattern += "never";
    Ambrose::PathRules rules(Ambrose::CaseMode::Sensitive);
    std::string error;
    ASSERT_TRUE(rules.Add(pattern, Ambrose::RuleEffect::Hide, Ambrose::RuleOrigin::Operator, "a pathological pattern", error)) << error;
    std::string deep;
    for (int index = 0; index < 64; ++index)
        deep += index == 0 ? "d" : "/d";
    EXPECT_FALSE(Hits(rules, deep)) << "three hundred ** against sixty-four folders is answered, not searched for ever";
    ASSERT_TRUE(rules.Add("**/d/**/d/**/d/**/d", Ambrose::RuleEffect::Hide, Ambrose::RuleOrigin::Operator, "a pattern that matches", error)) << error;
    EXPECT_TRUE(Hits(rules, deep));
}

TEST(PathRulesTest, RefusesAPatternThatCouldNeverMatch)
{
    for (std::string_view const pattern : { "", "!", "/", "a//b", "../x", "a/./b", "[abc", "trailing\\", "a\tb" })
    {
        std::string error;
        EXPECT_FALSE(Ambrose::PathRules::Compile(pattern, Ambrose::RuleEffect::Hide, Ambrose::RuleOrigin::Operator, {}, error).has_value()) << pattern;
        EXPECT_FALSE(error.empty()) << pattern;
    }
    std::string error;
    EXPECT_FALSE(Ambrose::PathRules::Compile(std::string(Ambrose::PathRules::MaxPatternBytes + 1, 'a'), Ambrose::RuleEffect::Hide, Ambrose::RuleOrigin::Operator, {}, error).has_value());
    EXPECT_TRUE(Ambrose::PathRules::IsComment("# a note"));
    EXPECT_TRUE(Ambrose::PathRules::IsComment("   "));
    EXPECT_FALSE(Ambrose::PathRules::IsComment("\\#literal"));
    EXPECT_EQ(Ambrose::PathRules::Escape("a*b?[c]"), "a\\*b\\?\\[c\\]");
    EXPECT_EQ(Ambrose::PathRules::Escape("#x"), "\\#x");
}
