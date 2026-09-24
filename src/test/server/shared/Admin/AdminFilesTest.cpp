/*
 * Project Ambrose by Imjustchico
 * Tests how the admin API serves the built panel: the index at the root and revalidated, fingerprinted assets cached for a year, only GET and HEAD, only known types, nothing outside the folder whatever the path tries, a link out of the folder refused where the system lets a test make one, and a note naming the setting when the folder holds no panel.
 */

#include "AdminFiles.h"
#include "LogTestDirectory.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace
{
    AdminRequest Ask(std::string path, std::string method = "GET")
    {
        AdminRequest request;
        request.Method = std::move(method);
        request.Path = std::move(path);
        request.RemoteAddress = "127.0.0.1";
        return request;
    }

    std::string HeaderValue(AdminResponse const& response, std::string const& name)
    {
        for (std::pair<std::string, std::string> const& header : response.Headers)
            if (header.first == name)
                return header.second;
        return {};
    }

    struct Panel
    {
        Panel()
        {
            Directory.Write("dist/index.html", "<!doctype html><title>Ambrose</title>");
            Directory.Write("dist/assets/index-abc123.js", "console.log(1);");
            Directory.Write("dist/assets/index-abc123.css", "body{}");
            Directory.Write("dist/notes.md", "# notes");
            Directory.Write("secret.txt", "outside the panel");
            Files.SetRoot(Directory.Path() / "dist");
        }

        LogTestDirectory Directory;
        AdminFiles Files;
    };
}

TEST(AdminFilesTest, ServesTheIndexAtTheRootAndRevalidatesIt)
{
    Panel panel;
    for (std::string const path : { "/", "/index.html" })
    {
        AdminResponse const index = panel.Files.Serve(Ask(path));
        EXPECT_EQ(index.Status, 200) << path;
        EXPECT_EQ(index.ContentType, "text/html; charset=utf-8") << path;
        EXPECT_EQ(index.Body, "<!doctype html><title>Ambrose</title>") << path;
        EXPECT_EQ(HeaderValue(index, "Cache-Control"), "no-cache") << path;
    }
}

TEST(AdminFilesTest, CachesFingerprintedAssetsForAYear)
{
    Panel panel;
    AdminResponse const script = panel.Files.Serve(Ask("/assets/index-abc123.js"));
    EXPECT_EQ(script.Status, 200);
    EXPECT_EQ(script.ContentType, "text/javascript; charset=utf-8");
    EXPECT_EQ(script.Body, "console.log(1);");
    EXPECT_EQ(HeaderValue(script, "Cache-Control"), "public, max-age=31536000, immutable");

    AdminResponse const style = panel.Files.Serve(Ask("/assets/index-abc123.css"));
    EXPECT_EQ(style.Status, 200);
    EXPECT_EQ(style.ContentType, "text/css; charset=utf-8");
}

TEST(AdminFilesTest, AnswersOnlyGetAndHead)
{
    Panel panel;
    EXPECT_EQ(panel.Files.Serve(Ask("/", "HEAD")).Status, 200);
    for (std::string const method : { "POST", "PUT", "DELETE", "OPTIONS" })
    {
        AdminResponse const refused = panel.Files.Serve(Ask("/", method));
        EXPECT_EQ(refused.Status, 405) << method;
        EXPECT_EQ(HeaderValue(refused, "Allow"), "GET, HEAD") << method;
    }
}

TEST(AdminFilesTest, RefusesEveryPathThatLeavesTheFolderOrIsNotPlain)
{
    Panel panel;
    std::vector<std::string> const paths{ "/../secret.txt", "/assets/../../secret.txt", "/assets/../index.html", "/%2e%2e/secret.txt", "/assets//index-abc123.js",
        "/assets/", "//secret.txt", "/.hidden.js", "/assets\\index-abc123.js", "/C:/Windows/win.ini", "/notes.md", "/assets/missing.js", "/index.html.",
        "/INDEX~1.HTM", "/index.html:stream", "" };
    for (std::string const& path : paths)
    {
        AdminResponse const refused = panel.Files.Serve(Ask(path));
        EXPECT_EQ(refused.Status, 404) << path;
        EXPECT_EQ(nlohmann::json::parse(refused.Body)["error"], "not_found") << path;
    }
}

TEST(AdminFilesTest, RefusesALinkThatLeadsOutOfTheFolder)
{
    Panel panel;
    std::error_code code;
    std::filesystem::create_symlink(panel.Directory.Path() / "secret.txt", panel.Directory.Path() / "dist" / "leak.txt", code);
    if (code)
        GTEST_SKIP() << "this system does not let the test make a link: " << code.message();
    EXPECT_EQ(panel.Files.Serve(Ask("/leak.txt")).Status, 404);
}

TEST(AdminFilesTest, NamesTheSettingWhenNoPanelIsBuilt)
{
    LogTestDirectory empty;
    AdminFiles files;
    for (bool const rooted : { false, true })
    {
        if (rooted)
            files.SetRoot(empty.Path());
        AdminResponse const missing = files.Serve(Ask("/"));
        EXPECT_EQ(missing.Status, 404);
        nlohmann::json const body = nlohmann::json::parse(missing.Body);
        EXPECT_EQ(body["error"], "panel_not_installed");
        EXPECT_NE(body["message"].get<std::string>().find("Admin.DashboardDir"), std::string::npos);
    }
}

TEST(AdminFilesTest, KnowsOnlyTheTypesTheBuildWrites)
{
    EXPECT_EQ(AdminFiles::ContentType("a.woff2"), "font/woff2");
    EXPECT_EQ(AdminFiles::ContentType("a.SVG"), "image/svg+xml");
    EXPECT_FALSE(AdminFiles::ContentType("a.exe").has_value());
    EXPECT_FALSE(AdminFiles::ContentType("a.md").has_value());
    EXPECT_FALSE(AdminFiles::ContentType("noextension").has_value());
}
