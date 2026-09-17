/*
 * Project Ambrose by Imjustchico
 * Runs guided setup: checks whether the configured install and type dump are usable, leaves keys set by the environment or command line alone, searches the machine once, offers each find with where it was found and a warning when its revision is not the pinned one, accepts a typed path only when it holds an install or a type dump, and saves the choices by merging them into conf.d/client-data.conf through a temporary file before reloading the configuration. Tools honour AMBROSE_SETUP_DISCOVER, AMBROSE_SETUP_PROMPT and AMBROSE_SETUP_PROMPT_TIMEOUT.
 */

#include "ClientSetup.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <cerrno>
#include <fstream>
#include <iterator>
#include <map>
#include <ostream>
#include <system_error>

namespace
{
    constexpr std::size_t MaxSavedFileBytes = 64 * 1024;

    std::string DescribeInstall(ClientCandidate const& candidate)
    {
        std::string text = fmt::format("{}, found through {}", candidate.Install.Describe(), candidate.Source);
        if (!candidate.Install.IsPinned())
            text += fmt::format("; not the pinned {}, so its messages and types may not match", ClientInstall::PinnedRevision);
        return text;
    }

    std::string DescribeDump(TypeDumpCandidate const& candidate)
    {
        return fmt::format("{}, found {}", ClientLocator::PathText(candidate.Path), candidate.Source);
    }

    std::string JoinFound(std::vector<std::string> const& found)
    {
        std::string text;
        for (std::string const& item : found)
            text += (text.empty() ? "" : "; ") + item;
        return text;
    }

    std::optional<std::filesystem::path> AskInstall(SetupPrompt& prompt, ClientSystem const& system, std::vector<ClientCandidate> const& installs, std::string const& question)
    {
        std::vector<std::string> options;
        for (ClientCandidate const& candidate : installs)
            options.push_back(DescribeInstall(candidate));
        std::string const heading = installs.empty() ? question + " No install was found on this machine." : question + " Found on this machine:";
        for (int attempt = 0; attempt < SetupPrompt::MaxAttempts && prompt.IsInteractive(); ++attempt)
        {
            SetupPrompt::Choice const choice = prompt.Choose(heading, options);
            if (choice.Kind == SetupPrompt::Answer::Skipped)
                return std::nullopt;
            if (choice.Kind == SetupPrompt::Answer::Picked && choice.Index < installs.size())
                return installs[choice.Index].Install.Root;
            std::filesystem::path const typed = ConfigMgr::PathFromUtf8(choice.Path);
            if (std::optional<ClientInstall> const install = ClientInstall::Inspect(system, typed))
            {
                if (!install->IsPinned())
                    prompt.Say(fmt::format("{} is not the pinned {}, so its messages and types may not match.", install->Describe(), ClientInstall::PinnedRevision));
                return install->Root;
            }
            prompt.Say(fmt::format("{} holds no Wizard101 install: there is no Data/GameData/Root.wad in it.", Ambrose::ForLog(choice.Path, 512)));
        }
        return std::nullopt;
    }

    std::optional<std::filesystem::path> AskTypeDump(SetupPrompt& prompt, ClientSystem const& system, std::vector<TypeDumpCandidate> const& dumps, std::string const& question)
    {
        std::vector<std::string> options;
        for (TypeDumpCandidate const& candidate : dumps)
            options.push_back(DescribeDump(candidate));
        std::string const heading = dumps.empty() ? question + " No type dump was found on this machine." : question + " Found on this machine:";
        for (int attempt = 0; attempt < SetupPrompt::MaxAttempts && prompt.IsInteractive(); ++attempt)
        {
            SetupPrompt::Choice const choice = prompt.Choose(heading, options);
            if (choice.Kind == SetupPrompt::Answer::Skipped)
                return std::nullopt;
            if (choice.Kind == SetupPrompt::Answer::Picked && choice.Index < dumps.size())
                return dumps[choice.Index].Path;
            std::filesystem::path const typed = ConfigMgr::PathFromUtf8(choice.Path);
            if (ClientLocator::LooksLikeTypeDump(system, typed))
                return typed;
            prompt.Say(fmt::format("{} is not a type dump: it does not start as a JSON object naming version and classes.", Ambrose::ForLog(choice.Path, 512)));
        }
        return std::nullopt;
    }

    std::string Quote(std::string_view value)
    {
        std::string quoted = "\"";
        for (char const c : value)
        {
            if (c == '\\' || c == '"')
                quoted.push_back('\\');
            quoted.push_back(c);
        }
        quoted.push_back('"');
        return quoted;
    }

    std::vector<ClientCandidate> WithConfigured(std::vector<ClientCandidate> installs, std::optional<ClientInstall> const& configured)
    {
        if (configured)
            installs.insert(installs.begin(), ClientCandidate{ *configured, "ClientDir" });
        return installs;
    }

