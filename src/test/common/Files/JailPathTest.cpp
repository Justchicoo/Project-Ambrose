/*
 * Project Ambrose by Imjustchico
 * Tests the request path a file root is asked for, on every system alike: traversal, absolute paths, drive letters, UNC and device prefixes and backslashes are refused, a query is decoded exactly once so an encoded traversal is refused and a doubly encoded one is only a name, a malformed escape is refused rather than cut short, control characters, names Windows cannot hold and reserved device names with any extension are refused on every system, over-long names, paths and depths are refused, ordinary names with inner dots, spaces and Unicode are kept, and the host path a refused request would have reached is worked out for the audit row.
 */

#include "JailPath.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    Ambrose::JailRefusal RefusalOf(std::string_view text)
    {
        return Ambrose::JailPaths::ParseText(text).Refusal;
    }

    Ambrose::JailRefusal QueryRefusal(std::string_view raw)
    {
        return Ambrose::JailPaths::ParseQuery(raw).Refusal;
    }
}

TEST(JailPathTest, RefusesTraversalAbsoluteDriveUncAndBackslashes)
{
    EXPECT_EQ(RefusalOf(".."), Ambrose::JailRefusal::Traversal);
    EXPECT_EQ(RefusalOf("../x"), Ambrose::JailRefusal::Traversal);
    EXPECT_EQ(RefusalOf("a/../../x"), Ambrose::JailRefusal::Traversal);
    EXPECT_EQ(RefusalOf("a/./b"), Ambrose::JailRefusal::Traversal);
    EXPECT_EQ(RefusalOf("."), Ambrose::JailRefusal::Traversal);
    EXPECT_EQ(RefusalOf("a\\..\\..\\x"), Ambrose::JailRefusal::Traversal);
    EXPECT_EQ(RefusalOf("/etc/passwd"), Ambrose::JailRefusal::Absolute);
    EXPECT_EQ(RefusalOf("C:/Windows"), Ambrose::JailRefusal::Absolute);
    EXPECT_EQ(RefusalOf("c:"), Ambrose::JailRefusal::Absolute);
    EXPECT_EQ(RefusalOf("\\\\server\\share\\x"), Ambrose::JailRefusal::Absolute);
    EXPECT_EQ(RefusalOf("\\\\?\\C:\\Windows"), Ambrose::JailRefusal::Absolute);
    EXPECT_EQ(RefusalOf("//server/share"), Ambrose::JailRefusal::Absolute);
    EXPECT_EQ(RefusalOf("a\\b"), Ambrose::JailRefusal::Name);
    EXPECT_EQ(RefusalOf("a//b"), Ambrose::JailRefusal::Name);
    EXPECT_EQ(RefusalOf("a/b/"), Ambrose::JailRefusal::Name);
}

TEST(JailPathTest, DecodesOnceSoAnEncodedTraversalIsRefusedAndADoubleEncodedOneIsAPlainName)
{
    EXPECT_EQ(QueryRefusal("%2e%2e%2fx"), Ambrose::JailRefusal::Traversal);
    EXPECT_EQ(QueryRefusal("%2E%2E/x"), Ambrose::JailRefusal::Traversal);
    EXPECT_EQ(QueryRefusal("%2fetc%2fpasswd"), Ambrose::JailRefusal::Absolute);
    EXPECT_EQ(QueryRefusal("a%5c..%5cb"), Ambrose::JailRefusal::Traversal);

    Ambrose::JailPathResult const twice = Ambrose::JailPaths::ParseQuery("%252e%252e%252fx");
    ASSERT_TRUE(twice.Ok()) << twice.Reason;
    ASSERT_EQ(twice.Path.Components.size(), 1u);
    EXPECT_EQ(twice.Path.Components[0], "%2e%2e%2fx") << "decoded once, a doubly encoded traversal is only a name holding percent signs";

    Ambrose::JailPathResult const plus = Ambrose::JailPaths::ParseQuery("a+b%20c");
    ASSERT_TRUE(plus.Ok()) << plus.Reason;
    EXPECT_EQ(plus.Path.Text(), "a+b c") << "a plus sign stays itself";

    EXPECT_EQ(QueryRefusal("abc%"), Ambrose::JailRefusal::Encoding);
    EXPECT_EQ(QueryRefusal("abc%2"), Ambrose::JailRefusal::Encoding);
    EXPECT_EQ(QueryRefusal("abc%zz/def"), Ambrose::JailRefusal::Encoding) << "a malformed escape is refused rather than cut short";
    EXPECT_EQ(QueryRefusal("%ff%fe"), Ambrose::JailRefusal::Encoding) << "what decodes to text that is not UTF-8 is refused";
    EXPECT_EQ(RefusalOf("caf\xC3"), Ambrose::JailRefusal::Encoding);
}

