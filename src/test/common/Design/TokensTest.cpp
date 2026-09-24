/*
 * Project Ambrose by Imjustchico
 * Tests that the generated terminal header holds every meaning in a usable terminal form and carries the same value for every token as the generated stylesheet.
 */

#include "Tokens.h"

#include <gtest/gtest.h>

#include <fstream>
#include <map>
#include <sstream>
#include <string>

namespace
{
    std::string ReadStylesheet()
    {
        std::ifstream file(AMBROSE_DESIGN_TOKENS_CSS, std::ios::binary);
        std::ostringstream text;
        text << file.rdbuf();
        return text.str();
    }

    std::map<std::string, std::string> Declarations(std::string const& css, std::string const& selector)
    {
        std::map<std::string, std::string> found;
        std::size_t const start = css.find(selector + " {");
        if (start == std::string::npos)
        {
            return found;
        }
        std::size_t const end = css.find("\n}", start);
        std::istringstream block(css.substr(start, end - start));
        std::string line;
        while (std::getline(block, line))
        {
            std::size_t const prefix = line.find("--ambrose-color-");
            if (prefix == std::string::npos)
            {
                continue;
            }
            std::size_t const colon = line.find(':', prefix);
            std::size_t const semicolon = line.find(';', colon);
            if (colon == std::string::npos || semicolon == std::string::npos)
            {
                continue;
            }
            std::string const name = line.substr(prefix + 16, colon - prefix - 16);
            std::string value = line.substr(colon + 1, semicolon - colon - 1);
            while (!value.empty() && value.front() == ' ')
            {
                value.erase(value.begin());
            }
            found.emplace(name, value);
        }
        return found;
    }
}

TEST(DesignTokensTest, EveryMeaningIsInTheHeaderOnce)
{
    ASSERT_EQ(Ambrose::Design::DarkTokens.size(), Ambrose::Design::LightTokens.size());
    for (std::size_t index = 0; index < Ambrose::Design::DarkTokens.size(); ++index)
    {
        EXPECT_EQ(Ambrose::Design::DarkTokens[index].Name, Ambrose::Design::LightTokens[index].Name);
        for (std::size_t other = index + 1; other < Ambrose::Design::DarkTokens.size(); ++other)
        {
            EXPECT_NE(Ambrose::Design::DarkTokens[index].Name, Ambrose::Design::DarkTokens[other].Name);
        }
    }
    EXPECT_NE(Ambrose::Design::Find(Ambrose::Design::DarkTokens, "state-healthy"), nullptr);
    EXPECT_EQ(Ambrose::Design::Find(Ambrose::Design::DarkTokens, "state-lilac"), nullptr);
}

TEST(DesignTokensTest, EveryTokenCarriesItsThreeTerminalForms)
{
    for (Ambrose::Design::TerminalColor const& token : Ambrose::Design::DarkTokens)
    {
        EXPECT_EQ(token.Hex.size(), 7u) << token.Name;
        EXPECT_EQ(token.Hex.front(), '#') << token.Name;
        EXPECT_LE(token.Index256, 255u) << token.Name;
        EXPECT_LE(token.Index16, 15u) << token.Name;
    }
    Ambrose::Design::TerminalColor const* healthy = Ambrose::Design::Find(Ambrose::Design::DarkTokens, "state-healthy");
    ASSERT_NE(healthy, nullptr);
    EXPECT_EQ(healthy->Index16, 14u);
    Ambrose::Design::TerminalColor const* wrong = Ambrose::Design::Find(Ambrose::Design::DarkTokens, "state-wrong");
    ASSERT_NE(wrong, nullptr);
    EXPECT_EQ(wrong->Index16, 9u);
}

TEST(DesignTokensTest, TheHexAndTheChannelsAgree)
{
    for (Ambrose::Design::TerminalColor const& token : Ambrose::Design::DarkTokens)
    {
        std::uint32_t const packed = static_cast<std::uint32_t>(std::stoul(std::string(token.Hex.substr(1)), nullptr, 16));
        EXPECT_EQ(token.Red, (packed >> 16) & 0xFFu) << token.Name;
        EXPECT_EQ(token.Green, (packed >> 8) & 0xFFu) << token.Name;
        EXPECT_EQ(token.Blue, packed & 0xFFu) << token.Name;
    }
}

TEST(DesignTokensTest, TheHeaderAndTheStylesheetHoldTheSameValues)
{
    std::string const css = ReadStylesheet();
    ASSERT_FALSE(css.empty()) << "the generated stylesheet is missing; run apps/designtokens/designtokens.py";

    std::map<std::string, std::string> const dark = Declarations(css, ":root");
    std::map<std::string, std::string> const light = Declarations(css, ":root[data-theme=\"light\"]");
    ASSERT_FALSE(dark.empty());
    ASSERT_FALSE(light.empty());

    for (Ambrose::Design::TerminalColor const& token : Ambrose::Design::DarkTokens)
    {
        auto const entry = dark.find(std::string(token.Name));
        ASSERT_NE(entry, dark.end()) << token.Name;
        EXPECT_EQ(entry->second, std::string(token.Hex)) << token.Name;
    }
    for (Ambrose::Design::TerminalColor const& token : Ambrose::Design::LightTokens)
    {
        auto const entry = light.find(std::string(token.Name));
        ASSERT_NE(entry, light.end()) << token.Name;
        EXPECT_EQ(entry->second, std::string(token.Hex)) << token.Name;
    }
    for (Ambrose::Design::TerminalColor const& token : Ambrose::Design::DarkSeries)
    {
        auto const entry = dark.find("series-" + std::string(token.Name.substr(7)));
        ASSERT_NE(entry, dark.end()) << token.Name;
        EXPECT_EQ(entry->second, std::string(token.Hex)) << token.Name;
    }
}

TEST(DesignTokensTest, TheDurationsAreTheFourTheDesignAllows)
{
    EXPECT_EQ(Ambrose::Design::DurationFlipMs, 90u);
    EXPECT_EQ(Ambrose::Design::DurationHoverMs, 120u);
    EXPECT_EQ(Ambrose::Design::DurationPanelMs, 200u);
    EXPECT_EQ(Ambrose::Design::DurationScreenMs, 320u);
}