    bool Locked(ConfigMgr const& config, std::string_view key)
    {
        std::optional<ConfigEntry> const entry = config.Resolve(std::string(key));
        return entry && (entry->Kind == ConfigSourceKind::Environment || entry->Kind == ConfigSourceKind::Override);
    }
}

ClientSetupResult ClientSetup::ForServer(ConfigMgr& config, SetupPrompt& prompt, ClientSystem const& system, ClientSetupRequest const& request, Report const& report)
{
    ClientSetupResult result;
    if (!config.GetOption<bool>("Setup.Discover", true, true))
        return result;
    bool searched = false;
    auto const search = [&result, &searched, &system]
    {
        if (!searched)
            result.Installs = ClientLocator::FindInstalls(system);
        searched = true;
    };
    std::vector<std::pair<std::string, std::string>> chosen;

    std::string const clientDir = config.GetOption<std::string>(std::string(ClientDirKey), "", true);
    std::optional<ClientInstall> install = clientDir.empty() ? std::nullopt : ClientInstall::Inspect(system, ConfigMgr::PathFromUtf8(clientDir));
    if (request.NeedClient && !install && !Locked(config, ClientDirKey))
    {
        search();
        std::string const reason = clientDir.empty() ? std::string("ClientDir is not set") : fmt::format("ClientDir {} holds no Wizard101 install", Ambrose::ForLog(clientDir, 512));
        if (prompt.IsInteractive())
        {
            if (std::optional<std::filesystem::path> const root = AskInstall(prompt, system, result.Installs, fmt::format("{} needs your own Wizard101 install, and {}.", request.AppName, reason)))
            {
                chosen.emplace_back(ClientDirKey, ConfigPath(*root));
                install = ClientInstall::Inspect(system, *root);
            }
        }
        else if (!result.Installs.empty())
        {
            std::vector<std::string> found;
            for (ClientCandidate const& candidate : result.Installs)
                found.push_back(DescribeInstall(candidate));
            report(true, fmt::format("{}, and Wizard101 was found on this machine: {}. Start {} in a terminal to choose one, or set ClientDir in conf.d/{}", reason, JoinFound(found), request.AppName, SavedFileName));
        }
    }

    std::string const typeDump = config.GetOption<std::string>(std::string(TypeDumpKey), "", true);
    bool const dumpUsable = !typeDump.empty() && ClientLocator::LooksLikeTypeDump(system, ConfigMgr::PathFromUtf8(typeDump));
    if (request.NeedTypeDump && !dumpUsable && !Locked(config, TypeDumpKey))
    {
        search();
        std::vector<TypeDumpCandidate> const dumps = ClientLocator::FindTypeDumps(system, WithConfigured(result.Installs, install));
        std::string const reason = typeDump.empty() ? std::string("TypeDumpPath is not set") : fmt::format("TypeDumpPath {} is not a type dump", Ambrose::ForLog(typeDump, 512));
        if (prompt.IsInteractive())
        {
            if (std::optional<std::filesystem::path> const path = AskTypeDump(prompt, system, dumps, fmt::format("{} needs the type dump made from your install, and {}.", request.AppName, reason)))
                chosen.emplace_back(TypeDumpKey, ConfigPath(*path));
        }
        else if (!dumps.empty())
        {
            std::vector<std::string> found;
            for (TypeDumpCandidate const& candidate : dumps)
                found.push_back(DescribeDump(candidate));
            report(true, fmt::format("{}, and a type dump was found on this machine: {}. Start {} in a terminal to choose one, or set TypeDumpPath in conf.d/{}", reason, JoinFound(found), request.AppName, SavedFileName));
        }
    }

    if (chosen.empty())
        return result;
    std::string error;
    if (!Save(config.GetFilename(), chosen, result.SavedTo, error))
    {
        report(true, fmt::format("The chosen client data could not be saved, so it applies to this run only after you set it yourself: {}", error));
        return result;
    }
    ConfigLoadResult const reloaded = config.Reload();
    if (!reloaded.Succeeded())
    {
        for (ConfigIssue const& issue : reloaded.Errors)
            report(true, fmt::format("Reloading the configuration after saving {} failed: {}", ClientLocator::PathText(result.SavedTo), issue.ToString()));
        return result;
    }
    result.Saved = chosen;
    for (auto const& [key, value] : chosen)
        report(false, fmt::format("Saved {} = {} to {}", key, value, ClientLocator::PathText(result.SavedTo)));
    return result;
}

