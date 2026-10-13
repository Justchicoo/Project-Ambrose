/*
 * Project Ambrose by Imjustchico
 * Tests that a wizard's class project answer follows Events.ClassProjects as a live setting: a listed project is active whatever its case and spacing, one not listed or with no name is not, and clearing the setting ends every project with nothing restarted.
 */

#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "MemorySettingStore.h"
#include "QuestLog.h"
#include "Settings.h"
#include "WizardRequirementContext.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

TEST(WizardRequirementContextTest, AClassProjectIsActiveOnlyWhileEventsClassProjectsListsIt)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Write("gameserver.conf", "Events.ClassProjects = DuelArena\n");
    ConfigMgr config([](std::string const&) -> std::optional<std::string> { return std::nullopt; });
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());
    sSettings.Clear();
    std::vector<std::string> errors;
    ASSERT_TRUE(sSettings.DeclareFor(SettingApps::Game, errors)) << (errors.empty() ? "" : errors.front());
    std::vector<std::string> warnings;
    ASSERT_TRUE(sSettings.Start(config, std::make_shared<MemorySettingStore>(), warnings));

    QuestLog const log;
    WizardRequirementContext const wizard(log, "Fire", 10, "WizardCity/WC_Duel_Arena");
    EXPECT_EQ(wizard.IsClassProjectActive("duelarena"), std::optional<bool>(true));
    EXPECT_EQ(wizard.IsClassProjectActive("Gauntlet"), std::optional<bool>(false));
    EXPECT_EQ(wizard.IsClassProjectActive(""), std::optional<bool>(false));

    SettingAuthor const author{ "test", 1, "unit_test" };
    ASSERT_TRUE(sSettings.Set("Events.ClassProjects", " Gauntlet , DuelArena ", author, "two projects").Ok());
    EXPECT_EQ(wizard.IsClassProjectActive("Gauntlet"), std::optional<bool>(true));
    EXPECT_EQ(wizard.IsClassProjectActive("DuelArena"), std::optional<bool>(true));
    ASSERT_TRUE(sSettings.Set("Events.ClassProjects", "", author, "event over").Ok());
    EXPECT_EQ(wizard.IsClassProjectActive("DuelArena"), std::optional<bool>(false));
    sSettings.Clear();
}
