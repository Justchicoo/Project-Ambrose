/*
 * Project Ambrose by Imjustchico
 * The folder an Ambrose-started client runs from, built beside the install and never inside it: config.xml and preferences.xml written every run, from the files the folder already holds so the client's own saved settings survive, or from the install's own files, or from defaultconfig.xml in its Root.wad when it has no config.xml, with the window mode, size and position asked for and SilentMetricsURL emptied so the client reaches nothing outside the machine, and copies of revision.dat and data.dat, the other files the client opens by relative name. A stamp records the install and revision those copies were made from, so they are made again when either changes and the two generated files are seeded from the install again with them.
 */

#ifndef AMBROSE_CLIENTRUNFOLDER_H
#define AMBROSE_CLIENTRUNFOLDER_H

#include "ClientSystem.h"
#include "LauncherFiles.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct RunFolderOptions
{
    std::filesystem::path Folder;
    std::filesystem::path Install;
    std::string Revision;
    int Fullscreen = 0;
    unsigned Width = 1280;
    unsigned Height = 720;
    std::optional<int> WindowX;
    std::optional<int> WindowY;
};

struct RunFolderFile
{
    std::string Name;
    std::string Text;
};

struct RunFolderPlan
{
    std::filesystem::path Folder;
    std::vector<RunFolderFile> Files;
    std::vector<RunFolderFile> Copies;
    std::string Stamp;
    std::string ConfigSource;
    std::string PreferencesSource;
    bool Rebuild = true;
};

class ClientRunFolder
{
public:
    static constexpr std::string_view ConfigName = "config.xml";
    static constexpr std::string_view PreferencesName = "preferences.xml";
    static constexpr std::string_view RevisionName = "revision.dat";
    static constexpr std::string_view DataName = "data.dat";
    static constexpr std::string_view StampName = "launcher.stamp";
    static constexpr std::string_view LogName = "WizardClient.log";
    static constexpr std::string_view ArchiveName = "Root.wad";
    static constexpr std::string_view DefaultConfigName = "defaultconfig.xml";
    static constexpr std::size_t MaxTemplateBytes = 4 * 1024 * 1024;
    static constexpr std::size_t MaxSmallFileBytes = 4096;
    static constexpr int MaxFullscreen = 2;
    static constexpr unsigned MinWindowSize = 320;
    static constexpr unsigned MaxWindowSize = 16384;

    ClientRunFolder() = delete;

    static std::optional<RunFolderPlan> Build(RunFolderOptions const& options, ClientSystem const& system, LauncherFiles const& files, std::string& error);
    static bool Write(RunFolderPlan const& plan, LauncherFiles const& files, std::string& error);
    static std::string Stamp(RunFolderOptions const& options);
    static std::optional<std::string> Generate(std::string const& templateText, std::string_view root, RunFolderOptions const& options, std::string& error);
    static std::string EmptyPreferences();
};

#endif