void ClientSetup::ForTool(std::optional<std::string>& client, std::optional<std::string>* typeDump, SetupPrompt& prompt, ClientSystem const& system, std::string_view toolName, std::ostream& err)
{
    if (system.GetEnv("AMBROSE_SETUP_DISCOVER") == std::optional<std::string>("0"))
        return;
    std::vector<ClientCandidate> installs;
    bool searched = false;
    if (!client)
    {
        installs = ClientLocator::FindInstalls(system);
        searched = true;
        if (prompt.IsInteractive())
        {
            if (std::optional<std::filesystem::path> const root = AskInstall(prompt, system, installs, fmt::format("{} needs your own Wizard101 install, and none was named.", toolName)))
                client = ConfigPath(*root);
        }
        else if (!installs.empty())
        {
            std::vector<std::string> found;
            for (ClientCandidate const& candidate : installs)
                found.push_back(DescribeInstall(candidate));
            err << fmt::format("{}: Wizard101 was found on this machine: {}. Pass --client with one of them, or run {} in a terminal to choose\n", toolName, JoinFound(found), toolName);
        }
    }
    if (!typeDump || *typeDump)
        return;
    if (!searched)
        installs = ClientLocator::FindInstalls(system);
    std::optional<ClientInstall> const configured = client ? ClientInstall::Inspect(system, ConfigMgr::PathFromUtf8(*client)) : std::nullopt;
    std::vector<TypeDumpCandidate> const dumps = ClientLocator::FindTypeDumps(system, WithConfigured(installs, configured));
    if (prompt.IsInteractive())
    {
        if (std::optional<std::filesystem::path> const path = AskTypeDump(prompt, system, dumps, fmt::format("{} needs the type dump made from your install, and none was named.", toolName)))
            *typeDump = ConfigPath(*path);
    }
    else if (!dumps.empty())
    {
        std::vector<std::string> found;
        for (TypeDumpCandidate const& candidate : dumps)
            found.push_back(DescribeDump(candidate));
        err << fmt::format("{}: a type dump was found on this machine: {}. Pass --type-dump with it, or run {} in a terminal to choose\n", toolName, JoinFound(found), toolName);
    }
}

std::unique_ptr<SetupPrompt> ClientSetup::ToolPrompt(std::ostream& out)
{
    std::optional<std::string> const enabled = Ambrose::GetEnv("AMBROSE_SETUP_PROMPT");
    std::optional<std::string> const timeoutText = Ambrose::GetEnv("AMBROSE_SETUP_PROMPT_TIMEOUT");
    std::optional<unsigned> const timeout = timeoutText ? Ambrose::StringTo<unsigned>(Ambrose::Trim(*timeoutText)) : std::nullopt;
    return SetupPrompt::ForProcess(out, !enabled || Ambrose::Trim(*enabled) != "0", std::chrono::seconds(timeout.value_or(DefaultTimeoutSeconds)));
}

bool ClientSetup::Save(std::filesystem::path const& configFile, std::vector<std::pair<std::string, std::string>> const& values, std::filesystem::path& savedTo, std::string& error)
{
    std::filesystem::path const folder = configFile.parent_path() / "conf.d";
    std::error_code created;
    std::filesystem::create_directories(folder, created);
    if (created)
    {
        error = fmt::format("cannot create {}: {}", ClientLocator::PathText(folder), created.message());
        return false;
    }
    std::filesystem::path const file = folder / ConfigMgr::PathFromUtf8(SavedFileName);
    std::map<std::string, std::string> entries;
    {
        std::ifstream existing(file, std::ios::binary);
        if (existing)
        {
            std::string text(MaxSavedFileBytes, '\0');
            existing.read(text.data(), static_cast<std::streamsize>(text.size()));
            text.resize(static_cast<std::size_t>(existing.gcount()));
            ParsedConfig const parsed = ConfigMgr::ParseText(text, file, ConfigSourceKind::ModuleConfig);
            for (auto const& [key, entry] : parsed.Entries)
                entries[key] = entry.Value;
        }
    }
    for (auto const& [key, value] : values)
        entries[key] = value;

    std::string text = "# Project Ambrose by Imjustchico\n# The client data paths guided setup found on this machine and you chose; edit or delete this file to choose again.\n";
    for (auto const& [key, value] : entries)
        text += fmt::format("{} = {}\n", key, Quote(value));
    std::filesystem::path temporary = file;
    temporary += ".partial";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        stream.close();
        if (!stream)
        {
            error = fmt::format("cannot write {}: {}", ClientLocator::PathText(temporary), std::error_code(errno, std::generic_category()).message());
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
    }
    std::error_code renamed;
    std::filesystem::rename(temporary, file, renamed);
    if (renamed)
    {
        error = fmt::format("cannot replace {}: {}", ClientLocator::PathText(file), renamed.message());
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    savedTo = file;
    return true;
}

std::string ClientSetup::ConfigPath(std::filesystem::path const& path)
{
    std::u8string const generic = path.lexically_normal().generic_u8string();
    return std::string(generic.begin(), generic.end());
}
