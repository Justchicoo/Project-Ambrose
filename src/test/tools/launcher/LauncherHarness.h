/*
 * Project Ambrose by Imjustchico
 * A machine for launcher tests: a Windows machine holding one install with the client program, its own config.xml and preferences.xml and its revision and data files, an in-memory run folder, a prompt nobody can answer, and helpers that prepare a plan, write its run folder and read one argument of the command.
 */

#ifndef AMBROSE_LAUNCHERHARNESS_H
#define AMBROSE_LAUNCHERHARNESS_H

#include "FakeClientSystem.h"
#include "FakeLauncherFiles.h"
#include "Launcher.h"
#include "ScriptedPromptInput.h"
#include "SetupPrompt.h"

#include <chrono>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace LauncherTestData
{
    constexpr char const* Install = "C:/ProgramData/KingsIsle Entertainment/Wizard101";
    constexpr char const* Revision = "r806919.Wizard_1_610";
    constexpr char const* DataFolder = "C:/Users/wiz/AppData/Local/ProjectAmbrose";
    constexpr char const* RunFolder = "C:/Users/wiz/AppData/Local/ProjectAmbrose/client/r806919.Wizard_1_610";

    constexpr char const* ConfigTemplate = R"(<?xml version="1.0" ?>
<config>
<_TableList>
  <RECORD>
    <Name TYPE="STR">GameSettings</Name>
  </RECORD>
  <RECORD>
    <Name TYPE="STR">VersionInfo</Name>
  </RECORD>
  <RECORD>
    <Name TYPE="STR">VideoSettings</Name>
  </RECORD>
</_TableList>
<GameSettings>
  <RECORD>
    <QuestHelperEnabled TYPE="INT">1</QuestHelperEnabled>
    <SilentMetricsURL TYPE="STR">https:example.invalid/metrics</SilentMetricsURL>
  </RECORD>
</GameSettings>
<VersionInfo>
  <RECORD>
    <VersionNumber TYPE="UINT">128</VersionNumber>
    <PrefVersionNumber TYPE="UINT">32</PrefVersionNumber>
  </RECORD>
</VersionInfo>
<VideoSettings>
  <RECORD>
    <IsFullscreen TYPE="INT">1</IsFullscreen>
    <Resolution TYPE="STR">1920x1080</Resolution>
    <WindowedX TYPE="INT">8</WindowedX>
    <WindowedY TYPE="INT">31</WindowedY>
    <UIScale TYPE="FLT">10.000000</UIScale>
  </RECORD>
</VideoSettings>
</config>
)";

    constexpr char const* PreferencesTemplate = R"(<?xml version="1.0" ?>
<preferences>
<_TableList>
  <RECORD>
    <Name TYPE="STR">SoundSettings</Name>
  </RECORD>
  <RECORD>
    <Name TYPE="STR">VideoSettings</Name>
  </RECORD>
</_TableList>
<SoundSettings>
  <RECORD>
    <EnableSound TYPE="INT">1</EnableSound>
  </RECORD>
</SoundSettings>
<VideoSettings>
  <RECORD>
    <IsFullscreen TYPE="INT">1</IsFullscreen>
    <Resolution TYPE="STR">1920x1080</Resolution>
  </RECORD>
</VideoSettings>
</preferences>
)";
}

struct LauncherHarness
{
    FakeClientSystem System;
    FakeLauncherFiles Files;
    std::ostringstream Out;
    std::ostringstream Err;
    std::string Error;
    std::shared_ptr<ScriptedPromptInput::Counters> Counters = std::make_shared<ScriptedPromptInput::Counters>();

    LauncherHarness()
    {
        System.Environment["ProgramData"] = "C:/ProgramData";
        System.Environment["LOCALAPPDATA"] = "C:/Users/wiz/AppData/Local";
        System.Working = "C:/work";
        System.Executable = "C:/ambrose/bin";
    }

    void AddInstall(bool program = true, bool configuration = true)
    {
        System.AddInstall(LauncherTestData::Install, LauncherTestData::Revision, program);
        System.AddFile(std::string(LauncherTestData::Install) + "/Bin/data.dat", std::string(LauncherTestData::Revision) + "\n");
        if (!configuration)
            return;
        System.AddFile(std::string(LauncherTestData::Install) + "/Bin/config.xml", LauncherTestData::ConfigTemplate);
        System.AddFile(std::string(LauncherTestData::Install) + "/Bin/preferences.xml", LauncherTestData::PreferencesTemplate);
    }

    std::optional<LauncherPlan> Prepare(LauncherRequest const& request, SetupMode mode = SetupMode::Auto)
    {
        SetupPrompt prompt(std::make_unique<ScriptedPromptInput>(std::vector<std::string>{}, true, Counters), Out, false, std::chrono::seconds(1));
        Launcher const launcher(System, Files, Err);
        Error.clear();
        return launcher.Prepare(request, mode, prompt, Error);
    }

    bool Write(LauncherPlan const& plan)
    {
        Launcher const launcher(System, Files, Err);
        Error.clear();
        return launcher.WriteRunFolder(plan, Error);
    }

    static std::string Argument(LauncherPlan const& plan, std::string const& flag, std::size_t after = 1)
    {
        for (std::size_t index = 0; index + after < plan.Arguments.size(); ++index)
            if (plan.Arguments[index] == flag)
                return plan.Arguments[index + after];
        return std::string();
    }
};

#endif
