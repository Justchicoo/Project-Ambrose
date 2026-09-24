/*
 * Project Ambrose by Imjustchico
 * Tests that the configuration the launcher writes is its template with only the launcher's own values changed: every differing line holds one of the keys it sets, a template with CRLF endings comes out byte for byte the same apart from those values, so do unusual spacing, attribute order and a declaration of the template's own, a key the template holds twice is emptied in both places while the window keys follow the VideoSettings table alone, a template missing the tables and keys takes them in its own indentation with _TableList updated, the defaultconfig.xml of the Root.wad fallback keeps everything but its root element's name, and content after the root element and a missing last newline are kept.
 */

#include "ClientRunFolder.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr std::string_view Lf = "\n";
    constexpr std::string_view Crlf = "\r\n";

    RunFolderOptions Window(unsigned width, unsigned height)
    {
        RunFolderOptions options;
        options.Width = width;
        options.Height = height;
        return options;
    }

    std::string Join(std::vector<std::string> const& lines, std::string_view lineEnd)
    {
        std::string text;
        for (std::string const& line : lines)
        {
            text.append(line);
            text.append(lineEnd);
        }
        return text;
    }

    std::vector<std::string> Split(std::string const& text, std::string_view lineEnd)
    {
        std::vector<std::string> lines;
        std::size_t at = 0;
        for (;;)
        {
            std::size_t const found = text.find(lineEnd, at);
            if (found == std::string::npos)
            {
                lines.push_back(text.substr(at));
                return lines;
            }
            lines.push_back(text.substr(at, found - at));
            at = found + lineEnd.size();
        }
    }

    void SetLine(std::vector<std::string>& lines, std::string_view holds, std::string line)
    {
        for (std::string& held : lines)
            if (held.find(holds) != std::string::npos)
            {
                held = std::move(line);
                return;
            }
    }

    std::vector<std::string> RetailShapedConfig()
    {
        return {
            "<?xml version=\"1.0\" ?>",
            "<config>",
            "<_TableList>",
            "  <RECORD>",
            "    <Name TYPE=\"STR\">GameSettings</Name>",
            "  </RECORD>",
            "  <RECORD>",
            "    <Name TYPE=\"STR\">VersionInfo</Name>",
            "  </RECORD>",
            "  <RECORD>",
            "    <Name TYPE=\"STR\">VideoSettings</Name>",
            "  </RECORD>",
            "</_TableList>",
            "<GameSettings>",
            "  <RECORD>",
            "    <AccessPassWebPage TYPE=\"STR\">http://mmo.example.invalid/play/accesspass</AccessPassWebPage>",
            "    <SilentMetricsURL TYPE=\"STR\">https://www.example.invalid/static/noop.html</SilentMetricsURL>",
            "    <QuestHelperEnabled TYPE=\"INT\">1</QuestHelperEnabled>",
            "  </RECORD>",
            "</GameSettings>",
            "<VersionInfo>",
            "  <RECORD>",
            "    <VersionNumber TYPE=\"UINT\">128</VersionNumber>",
            "  </RECORD>",
            "</VersionInfo>",
            "<VideoSettings>",
            "  <RECORD>",
            "    <BloomActive TYPE=\"INT\">1</BloomActive>",
            "    <IsFullscreen TYPE=\"INT\">1</IsFullscreen>",
            "    <RefreshRate TYPE=\"STR\">65</RefreshRate>",
            "    <Resolution TYPE=\"STR\">1920x1080</Resolution>",
            "    <WindowedX TYPE=\"INT\">8</WindowedX>",
            "    <WindowedY TYPE=\"INT\">31</WindowedY>",
            "    <UIScale TYPE=\"FLT\">10.000000</UIScale>",
            "  </RECORD>",
            "</VideoSettings>",
            "</config>"
        };
    }

    std::vector<std::string> RetailShapedConfigAsWritten()
    {
        std::vector<std::string> lines = RetailShapedConfig();
        SetLine(lines, "<SilentMetricsURL", "    <SilentMetricsURL TYPE=\"STR\"></SilentMetricsURL>");
        SetLine(lines, "<IsFullscreen", "    <IsFullscreen TYPE=\"INT\">0</IsFullscreen>");
        SetLine(lines, "<Resolution", "    <Resolution TYPE=\"STR\">1024x768</Resolution>");
        return lines;
    }

    void ExpectOnlyTheseKeysDiffer(std::string const& templateText, std::string const& written, std::vector<std::string> const& keys, std::string_view lineEnd)
    {
        std::vector<std::string> const before = Split(templateText, lineEnd);
        std::vector<std::string> const after = Split(written, lineEnd);
        ASSERT_EQ(before.size(), after.size()) << written;
        for (std::size_t index = 0; index < before.size(); ++index)
        {
            if (before[index] == after[index])
                continue;
            bool named = false;
            for (std::string const& key : keys)
                if (before[index].find(key) != std::string::npos && after[index].find(key) != std::string::npos)
                    named = true;
            EXPECT_TRUE(named) << "line " << index + 1 << " went from " << before[index] << " to " << after[index];
        }
    }
}

