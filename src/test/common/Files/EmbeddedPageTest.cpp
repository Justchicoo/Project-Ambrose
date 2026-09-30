/*
 * Project Ambrose by Imjustchico
 * Checks the manifest ambrose_embed_page generates and how a compiled-in page answers: every file of the fixture folder is there byte for byte with the media type its extension names and an entity tag drawn from its SHA-256, only files under assets/ are marked hashed and given an immutable cache header, the root and an unknown path outside the API prefix answer index.html while an unknown path under the API prefix answers 404, a query or fragment is ignored and a parent segment never matches, and a folder with no built page embeds a placeholder index that says why.
 */

#include "EmbeddedPage.h"
#include "Hex.h"
#include "SHA256.h"
#include "TestEmbeddedPage.h"
#include "TestPlaceholderPage.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

namespace
{
    std::string Header(EmbeddedAnswer const& answer, std::string const& name)
    {
        for (auto const& [key, value] : answer.Headers)
            if (key == name)
                return value;
        return std::string();
    }

    std::string ReadFixture(std::string const& relative)
    {
        std::ifstream file(std::filesystem::path(AMBROSE_EMBED_FIXTURE) / relative, std::ios::binary);
        std::ostringstream text;
        text << file.rdbuf();
        return text.str();
    }
}

TEST(EmbeddedPageTest, EveryFixtureFileIsThereWithItsTypeTagAndBytes)
{
    EmbeddedPage const& page = TestEmbeddedPage();
    EXPECT_FALSE(page.IsPlaceholder());
    std::map<std::string, std::string> const types{
        { "index.html", "text/html; charset=utf-8" },
        { "assets/app-Bx3kQ9aZ.js", "text/javascript; charset=utf-8" },
        { "assets/app-Bx3kQ9aZ.css", "text/css; charset=utf-8" },
        { "assets/empty-Q1w2e3r4.txt", "text/plain; charset=utf-8" },
        { "favicon.ico", "image/x-icon" },
        { "robots.txt", "text/plain; charset=utf-8" },
        { "data.bin", "application/octet-stream" },
    };
    ASSERT_EQ(page.Files().size(), types.size());
    for (auto const& [path, type] : types)
    {
        EmbeddedFile const* const file = page.Find("/" + path);
        ASSERT_NE(file, nullptr) << path;
        EXPECT_EQ(file->MediaType, type) << path;
        std::string const bytes = ReadFixture(path);
        EXPECT_EQ(file->Bytes, bytes) << path;
        std::string const digest = Hex::Encode(SHA256::GetDigestOf(std::string_view(bytes)), Hex::Case::Lower);
        EXPECT_EQ(file->EntityTag, "\"" + digest.substr(0, 16) + "\"") << path;
        EXPECT_EQ(file->Hashed, path.starts_with("assets/")) << path;
    }
}

TEST(EmbeddedPageTest, OnlyHashedAssetsAreCachedForever)
{
    EmbeddedPage const& page = TestEmbeddedPage();
    EmbeddedAnswer const asset = page.Serve("/assets/app-Bx3kQ9aZ.js");
    ASSERT_EQ(asset.Status, 200);
    EXPECT_EQ(Header(asset, "Cache-Control"), "public, max-age=31536000, immutable");
    EXPECT_EQ(Header(asset, "Content-Type"), "text/javascript; charset=utf-8");
    EXPECT_EQ(Header(asset, "ETag"), std::string(asset.File->EntityTag));

    for (std::string const path : { "/", "/index.html", "/robots.txt", "/favicon.ico" })
    {
        EmbeddedAnswer const answer = page.Serve(path);
        ASSERT_EQ(answer.Status, 200) << path;
        EXPECT_EQ(Header(answer, "Cache-Control"), "no-cache") << path;
        EXPECT_FALSE(Header(answer, "ETag").empty()) << path;
    }
}

TEST(EmbeddedPageTest, UnknownPathsAnswerTheIndexOutsideTheApiPrefixAnd404WithinIt)
{
    EmbeddedPage const& page = TestEmbeddedPage();
    EmbeddedFile const* const index = page.Find("index.html");
    ASSERT_NE(index, nullptr);
    for (std::string const path : { "/", "/servers/realm-1", "/assets/missing-00000000.js", "/../index.html", "/robots.txt/..", "/index.html?next=/x#top" })
    {
        EmbeddedAnswer const answer = page.Serve(path);
        EXPECT_EQ(answer.Status, 200) << path;
        EXPECT_EQ(answer.File, index) << path;
        EXPECT_EQ(Header(answer, "Content-Type"), "text/html; charset=utf-8") << path;
    }
    for (std::string const path : { "/api/", "/api", "/api/health", "/api/panel/unknown?x=1" })
    {
        EmbeddedAnswer const answer = page.Serve(path);
        EXPECT_EQ(answer.Status, 404) << path;
        EXPECT_EQ(answer.File, nullptr) << path;
    }
    EXPECT_EQ(page.Serve("/robots.txt?v=2").File, page.Find("robots.txt"));
    EXPECT_EQ(page.Find("/assets/../index.html"), nullptr);
}

TEST(EmbeddedPageTest, AFolderWithNoBuiltPageEmbedsAPlaceholderThatSaysWhy)
{
    EmbeddedPage const& page = TestPlaceholderPage();
    EXPECT_TRUE(page.IsPlaceholder());
    ASSERT_EQ(page.Files().size(), 1u);
    EmbeddedAnswer const answer = page.Serve("/anything");
    ASSERT_EQ(answer.Status, 200);
    EXPECT_EQ(answer.File->Path, "index.html");
    EXPECT_NE(answer.File->Bytes.find("This build has no page."), std::string_view::npos);
    EXPECT_EQ(page.Serve("/api/health").Status, 404);
}
