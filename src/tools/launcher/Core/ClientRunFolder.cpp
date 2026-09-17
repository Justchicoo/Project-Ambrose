/*
 * Project Ambrose by Imjustchico
 * Builds and writes the client run folder: it reads the configuration the folder already holds, or else the install's own config.xml, or defaultconfig.xml from its Root.wad whose root element is renamed to config, and the same for preferences.xml, which falls back to an empty one, so a file the client left unreadable costs the next template and not the run; pugixml reads the template only to refuse one that is not a client configuration, and the values are then spliced into the template's own bytes, because the r806919 client ignores a file that has been parsed and written again and falls back to its built-in defaults: it sets only the VideoSettings keys the window comes from and empties SilentMetricsURL wherever either file holds it, adding a table or key the template lacks and listing it only when the template lists its tables, and leaving every other byte, VersionInfo and the declaration included, exactly as it is; config.xml and preferences.xml are written every run, because the client saves its own over them as it exits, while the stamp says which install's revision.dat and data.dat the folder holds, so those are copied again only when the install or its revision changes, the stamp is removed first and written last, and an interrupted copy is done again.
 */

#include "ClientRunFolder.h"

#include "ClientConfigText.h"
#include "ConfigMgr.h"

#include <fmt/format.h>

#include <pugixml.hpp>

#include <utility>

namespace
{
    constexpr std::string_view ConfigRoot = "config";
    constexpr std::string_view DefaultConfigRoot = "defaultconfig";
    constexpr std::string_view PreferencesRoot = "preferences";
    constexpr std::string_view VideoTable = "VideoSettings";
    constexpr std::string_view GameTable = "GameSettings";
    constexpr std::string_view MetricsKey = "SilentMetricsURL";
}

std::optional<std::string> ClientRunFolder::Generate(std::string const& templateText, std::string_view root, RunFolderOptions const& options, std::string& error)
{
    pugi::xml_document document;
    pugi::xml_parse_result const parsed = document.load_buffer(templateText.data(), templateText.size(), pugi::parse_default);
    if (!parsed)
    {
        error = fmt::format("{} at offset {}", parsed.description(), parsed.offset);
        return std::nullopt;
    }
    std::string_view const held = document.document_element() ? std::string_view(document.document_element().name()) : std::string_view();
    bool const renaming = root == ConfigRoot && held == DefaultConfigRoot;
    if (held != root && !renaming)
    {
        error = root == ConfigRoot ? fmt::format("its root element is not <{}> or <{}>", root, DefaultConfigRoot) : fmt::format("its root element is not <{}>", root);
        return std::nullopt;
    }

    ClientConfigText text(templateText);
    if (renaming)
        text.RenameRoot(root);
    text.SetKey(VideoTable, "IsFullscreen", "INT", std::to_string(options.Fullscreen));
    text.SetKey(VideoTable, "Resolution", "STR", fmt::format("{}x{}", options.Width, options.Height));
    if (options.WindowX)
        text.SetKey(VideoTable, "WindowedX", "INT", std::to_string(*options.WindowX));
    if (options.WindowY)
        text.SetKey(VideoTable, "WindowedY", "INT", std::to_string(*options.WindowY));
    text.EmptyKey(MetricsKey);
    if (root == ConfigRoot)
        text.SetKey(GameTable, MetricsKey, "STR", std::string_view());
    return text.Text();
}

std::string ClientRunFolder::EmptyPreferences()
{
    return "<?xml version=\"1.0\" ?>\n<preferences>\n</preferences>\n";
}

std::string ClientRunFolder::Stamp(RunFolderOptions const& options)
{
    return fmt::format("# Project Ambrose by Imjustchico\n# The install whose revision.dat and data.dat this run folder holds; delete this file to have them copied again.\n"
        "Stamp = 2\nRevision = {}\nInstall = {}\n", options.Revision, ConfigMgr::PathToUtf8(options.Install));
}

