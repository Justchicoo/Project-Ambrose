/*
 * Project Ambrose by Imjustchico
 * Checks that a remembered window place comes back exactly as it was written, and that every place a window could not be opened at is dropped instead: a size too small for the screens to hold their words, a size no display could show, a corner further away than any desktop reaches, a value that is not a number, a file from a version that wrote something else, and a file that is not a place at all. A window that forgets is a small annoyance; a window that comes back somewhere its user cannot reach cannot be closed, so every doubtful case forgets.
 */

#include "LauncherPlace.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>

TEST(LauncherPlaceTest, APlaceComesBackAsItWasWritten)
{
    WindowPlace written;
    written.X = 240;
    written.Y = 120;
    written.Width = 1180;
    written.Height = 780;
    written.Maximised = true;

    std::optional<WindowPlace> const read = LauncherPlace::Read(LauncherPlace::Describe(written));
    ASSERT_TRUE(read);
    EXPECT_EQ(read->X, written.X);
    EXPECT_EQ(read->Y, written.Y);
    EXPECT_EQ(read->Width, written.Width);
    EXPECT_EQ(read->Height, written.Height);
    EXPECT_TRUE(read->Maximised);
}

TEST(LauncherPlaceTest, ANegativeCornerIsAPlaceASecondScreenCanBe)
{
    WindowPlace written;
    written.X = -1720;
    written.Y = -300;
    written.Width = 980;
    written.Height = 720;

    std::optional<WindowPlace> const read = LauncherPlace::Read(LauncherPlace::Describe(written));
    ASSERT_TRUE(read) << "a screen left of or above the main one has negative coordinates and is a real place to be";
    EXPECT_EQ(read->X, -1720);
    EXPECT_EQ(read->Y, -300);
    EXPECT_FALSE(read->Maximised);
}

TEST(LauncherPlaceTest, ASizeTheScreensCouldNotBeReadAtIsDropped)
{
    std::string const narrow = R"({"schema":1,"x":10,"y":10,"width":200,"height":720})";
    EXPECT_FALSE(LauncherPlace::Read(narrow)) << "the launcher's own screens need room for their words, so a sliver is not a size";

    std::string const short_ = R"({"schema":1,"x":10,"y":10,"width":980,"height":40})";
    EXPECT_FALSE(LauncherPlace::Read(short_));

    std::string const huge = R"({"schema":1,"x":10,"y":10,"width":99999,"height":720})";
    EXPECT_FALSE(LauncherPlace::Read(huge));
}

TEST(LauncherPlaceTest, APlaceNoDesktopReachesIsDropped)
{
    EXPECT_FALSE(LauncherPlace::Read(R"({"schema":1,"x":900000,"y":10,"width":980,"height":720})"));
    EXPECT_FALSE(LauncherPlace::Read(R"({"schema":1,"x":10,"y":-900000,"width":980,"height":720})"));
    EXPECT_FALSE(LauncherPlace::Read(R"({"schema":1,"x":10,"y":10,"width":980,"height":720,"maximised":18446744073709551615})"));
}

TEST(LauncherPlaceTest, AnythingThatIsNotAPlaceLeavesTheWindowWhereItWouldHaveOpened)
{
    EXPECT_FALSE(LauncherPlace::Read(""));
    EXPECT_FALSE(LauncherPlace::Read("not a place at all"));
    EXPECT_FALSE(LauncherPlace::Read("[1,2,3]"));
    EXPECT_FALSE(LauncherPlace::Read(R"({"x":10,"y":10,"width":980,"height":720})")) << "a file with no schema is a file this version cannot read";
    EXPECT_FALSE(LauncherPlace::Read(R"({"schema":99,"x":10,"y":10,"width":980,"height":720})"));
    EXPECT_FALSE(LauncherPlace::Read(R"({"schema":1,"x":"ten","y":10,"width":980,"height":720})"));
    EXPECT_FALSE(LauncherPlace::Read(R"({"schema":1,"x":10.5,"y":10,"width":980,"height":720})"));
    EXPECT_FALSE(LauncherPlace::Read(R"({"schema":1,"y":10,"width":980,"height":720})"));
}