TEST(LauncherConfigTextTest, EveryLineThatDiffersFromTheTemplateHoldsAKeyTheLauncherSets)
{
    std::string const templateText = Join(RetailShapedConfig(), Lf);
    std::string error;
    std::optional<std::string> const written = ClientRunFolder::Generate(templateText, "config", Window(1024, 768), error);
    ASSERT_TRUE(written) << error;
    ExpectOnlyTheseKeysDiffer(templateText, *written, { "IsFullscreen", "Resolution", "WindowedX", "WindowedY", "SilentMetricsURL" }, Lf);
    EXPECT_EQ(*written, Join(RetailShapedConfigAsWritten(), Lf));
}

TEST(LauncherConfigTextTest, ATemplateWithCrlfEndingsKeepsThemAndEveryOtherByte)
{
    std::string const templateText = Join(RetailShapedConfig(), Crlf);
    std::string error;
    std::optional<std::string> const written = ClientRunFolder::Generate(templateText, "config", Window(1024, 768), error);
    ASSERT_TRUE(written) << error;
    EXPECT_EQ(*written, Join(RetailShapedConfigAsWritten(), Crlf));
    ExpectOnlyTheseKeysDiffer(templateText, *written, { "IsFullscreen", "Resolution", "SilentMetricsURL" }, Crlf);
}

TEST(LauncherConfigTextTest, TheDeclarationSpacingAndAttributeOrderOfTheTemplateAreKept)
{
    std::vector<std::string> const lines = {
        "<?xml   version='1.0'  ?>",
        "<config >",
        "<VideoSettings   >",
        "\t<RECORD >",
        "\t\t<Resolution UITYPE=\"LIST\" TYPE=\"STR\" UIDATA=\"1:autodetect\" >autodetect</Resolution>",
        "\t\t<IsFullscreen TYPE = \"INT\" >1</IsFullscreen>",
        "\t</RECORD >",
        "</VideoSettings >",
        "<GameSettings>",
        "  <RECORD>",
        "    <SilentMetricsURL  TYPE='STR'>https://www.example.invalid/x</SilentMetricsURL>",
        "  </RECORD>",
        "</GameSettings>",
        "</config>"
    };
    std::vector<std::string> written = lines;
    SetLine(written, "<Resolution", "\t\t<Resolution UITYPE=\"LIST\" TYPE=\"STR\" UIDATA=\"1:autodetect\" >1024x768</Resolution>");
    SetLine(written, "<IsFullscreen", "\t\t<IsFullscreen TYPE = \"INT\" >0</IsFullscreen>");
    SetLine(written, "<SilentMetricsURL", "    <SilentMetricsURL  TYPE='STR'></SilentMetricsURL>");
    std::string error;
    std::optional<std::string> const held = ClientRunFolder::Generate(Join(lines, Lf), "config", Window(1024, 768), error);
    ASSERT_TRUE(held) << error;
    EXPECT_EQ(*held, Join(written, Lf));
}