std::optional<RunFolderPlan> ClientRunFolder::Build(RunFolderOptions const& options, ClientSystem const& system, LauncherFiles const& files, std::string& error)
{
    RunFolderPlan plan;
    plan.Folder = options.Folder;
    plan.Stamp = Stamp(options);
    std::optional<std::string> const written = system.ReadText(options.Folder / std::string(StampName), MaxSmallFileBytes);
    plan.Rebuild = !written || *written != plan.Stamp;
    std::filesystem::path const bin = options.Install / "Bin";

    std::string reason;
    std::optional<std::string> config;
    if (!plan.Rebuild)
        if (std::optional<std::string> const held = system.ReadText(options.Folder / std::string(ConfigName), MaxTemplateBytes))
        {
            plan.ConfigSource = fmt::format("the {} the folder already holds", ConfigName);
            config = Generate(*held, ConfigRoot, options, reason);
        }
    if (!config)
    {
        std::optional<std::string> own = system.ReadText(bin / std::string(ConfigName), MaxTemplateBytes);
        plan.ConfigSource = fmt::format("Bin/{}", ConfigName);
        if (!own)
        {
            std::string archiveError;
            own = files.ReadArchiveEntry(options.Install / "Data" / "GameData" / std::string(ArchiveName), DefaultConfigName, MaxTemplateBytes, archiveError);
            if (!own)
            {
                error = fmt::format("the install has no Bin/{}, and {} could not be read from its {}: {}", ConfigName, DefaultConfigName, ArchiveName, archiveError);
                return std::nullopt;
            }
            plan.ConfigSource = fmt::format("{} in {}", DefaultConfigName, ArchiveName);
        }
        config = Generate(*own, ConfigRoot, options, reason);
        if (!config)
        {
            error = fmt::format("the client configuration cannot be built from {}: {}", plan.ConfigSource, reason);
            return std::nullopt;
        }
    }
    plan.Files.push_back({ std::string(ConfigName), *config });

    std::optional<std::string> preferences;
    if (!plan.Rebuild)
        if (std::optional<std::string> const held = system.ReadText(options.Folder / std::string(PreferencesName), MaxTemplateBytes))
        {
            plan.PreferencesSource = fmt::format("the {} the folder already holds", PreferencesName);
            preferences = Generate(*held, PreferencesRoot, options, reason);
        }
    if (!preferences)
        if (std::optional<std::string> const own = system.ReadText(bin / std::string(PreferencesName), MaxTemplateBytes))
        {
            plan.PreferencesSource = fmt::format("Bin/{}", PreferencesName);
            preferences = Generate(*own, PreferencesRoot, options, reason);
        }
    if (!preferences)
    {
        plan.PreferencesSource = "an empty one";
        preferences = Generate(EmptyPreferences(), PreferencesRoot, options, reason);
    }
    if (!preferences)
    {
        error = fmt::format("the client preferences cannot be built from {}: {}", plan.PreferencesSource, reason);
        return std::nullopt;
    }
    plan.Files.push_back({ std::string(PreferencesName), *preferences });

    std::optional<std::string> const revision = system.ReadText(bin / std::string(RevisionName), MaxSmallFileBytes);
    plan.Copies.push_back({ std::string(RevisionName), revision.value_or(options.Revision + "\n") });
    if (std::optional<std::string> const data = system.ReadText(bin / std::string(DataName), MaxSmallFileBytes))
        plan.Copies.push_back({ std::string(DataName), *data });
    return plan;
}

bool ClientRunFolder::Write(RunFolderPlan const& plan, LauncherFiles const& files, std::string& error)
{
    if (!files.CreateFolder(plan.Folder, error))
        return false;
    std::filesystem::path const stamp = plan.Folder / std::string(StampName);
    if (plan.Rebuild && !files.RemoveFile(stamp, error))
        return false;
    for (RunFolderFile const& file : plan.Files)
        if (!files.WriteFile(plan.Folder / file.Name, file.Text, error))
            return false;
    if (!plan.Rebuild)
        return true;
    for (RunFolderFile const& file : plan.Copies)
        if (!files.WriteFile(plan.Folder / file.Name, file.Text, error))
            return false;
    return files.WriteFile(stamp, plan.Stamp, error);
}
