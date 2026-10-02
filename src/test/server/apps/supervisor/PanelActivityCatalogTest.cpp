/*
 * Project Ambrose by Imjustchico
 * Tests the activity event catalog against the supervisor's own source: every event name written there as a literal, and every power action the relay builds a name from, has a sentence, so adding an event without one fails here; a rendered sentence has no placeholder left, cuts a long value and shows a control character as an escape, the catalog is sorted with no name twice, and console commands and refused file paths are the high-volume class while sign-ins are security.
 */

#include "ConfigMgr.h"
#include "ManagedApp.h"
#include "PanelActivityCatalog.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace
{
    std::set<std::string> EmittedNames()
    {
        std::set<std::string> names;
        std::regex const literal(R"re("([a-z_]+:[a-z_]+(?:\.[a-z_]+)+)")re");
        std::filesystem::path const root = ConfigMgr::PathFromUtf8(AMBROSE_SUPERVISOR_SOURCE);
        for (std::filesystem::directory_entry const& entry : std::filesystem::recursive_directory_iterator(root))
        {
            if (!entry.is_regular_file())
                continue;
            std::filesystem::path const& path = entry.path();
            if (path.extension() != ".cpp" && path.extension() != ".h")
                continue;
            if (path.filename() == "PanelActivityCatalog.cpp")
                continue;
            std::ifstream stream(path, std::ios::binary);
            std::string const text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
            for (std::sregex_iterator match(text.begin(), text.end(), literal), end; match != end; ++match)
                names.insert((*match)[1].str());
        }
        for (PowerAction const action : { PowerAction::Start, PowerAction::Stop, PowerAction::Restart, PowerAction::Kill })
            names.insert("app:power." + std::string(ManagedApp::ActionName(action)));
        return names;
    }
}

TEST(PanelActivityCatalogTest, EveryEventNameTheSupervisorEmitsRendersASentence)
{
    std::set<std::string> const names = EmittedNames();
    ASSERT_GT(names.size(), 30u) << "the scan of " << AMBROSE_SUPERVISOR_SOURCE << " found too few event names to be reading the source";
    for (std::string const& name : names)
    {
        PanelActivitySentence const* const sentence = PanelActivityCatalog::Find(name);
        EXPECT_NE(sentence, nullptr) << name << " is emitted but has no sentence in PanelActivityCatalog.cpp";
        AuditEvent event;
        event.Name = name;
        std::string const rendered = PanelActivityCatalog::Render(event);
        EXPECT_EQ(rendered.find('{'), std::string::npos) << name << " left a placeholder: " << rendered;
        EXPECT_EQ(rendered.find(name), std::string::npos) << name << " rendered as its raw name: " << rendered;
    }
}

TEST(PanelActivityCatalogTest, TheCatalogIsSortedAndEveryNameIsNamespaced)
{
    std::regex const form(R"([a-z_]+:[a-z_]+(\.[a-z_]+)+)");
    std::string previous;
    for (PanelActivitySentence const& sentence : PanelActivityCatalog::All())
    {
        EXPECT_TRUE(std::regex_match(std::string(sentence.Name), form)) << sentence.Name;
        EXPECT_LT(previous, std::string(sentence.Name));
        EXPECT_FALSE(sentence.Text.empty()) << sentence.Name;
        previous = std::string(sentence.Name);
    }
}

TEST(PanelActivityCatalogTest, ValuesAreFilledCutAndEscaped)
{
    AuditEvent event;
    event.Name = "app:power.restart";
    event.Actor = AuditActor::User;
    event.ActorId = "7";
    event.ActorName = "<b>owner</b>\x07";
    event.On("panel_user", "7", event.ActorName).On("app", "gameserver", "gameserver");
    EXPECT_EQ(PanelActivityCatalog::Render(event), "<b>owner</b>\\x07 restarted gameserver");

    AuditEvent created;
    created.Name = "panel:user.created";
    created.ActorName = "owner";
    created.ActorId = "1";
    created.On("panel_user", "1", "owner").On("panel_user", "2", std::string(400, 'n'));
    std::string const rendered = PanelActivityCatalog::Render(created);
    EXPECT_TRUE(rendered.starts_with("owner added the operator nnnn")) << rendered;
    EXPECT_TRUE(rendered.ends_with("...")) << rendered;
    EXPECT_LT(rendered.size(), 200u);

    AuditEvent swept;
    swept.Name = "panel:activity.swept";
    swept.Properties = R"({"removed":12})";
    EXPECT_EQ(PanelActivityCatalog::Render(swept), "The retention sweep removed 12 activity rows");

    AuditEvent unknown;
    unknown.Name = "app:power.restart";
    EXPECT_EQ(PanelActivityCatalog::Render(unknown), "The panel restarted an app");
}

TEST(PanelActivityCatalogTest, HighVolumeEventsAreKeptForTheShorterWindow)
{
    EXPECT_EQ(PanelActivityCatalog::ClassOf("app:console.command"), PanelActivityClass::HighVolume);
    EXPECT_EQ(PanelActivityCatalog::ClassOf("file:path.refused"), PanelActivityClass::HighVolume);
    EXPECT_EQ(PanelActivityCatalog::ClassOf("panel:session.opened"), PanelActivityClass::Security);
    EXPECT_EQ(PanelActivityCatalog::ClassOf("app:power.restart"), PanelActivityClass::Security);
    EXPECT_EQ(PanelActivityCatalog::ClassOf("not:in.catalog"), PanelActivityClass::Security);
    std::vector<std::string> const high = PanelActivityCatalog::NamesOf(PanelActivityClass::HighVolume);
    EXPECT_NE(std::find(high.begin(), high.end(), "panel:request.throttled"), high.end());
}