TEST(JailPathTest, RefusesDeviceNamesWithAnyExtensionOnEverySystem)
{
    for (std::string_view const name : { "CON", "con", "PRN", "AUX", "NUL", "nul.txt", "Con.tar.gz", "COM1", "com9.log", "LPT1", "lpt0", "COM0", "CONIN$", "conout$.x", "NUL .txt",
             "COM\xC2\xB9", "LPT\xC2\xB2.txt", "com\xC2\xB3" })
        EXPECT_EQ(RefusalOf(name), Ambrose::JailRefusal::DeviceName) << name;
    EXPECT_EQ(RefusalOf("logs/CON/x"), Ambrose::JailRefusal::DeviceName) << "a device name is refused at any depth";
    for (std::string_view const name : { "CONSOLE", "com10", "LPT", "nullable.txt", "auxiliary", "COM\xC2\xB9" "0" })
        EXPECT_EQ(RefusalOf(name), Ambrose::JailRefusal::None) << name;
}

TEST(JailPathTest, RefusesControlCharactersNamesWindowsCannotHoldAndMalformedEscapes)
{
    EXPECT_EQ(RefusalOf(std::string_view("a\0b", 3)), Ambrose::JailRefusal::Name);
    EXPECT_EQ(RefusalOf("a\tb"), Ambrose::JailRefusal::Name);
    EXPECT_EQ(RefusalOf("a\x7F"), Ambrose::JailRefusal::Name);
    EXPECT_EQ(RefusalOf("a\xC2\x85z"), Ambrose::JailRefusal::Name) << "a C1 control is refused too";
    EXPECT_EQ(QueryRefusal("a%00b"), Ambrose::JailRefusal::Name);
    EXPECT_EQ(QueryRefusal("a%0ab"), Ambrose::JailRefusal::Name);
    for (std::string_view const name : { "a<b", "a>b", "ab:c", "file.txt:stream", "a\"b", "a|b", "a?b", "a*b" })
        EXPECT_EQ(RefusalOf(name), Ambrose::JailRefusal::Name) << name;
    EXPECT_EQ(RefusalOf("name."), Ambrose::JailRefusal::Name);
    EXPECT_EQ(RefusalOf("name "), Ambrose::JailRefusal::Name);
    EXPECT_EQ(RefusalOf("folder./x"), Ambrose::JailRefusal::Name);
    EXPECT_EQ(QueryRefusal("%zz"), Ambrose::JailRefusal::Encoding);
}

