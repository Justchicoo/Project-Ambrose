/*
 * Project Ambrose by Imjustchico
 * Holds doc/config/settings.md to the declarations it is written from: the page must be exactly what the table renders, so a setting added, retuned or removed without the page following fails here; run this test with AMBROSE_WRITE_SETTINGS_DOC=1 to write the page from the table.
 */

#include "ConfigMgr.h"
#include "Environment.h"
#include "SettingDeclarations.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

TEST(SettingsDocTest, TheSettingsPageIsWhatTheDeclarationsRender)
{
    std::filesystem::path const page = ConfigMgr::PathFromUtf8(AMBROSE_SETTINGS_DOC);
    std::string const rendered = SettingDeclarations::RenderDocument();
    if (std::optional<std::string> const write = Ambrose::GetEnv("AMBROSE_WRITE_SETTINGS_DOC"); write && *write == "1")
        std::ofstream(page, std::ios::binary | std::ios::trunc) << rendered;
    std::ifstream stream(page, std::ios::binary);
    ASSERT_TRUE(stream) << "doc/config/settings.md is missing; run this test with AMBROSE_WRITE_SETTINGS_DOC=1 to write it";
    std::string const held((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    EXPECT_EQ(held, rendered) << "doc/config/settings.md no longer matches the declarations; run this test with AMBROSE_WRITE_SETTINGS_DOC=1 to write it again";
}
