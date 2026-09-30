/*
 * Project Ambrose by Imjustchico
 * Checks where a program's web views keep their data: every profile lies under the program's own data folder, the program's own pages and each remote origin get folders of their own, two origins that differ only by port or scheme never share one, and deleting a profile removes it with everything in it while leaving the others.
 */

#include "LogTestDirectory.h"
#include "ShellProfiles.h"
#include "ShellWindow.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <optional>
#include <string>

namespace
{
    ShellOrigin Origin(std::string const& url)
    {
        return ShellOrigin::Of(url).value_or(ShellOrigin{});
    }
}

TEST(ShellProfilesTest, EachOriginGetsAFolderOfItsOwnUnderTheProgramsDataFolder)
{
    std::filesystem::path const data = "data-folder";
    std::filesystem::path const first = ShellProfiles::FolderFor(data, Origin("http://127.0.0.1:12021"));
    std::filesystem::path const second = ShellProfiles::FolderFor(data, Origin("http://127.0.0.1:12022"));
    std::filesystem::path const secure = ShellProfiles::FolderFor(data, Origin("https://127.0.0.1:12021"));
    std::filesystem::path const own = ShellProfiles::OwnFolder(data);
    EXPECT_NE(first, second);
    EXPECT_NE(first, secure);
    EXPECT_NE(first, own);
    for (std::filesystem::path const& folder : { first, second, secure, own })
        EXPECT_EQ(folder.parent_path(), data / ShellProfiles::Folder) << folder;
    EXPECT_EQ(ShellProfiles::NameFor(Origin("http://[::1]:12021")), "http-___1_-12021");
    EXPECT_EQ(ShellProfiles::NameFor(Origin("https://Panel.Example.org:8443")), "https-panel.example.org-8443");

    ShellWindowOptions options;
    options.Program = "panel";
    options.DataFolder = data;
    options.Remote = "http://127.0.0.1:12021/";
    EXPECT_EQ(ShellWindow::ProfileFolder(options), first);
}

TEST(ShellProfilesTest, DeletingAProfileRemovesEverythingInItAndLeavesTheOthers)
{
    LogTestDirectory directory;
    ShellOrigin const first = Origin("http://127.0.0.1:12021");
    ShellOrigin const second = Origin("http://127.0.0.1:12022");
    for (ShellOrigin const& origin : { first, second })
    {
        std::filesystem::path const folder = ShellProfiles::FolderFor(directory.Path(), origin) / "EBWebView" / "Default";
        std::filesystem::create_directories(folder);
        std::ofstream(folder / "Cookies") << "cookie";
    }
    std::vector<std::string> names = ShellProfiles::List(directory.Path());
    EXPECT_EQ(names.size(), 2u);

    std::string error;
    ASSERT_TRUE(ShellProfiles::Delete(directory.Path(), first, error)) << error;
    EXPECT_FALSE(std::filesystem::exists(ShellProfiles::FolderFor(directory.Path(), first)));
    EXPECT_TRUE(std::filesystem::exists(ShellProfiles::FolderFor(directory.Path(), second) / "EBWebView" / "Default" / "Cookies"));
    names = ShellProfiles::List(directory.Path());
    ASSERT_EQ(names.size(), 1u);
    EXPECT_EQ(names.front(), ShellProfiles::NameFor(second));
    EXPECT_TRUE(ShellProfiles::Delete(directory.Path(), first, error)) << "deleting a profile that is gone succeeds";
}