TEST(JailPathTest, RefusesOverlongComponentsAndExcessiveDepth)
{
    std::string const longest(Ambrose::JailPaths::MaxComponentBytes, 'a');
    EXPECT_EQ(RefusalOf(longest), Ambrose::JailRefusal::None);
    EXPECT_EQ(RefusalOf(longest + "a"), Ambrose::JailRefusal::TooLong);

    std::string deepest;
    for (std::size_t index = 0; index < Ambrose::JailPaths::MaxComponents; ++index)
        deepest += index == 0 ? "d" : "/d";
    EXPECT_EQ(RefusalOf(deepest), Ambrose::JailRefusal::None);
    EXPECT_EQ(RefusalOf(deepest + "/d"), Ambrose::JailRefusal::TooDeep);

    std::string wide;
    while (wide.size() <= Ambrose::JailPaths::MaxPathBytes)
        wide += (wide.empty() ? "" : "/") + std::string(200, 'w');
    EXPECT_EQ(RefusalOf(wide), Ambrose::JailRefusal::TooLong);

    for (Ambrose::JailRefusal const refusal : { Ambrose::JailRefusal::Traversal, Ambrose::JailRefusal::Absolute, Ambrose::JailRefusal::Encoding, Ambrose::JailRefusal::Name,
             Ambrose::JailRefusal::DeviceName, Ambrose::JailRefusal::TooLong, Ambrose::JailRefusal::TooDeep })
        EXPECT_FALSE(Ambrose::JailPaths::RefusalCode(refusal).empty());
    EXPECT_EQ(Ambrose::JailPaths::RefusalCode(Ambrose::JailRefusal::DeviceName), "device_name");
}

TEST(JailPathTest, AcceptsOrdinaryNamesIncludingInnerDotsAndUnicode)
{
    Ambrose::JailPathResult const empty = Ambrose::JailPaths::ParseQuery("");
    ASSERT_TRUE(empty.Ok());
    EXPECT_TRUE(empty.Path.IsRoot());

    Ambrose::JailPathResult const nested = Ambrose::JailPaths::ParseText("types/r806919.types.json");
    ASSERT_TRUE(nested.Ok()) << nested.Reason;
    EXPECT_EQ(nested.Path.Components, (std::vector<std::string>{ "types", "r806919.types.json" }));
    EXPECT_EQ(nested.Path.Leaf(), "r806919.types.json");
    EXPECT_EQ(nested.Path.Parent().Text(), "types");
    EXPECT_EQ(nested.Path.Parent().Child("other").Text(), "types/other");

    for (std::string_view const name : { ".hidden", "a..b", "My Logs/today.log", "caf\xC3\xA9/\xE6\x97\xA5\xE8\xAA\x8C.txt", "..a", "~backup", "-dash", "#hash", "!bang" })
        EXPECT_EQ(RefusalOf(name), Ambrose::JailRefusal::None) << name;
    EXPECT_EQ(RefusalOf("a:"), Ambrose::JailRefusal::Absolute) << "a letter and a colon reads as a drive, which is refused before the colon is";
    EXPECT_EQ(Ambrose::JailPaths::ParseQuery("caf%C3%A9").Path.Text(), "caf\xC3\xA9");
}

TEST(JailPathTest, WorksOutTheHostPathARefusedRequestWouldHaveReached)
{
    std::filesystem::path const root = std::filesystem::path("srv") / "ambrose" / "data";
    EXPECT_EQ(Ambrose::JailPaths::HostPathFor(root, "../x"), (std::filesystem::path("srv") / "ambrose" / "x").lexically_normal());
    EXPECT_EQ(Ambrose::JailPaths::HostPathFor(root, "/etc/passwd"), std::filesystem::path("/etc/passwd").lexically_normal());
    EXPECT_EQ(Ambrose::JailPaths::HostPathFor(root, "C:/Windows"), std::filesystem::path("C:/Windows").lexically_normal());
    EXPECT_EQ(Ambrose::JailPaths::HostPathFor(root, "CON"), (root / "CON").lexically_normal());
    EXPECT_EQ(Ambrose::JailPaths::HostPathFor(root, "a\\..\\..\\b"), (std::filesystem::path("srv") / "ambrose" / "b").lexically_normal());
    EXPECT_FALSE(Ambrose::JailPaths::HostPathFor(root, std::string_view("x\0y", 3)).empty());
}
