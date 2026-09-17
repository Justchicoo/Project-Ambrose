/*
 * Project Ambrose by Imjustchico
 * Builds and writes the client run folder: it reads the install's own config.xml, or defaultconfig.xml from its Root.wad, and its preferences.xml, sets only the VideoSettings keys the window comes from and empties SilentMetricsURL, adding a table or key the template lacks and leaving everything else, VersionInfo included, as the install has it; it copies revision.dat and data.dat, compares the stamp already in the folder with the one the options ask for, and on a rebuild removes the stamp first, writes every file and writes the stamp last, so an interrupted rebuild is done again.
 */

#include "ClientRunFolder.h"

#include "ConfigMgr.h"

#include <fmt/format.h>

#include <pugixml.hpp>

#include <sstream>
#include <utility>

namespace
{
    constexpr std::string_view ConfigRoot = "config";
    constexpr std::string_view PreferencesRoot = "preferences";

    pugi::xml_node EnsureRecord(pugi::xml_node root, char const* table)
    {
        pugi::xml_node node = root.child(table);
        if (!node)
        {
            node = root.append_child(table);
            pugi::xml_node list = root.child("_TableList");
            if (!list)
                list = root.prepend_child("_TableList");
            bool listed = false;
            for (pugi::xml_node const record : list.children("RECORD"))
                if (std::string_view(record.child("Name").child_value()) == std::string_view(table))
                    listed = true;
            if (!listed)
            {
                pugi::xml_node name = list.append_child("RECORD").append_child("Name");
                name.append_attribute("TYPE") = "STR";
                name.text().set(table);
            }
        }
        pugi::xml_node record = node.child("RECORD");
        if (!record)
            record = node.append_child("RECORD");
        return record;
    }

    void SetValue(pugi::xml_node record, char const* key, char const* type, std::string const& value)
    {
        pugi::xml_node node = record.child(key);
        if (!node)
        {
            node = record.append_child(key);
            node.append_attribute("TYPE") = type;
        }
        node.text().set(value.c_str());
    }
}

std::optional<std::string> ClientRunFolder::Generate(std::string const& templateText, std::string_view root, RunFolderOptions const& options, std::string& error)
{
    pugi::xml_document document;
    pugi::xml_parse_result const parsed = document.load_buffer(templateText.data(), templateText.size(), pugi::parse_default | pugi::parse_declaration);
    if (!parsed)
    {
        error = fmt::format("{} at offset {}", parsed.description(), parsed.offset);
        return std::nullopt;
    }
    pugi::xml_node const element = document.child(std::string(root).c_str());
    if (!element)
    {
        error = fmt::format("its root element is not <{}>", root);
        return std::nullopt;
    }
    pugi::xml_node const video = EnsureRecord(element, "VideoSettings");
    SetValue(video, "IsFullscreen", "INT", std::to_string(options.Fullscreen));
    SetValue(video, "Resolution", "STR", fmt::format("{}x{}", options.Width, options.Height));
    if (options.WindowX)
        SetValue(video, "WindowedX", "INT", std::to_string(*options.WindowX));
    if (options.WindowY)
        SetValue(video, "WindowedY", "INT", std::to_string(*options.WindowY));
    if (root == ConfigRoot)
        SetValue(EnsureRecord(element, "GameSettings"), "SilentMetricsURL", "STR", std::string());

    std::ostringstream text;
    document.save(text, "  ", pugi::format_default | pugi::format_no_empty_element_tags);
    return text.str();
}

std::string ClientRunFolder::EmptyPreferences()
{
    return "<?xml version=\"1.0\" ?>\n<preferences>\n</preferences>\n";
}

std::string ClientRunFolder::Stamp(RunFolderOptions const& options)
{
    return fmt::format("# Project Ambrose by Imjustchico\n# What the launcher wrote this run folder for; delete this file to have it written again.\n"
        "Stamp = 1\nRevision = {}\nInstall = {}\nFullscreen = {}\nResolution = {}x{}\nWindowX = {}\nWindowY = {}\n",
        options.Revision, ConfigMgr::PathToUtf8(options.Install), options.Fullscreen, options.Width, options.Height,
        options.WindowX ? std::to_string(*options.WindowX) : std::string("-"), options.WindowY ? std::to_string(*options.WindowY) : std::string("-"));
}

std::optional<RunFolderPlan> ClientRunFolder::Build(RunFolderOptions const& options, ClientSystem const& system, LauncherFiles const& files, std::string& error)
{
    RunFolderPlan plan;
    plan.Folder = options.Folder;
    plan.Stamp = Stamp(options);
    std::filesystem::path const bin = options.Install / "Bin";

    std::optional<std::string> configTemplate = system.ReadText(bin / std::string(ConfigName), MaxTemplateBytes);
    plan.ConfigSource = fmt::format("Bin/{}", ConfigName);
    if (!configTemplate)
    {
        std::string archiveError;
        configTemplate = files.ReadArchiveEntry(options.Install / "Data" / "GameData" / std::string(ArchiveName), DefaultConfigName, MaxTemplateBytes, archiveError);
        if (!configTemplate)
        {
            error = fmt::format("the install has no Bin/{}, and {} could not be read from its {}: {}", ConfigName, DefaultConfigName, ArchiveName, archiveError);
            return std::nullopt;
        }
        plan.ConfigSource = fmt::format("{} in {}", DefaultConfigName, ArchiveName);
    }
    std::string reason;
    std::optional<std::string> const config = Generate(*configTemplate, ConfigRoot, options, reason);
    if (!config)
    {
        error = fmt::format("the client configuration cannot be built from {}: {}", plan.ConfigSource, reason);
        return std::nullopt;
    }
    plan.Files.push_back({ std::string(ConfigName), *config });

    std::optional<std::string> preferencesTemplate = system.ReadText(bin / std::string(PreferencesName), MaxTemplateBytes);
    if (!preferencesTemplate)
        preferencesTemplate = EmptyPreferences();
    std::optional<std::string> const preferences = Generate(*preferencesTemplate, PreferencesRoot, options, reason);
    if (!preferences)
    {
        error = fmt::format("the client preferences cannot be built from Bin/{}: {}", PreferencesName, reason);
        return std::nullopt;
    }
    plan.Files.push_back({ std::string(PreferencesName), *preferences });

    std::optional<std::string> const revision = system.ReadText(bin / std::string(RevisionName), MaxSmallFileBytes);
    plan.Files.push_back({ std::string(RevisionName), revision.value_or(options.Revision + "\n") });
    if (std::optional<std::string> const data = system.ReadText(bin / std::string(DataName), MaxSmallFileBytes))
        plan.Files.push_back({ std::string(DataName), *data });

    std::optional<std::string> const written = system.ReadText(options.Folder / std::string(StampName), MaxSmallFileBytes);
    plan.Rebuild = !written || *written != plan.Stamp;
    return plan;
}

bool ClientRunFolder::Write(RunFolderPlan const& plan, LauncherFiles const& files, std::string& error)
{
    if (!files.CreateFolder(plan.Folder, error))
        return false;
    if (!plan.Rebuild)
        return true;
    std::filesystem::path const stamp = plan.Folder / std::string(StampName);
    if (!files.RemoveFile(stamp, error))
        return false;
    for (RunFolderFile const& file : plan.Files)
        if (!files.WriteFile(plan.Folder / file.Name, file.Text, error))
            return false;
    return files.WriteFile(stamp, plan.Stamp, error);
}