TEST(LauncherConfigTextTest, AKeyTheTemplateHoldsTwiceIsEmptiedInBothPlacesAndTheWindowFollowsItsOwnTable)
{
    std::vector<std::string> const lines = {
        "<?xml version=\"1.0\" ?>",
        "<preferences>",
        "<GameSettings>",
        "  <RECORD>",
        "    <SilentMetricsURL TYPE=\"STR\">https://www.example.invalid/one</SilentMetricsURL>",
        "  </RECORD>",
        "  <RECORD>",
        "    <SilentMetricsURL TYPE=\"STR\">https://www.example.invalid/two</SilentMetricsURL>",
        "  </RECORD>",
        "</GameSettings>",
        "<DebugSettings>",
        "  <RECORD>",
        "    <Resolution TYPE=\"STR\">1920x1080</Resolution>",
        "    <SilentMetricsURL TYPE=\"STR\">https://www.example.invalid/three</SilentMetricsURL>",
        "  </RECORD>",
        "</DebugSettings>",
        "<VideoSettings>",
        "  <RECORD>",
        "    <Resolution TYPE=\"STR\">1920x1080</Resolution>",
        "  </RECORD>",
        "  <RECORD>",
        "    <Resolution TYPE=\"STR\">1280x1024</Resolution>",
        "  </RECORD>",
        "</VideoSettings>",
        "</preferences>"
    };
    std::vector<std::string> written = lines;
    written[4] = "    <SilentMetricsURL TYPE=\"STR\"></SilentMetricsURL>";
    written[7] = "    <SilentMetricsURL TYPE=\"STR\"></SilentMetricsURL>";
    written[13] = "    <SilentMetricsURL TYPE=\"STR\"></SilentMetricsURL>";
    written[18] = "    <Resolution TYPE=\"STR\">1024x768</Resolution>";
    written[21] = "    <Resolution TYPE=\"STR\">1024x768</Resolution>";
    written.insert(written.begin() + 19, "    <IsFullscreen TYPE=\"INT\">0</IsFullscreen>");
    std::string error;
    std::optional<std::string> const held = ClientRunFolder::Generate(Join(lines, Crlf), "preferences", Window(1024, 768), error);
    ASSERT_TRUE(held) << error;
    EXPECT_EQ(*held, Join(written, Crlf));
}

TEST(LauncherConfigTextTest, ATemplateWithoutTheTablesAndKeysTakesThemInItsOwnIndentationAndListsThem)
{
    std::vector<std::string> const lines = {
        "<?xml version=\"1.0\" ?>",
        "<config>",
        "    <_TableList>",
        "        <RECORD>",
        "            <Name TYPE=\"STR\">VersionInfo</Name>",
        "        </RECORD>",
        "    </_TableList>",
        "    <VersionInfo>",
        "        <RECORD>",
        "            <VersionNumber TYPE=\"UINT\">128</VersionNumber>",
        "        </RECORD>",
        "    </VersionInfo>",
        "</config>"
    };
    std::vector<std::string> const written = {
        "<?xml version=\"1.0\" ?>",
        "<config>",
        "    <_TableList>",
        "        <RECORD>",
        "            <Name TYPE=\"STR\">VersionInfo</Name>",
        "        </RECORD>",
        "        <RECORD>",
        "            <Name TYPE=\"STR\">VideoSettings</Name>",
        "        </RECORD>",
        "        <RECORD>",
        "            <Name TYPE=\"STR\">GameSettings</Name>",
        "        </RECORD>",
        "    </_TableList>",
        "    <VersionInfo>",
        "        <RECORD>",
        "            <VersionNumber TYPE=\"UINT\">128</VersionNumber>",
        "        </RECORD>",
        "    </VersionInfo>",
        "    <VideoSettings>",
        "        <RECORD>",
        "            <IsFullscreen TYPE=\"INT\">0</IsFullscreen>",
        "            <Resolution TYPE=\"STR\">1024x768</Resolution>",
        "            <WindowedX TYPE=\"INT\">12</WindowedX>",
        "            <WindowedY TYPE=\"INT\">34</WindowedY>",
        "        </RECORD>",
        "    </VideoSettings>",
        "    <GameSettings>",
        "        <RECORD>",
        "            <SilentMetricsURL TYPE=\"STR\"></SilentMetricsURL>",
        "        </RECORD>",
        "    </GameSettings>",
        "</config>"
    };
    RunFolderOptions options = Window(1024, 768);
    options.WindowX = 12;
    options.WindowY = 34;
    std::string error;
    std::optional<std::string> const held = ClientRunFolder::Generate(Join(lines, Crlf), "config", options, error);
    ASSERT_TRUE(held) << error;
    EXPECT_EQ(*held, Join(written, Crlf));
}

TEST(LauncherConfigTextTest, TheArchiveFallbackKeepsEverythingButItsRootElementsName)
{
    std::vector<std::string> const lines = {
        "<?xml version=\"1.0\" ?>",
        "<defaultconfig>",
        "<VideoSettings>",
        "  <RECORD>",
        "    <IsFullscreen TYPE=\"INT\" UITYPE=\"LIST\" UIDATA=\"1:Off,On,On Borderless\">1</IsFullscreen>",
        "    <Resolution TYPE=\"STR\" UITYPE=\"LIST\" UIDATA=\"1:autodetect\">autodetect</Resolution>",
        "  </RECORD>",
        "</VideoSettings>",
        "<GameSettings>",
        "  <RECORD>",
        "    <SilentMetricsURL TYPE=\"STR\">https://www.example.invalid/static/noop.html</SilentMetricsURL>",
        "  </RECORD>",
        "</GameSettings>",
        "</defaultconfig>"
    };
    std::vector<std::string> written = lines;
    SetLine(written, "<defaultconfig>", "<config>");
    SetLine(written, "</defaultconfig>", "</config>");
    SetLine(written, "<IsFullscreen", "    <IsFullscreen TYPE=\"INT\" UITYPE=\"LIST\" UIDATA=\"1:Off,On,On Borderless\">0</IsFullscreen>");
    SetLine(written, "<Resolution", "    <Resolution TYPE=\"STR\" UITYPE=\"LIST\" UIDATA=\"1:autodetect\">1280x720</Resolution>");
    SetLine(written, "<SilentMetricsURL", "    <SilentMetricsURL TYPE=\"STR\"></SilentMetricsURL>");
    std::string error;
    std::optional<std::string> const held = ClientRunFolder::Generate(Join(lines, Crlf), "config", Window(1280, 720), error);
    ASSERT_TRUE(held) << error;
    EXPECT_EQ(*held, Join(written, Crlf));
}

TEST(LauncherConfigTextTest, ContentAfterTheRootElementAndAMissingLastNewlineAreKept)
{
    std::vector<std::string> const lines = {
        "<?xml version=\"1.0\" ?>",
        "<config>",
        "<VideoSettings>",
        "  <RECORD>",
        "    <Resolution TYPE=\"STR\">1920x1080</Resolution>",
        "    <IsFullscreen TYPE=\"INT\">1</IsFullscreen>",
        "  </RECORD>",
        "</VideoSettings>",
        "<GameSettings>",
        "  <RECORD>",
        "    <SilentMetricsURL TYPE=\"STR\">https://www.example.invalid/x</SilentMetricsURL>",
        "  </RECORD>",
        "</GameSettings>",
        "</config>"
    };
    std::vector<std::string> written = lines;
    SetLine(written, "<Resolution", "    <Resolution TYPE=\"STR\">1024x768</Resolution>");
    SetLine(written, "<IsFullscreen", "    <IsFullscreen TYPE=\"INT\">0</IsFullscreen>");
    SetLine(written, "<SilentMetricsURL", "    <SilentMetricsURL TYPE=\"STR\"></SilentMetricsURL>");
    std::string const trailing = "<!-- the client's own footer -->";
    std::string error;
    std::optional<std::string> const held = ClientRunFolder::Generate(Join(lines, Lf) + trailing, "config", Window(1024, 768), error);
    ASSERT_TRUE(held) << error;
    EXPECT_EQ(*held, Join(written, Lf) + trailing);
}
