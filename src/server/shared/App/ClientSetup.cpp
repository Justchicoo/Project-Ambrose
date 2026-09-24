/*
 * Project Ambrose by Imjustchico
 * Runs setup in its mode: checks whether the configured install and type dump are usable, leaves keys set by the environment or command line alone even when set empty, and skips found installs and dumps whose paths are not valid Unicode, naming them; auto picks the newest install found, for this run only when ClientDir names a folder without an install, and builds or reuses the type dump through the provider, announcing a build as the extractor starts; ask uses a current built dump without asking, offers a build when no dump found is named for the install, and offers the finds, only when there is something to offer; off uses the configured values as they are. A server saves only ClientDir and a chosen dump, never saves an install when it needs a type dump and none is usable, and does not use an install setup picked for it either when its install needs the dump. Tools get the same flow for --client and --type-dump without saving: auto prints one line naming what it used, off and a run without a terminal print the finds and the flag to pass, and Ctrl+C or SIGTERM during a tool's build stops the extractor. Saving merges the values into conf.d/client-data.conf line by line, refusing a file it cannot read, parse or fit, writes paths as absolute quoted values with newlines and tabs escaped, checks the new text reads back before replacing the file through a unique temporary file, restores the old file when the reload fails, reports a saved value another file overrides instead of claiming it, and warns when a saved value overrides a different one in the app's own configuration file. Configuration path text never throws, replacing what is not valid Unicode; typed answers lose one pair of surrounding quotes and expand ~ from HOME, and advice names why no question could be asked.
 */

#include "ClientSetup.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "StringUtil.h"
#include "TypeDumpCache.h"
#include "Types.h"
#include "Utf.h"

#include <fmt/format.h>

#include <algorithm>
#include <csignal>
#include <fstream>
#include <iterator>
#include <map>
#include <ostream>
#include <random>
#include <set>
#include <string_view>
#include <system_error>

namespace
{
    constexpr std::size_t MaxAppConfigBytes = 1024 * 1024;

    volatile std::sig_atomic_t ToolStopSignal = 0;

    void OnToolStopSignal(int)
    {
        ToolStopSignal = 1;
    }

    constexpr std::string_view SavedHeader = "# Project Ambrose by Imjustchico\n# The client data paths setup chose on this machine; edit or delete this file to choose again.\n";

    std::string DescribeInstall(ClientCandidate const& candidate)
    {
        return fmt::format("{}, found through {}", candidate.Install.Describe(), candidate.Source);
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

    std::string ListInstalls(std::vector<ClientCandidate> const& installs)
    {
        std::vector<std::string> found;
        for (ClientCandidate const& candidate : installs)
            found.push_back(DescribeInstall(candidate));
        return JoinFound(found);
    }

    std::string ListDumps(std::vector<TypeDumpCandidate> const& dumps)
    {
        std::vector<std::string> found;
        for (TypeDumpCandidate const& candidate : dumps)
            found.push_back(DescribeDump(candidate));
        return JoinFound(found);
    }

    std::optional<std::string> UnicodeText(std::filesystem::path const& path)
    {
#ifdef _WIN32
        std::wstring const generic = path.lexically_normal().generic_wstring();
        return Utf::Utf16ToUtf8(std::u16string_view(reinterpret_cast<char16_t const*>(generic.data()), generic.size()), Utf::InvalidPolicy::Reject);
#else
        std::u8string const generic = path.lexically_normal().generic_u8string();
        return std::string(generic.begin(), generic.end());
#endif
    }

    std::vector<ClientCandidate> UnicodeInstalls(std::vector<ClientCandidate> found, std::function<void(std::string const&)> const& skip)
    {
        std::vector<ClientCandidate> kept;
        for (ClientCandidate& candidate : found)
        {
            if (UnicodeText(candidate.Install.Root))
                kept.push_back(std::move(candidate));
            else
                skip(fmt::format("Setup skips the Wizard101 install {}: its path is not valid Unicode, so it cannot be saved or passed on; rename its folder to use it", DescribeInstall(candidate)));
        }
        return kept;
    }

    std::vector<TypeDumpCandidate> UnicodeDumps(std::vector<TypeDumpCandidate> found, std::function<void(std::string const&)> const& skip)
    {
        std::vector<TypeDumpCandidate> kept;
        for (TypeDumpCandidate& candidate : found)
        {
            if (UnicodeText(candidate.Path))
                kept.push_back(std::move(candidate));
            else
                skip(fmt::format("Setup skips the type dump {}: its path is not valid Unicode, so it cannot be saved or passed on; rename the file to use it", DescribeDump(candidate)));
        }
        return kept;
    }

    bool SamePathText(ClientSystem const& system, std::string_view left, std::string_view right)
    {
        return system.IsWindows() ? Ambrose::EqualsIgnoreCase(left, right) : left == right;
    }

    bool HasDumpNamedFor(ClientSystem const& system, std::vector<TypeDumpCandidate> const& dumps, ClientInstall const& install)
    {
        if (install.Revision.empty())
            return false;
        std::string const name = install.Revision + ".json";
        std::filesystem::path const data = ClientLocator::GetDataFolder(system);
        std::string const cache = data.empty() ? std::string() : ClientLocator::PathText((data / ConfigMgr::PathFromUtf8(TypeDumpCache::FolderName) / ConfigMgr::PathFromUtf8(name)).lexically_normal());
        return std::any_of(dumps.begin(), dumps.end(), [&](TypeDumpCandidate const& dump)
        {
            return SamePathText(system, ClientLocator::PathText(dump.Path.filename()), name) && (cache.empty() || !SamePathText(system, ClientLocator::PathText(dump.Path.lexically_normal()), cache));
        });
    }

    std::string NamedForQuestion(std::string const& question, ClientInstall const& install)
    {
        return fmt::format("{} No type dump found is known to fit {}, so choose one only if it was made from that install.", question, install.Describe());
    }

    std::optional<std::filesystem::path> CurrentTypeDump(ClientInstall const& install, std::filesystem::path const& dataFolder, std::string& error)
    {
        std::optional<std::filesystem::path> const dump = TypeDumpCache::PathFor(dataFolder, install.Revision);
        if (!dump)
        {
            error = fmt::format("no built type dump can be kept for {}", install.Describe());
            return std::nullopt;
        }
        std::optional<TypeDumpHeader> const header = TypeDumpCache::ReadHeader(*dump);
        if (!header || header->Revision != install.Revision)
        {
            error = fmt::format("{} is not a type dump built for {}", ClientLocator::PathText(*dump), install.Describe());
            return std::nullopt;
        }
        std::optional<std::string> const executableSha256 = TypeDumpCache::ExecutableSha256(install, error);
        if (!executableSha256)
            return std::nullopt;
        if (!TypeDumpCache::IsCurrent(install, *dump, *executableSha256))
        {
            error = fmt::format("{} was built from another client program than the one in {}", ClientLocator::PathText(*dump), install.Describe());
            return std::nullopt;
        }
        return dump;
    }

    std::optional<ClientInstall> AskInstall(SetupPrompt& prompt, ClientSystem const& system, std::vector<ClientCandidate> const& installs, std::string const& question)
    {
        std::vector<std::string> options;
        for (ClientCandidate const& candidate : installs)
            options.push_back(DescribeInstall(candidate));
        std::string const heading = question + " Found on this machine:";
        for (int attempt = 0; attempt < SetupPrompt::MaxAttempts && prompt.IsInteractive(); ++attempt)
        {
            SetupPrompt::Choice const choice = prompt.Choose(heading, options);
            if (choice.Kind == SetupPrompt::Answer::Skipped)
                return std::nullopt;
            if (choice.Kind == SetupPrompt::Answer::Picked && choice.Index < installs.size())
                return installs[choice.Index].Install;
            if (std::optional<ClientInstall> install = ClientInstall::Inspect(system, ClientSetup::TypedPath(system, choice.Path)))
                return install;
            prompt.Say(fmt::format("{} holds no Wizard101 install: there is no Data/GameData/Root.wad in it.", Ambrose::ForLog(choice.Path, 512)));
        }
        return std::nullopt;
    }

    std::optional<ClientInstall> AskToInstallAndRetry(SetupPrompt& prompt, ClientSystem const& system, std::string_view app,
        std::vector<ClientCandidate>& found, std::function<void()> const& search)
    {
        prompt.Say(fmt::format("{} needs your own Wizard101 install, and none was found on this machine.", app));
        prompt.Say("Install Wizard101 from KingsIsle, or copy an install onto this machine. Ambrose never downloads it for you.");

        std::vector<std::string> const options{ "Look again, now that Wizard101 is installed", "Start without client data for now" };
        while (prompt.IsInteractive())
        {
            SetupPrompt::Choice const choice = prompt.Choose("When it is installed, choose the first option, or type the folder that holds Data and Bin.", options);
            if (choice.Kind == SetupPrompt::Answer::Skipped || (choice.Kind == SetupPrompt::Answer::Picked && choice.Index == 1))
                return std::nullopt;

            if (choice.Kind == SetupPrompt::Answer::Path)
            {
                if (std::optional<ClientInstall> install = ClientInstall::Inspect(system, ClientSetup::TypedPath(system, choice.Path)))
                    return install;
                prompt.Say(fmt::format("{} holds no Wizard101 install: there is no Data/GameData/Root.wad in it.", Ambrose::ForLog(choice.Path, 512)));
                continue;
            }

            found.clear();
            search();
            if (!found.empty())
            {
                prompt.Say(fmt::format("Found {}.", ListInstalls(found)));
                return found.front().Install;
            }
            prompt.Say("Still no Wizard101 install on this machine. Install it and choose the first option again, or type its folder.");
        }
        return std::nullopt;
    }

    std::optional<std::filesystem::path> AskTypeDump(SetupPrompt& prompt, ClientSystem const& system, std::vector<TypeDumpCandidate> const& dumps, std::string const& question)
    {
        std::vector<std::string> options;
        for (TypeDumpCandidate const& candidate : dumps)
            options.push_back(DescribeDump(candidate));
        std::string const heading = question + " Found on this machine:";
        for (int attempt = 0; attempt < SetupPrompt::MaxAttempts && prompt.IsInteractive(); ++attempt)
        {
            SetupPrompt::Choice const choice = prompt.Choose(heading, options);
            if (choice.Kind == SetupPrompt::Answer::Skipped)
                return std::nullopt;
            if (choice.Kind == SetupPrompt::Answer::Picked && choice.Index < dumps.size())
                return dumps[choice.Index].Path;
            std::filesystem::path const typed = ClientSetup::TypedPath(system, choice.Path);
            if (ClientLocator::LooksLikeTypeDump(system, typed))
                return typed;
            prompt.Say(fmt::format("{} is not a type dump: it does not start as a JSON object naming version or classes.", Ambrose::ForLog(choice.Path, 512)));
        }
        return std::nullopt;
    }

    std::string ServerAdvice(SetupPrompt const& prompt, std::string_view app, std::string_view key)
    {
        std::string_view const automatic = key == ClientSetup::TypeDumpKey ? "use the type dump built from the install" : "use the newest";
        SetupPrompt::Status const status = prompt.GetStatus();
        if (status == SetupPrompt::Status::NotATerminal)
            return fmt::format("Start {} in a terminal whose input and output are not redirected to choose one, set Setup.Mode = auto to {}, or set {} in conf.d/{}", app, automatic, key, ClientSetup::SavedFileName);
        if (status == SetupPrompt::Status::InputClosed || status == SetupPrompt::Status::TimedOut || status == SetupPrompt::Status::Stopped)
            return fmt::format("The setup questions were skipped, so set {} in conf.d/{}, or set Setup.Mode = auto to {}", key, ClientSetup::SavedFileName, automatic);
        return fmt::format("Set {} in conf.d/{}, or set Setup.Mode = auto to {}", key, ClientSetup::SavedFileName, automatic);
    }

    std::string ToolAdvice(SetupMode mode, SetupPrompt const& prompt, std::string_view tool, std::string_view pass, std::string_view offer, std::string_view choose)
    {
        if (mode == SetupMode::Off)
            return fmt::format("{}, or set {}=auto to {}", pass, ClientSetup::ModeVariable, offer);
        if (prompt.GetStatus() == SetupPrompt::Status::NotATerminal)
            return fmt::format("{}, or run {} in a terminal whose input and output are not redirected to {}", pass, tool, choose);
        return fmt::format("{}; the setup questions were skipped", pass);
    }

    std::string Quote(std::string_view value)
    {
        std::string quoted = "\"";
        for (char const c : value)
        {
            if (c == '\\' || c == '"')
            {
                quoted.push_back('\\');
                quoted.push_back(c);
            }
            else if (c == '\n')
                quoted += "\\n";
            else if (c == '\t')
                quoted += "\\t";
            else
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

    std::string SourceOf(std::optional<ConfigEntry> const& entry)
    {
        if (!entry)
            return "nothing";
        if (entry->Kind == ConfigSourceKind::Environment)
            return fmt::format("the environment variable {}", ClientLocator::PathText(entry->File));
        if (entry->Kind == ConfigSourceKind::Override)
            return "the command line";
        return ClientLocator::PathText(entry->File);
    }

    bool SameFile(std::filesystem::path const& left, std::filesystem::path const& right)
    {
        std::error_code error;
        if (std::filesystem::equivalent(left, right, error) && !error)
            return true;
        return left.lexically_normal() == right.lexically_normal();
    }

    std::string JoinPairs(std::vector<std::pair<std::string, std::string>> const& values)
    {
        std::string text;
        for (auto const& [key, value] : values)
            text += fmt::format("{}{} = {}", text.empty() ? "" : " and ", key, Quote(value));
        return text;
    }

    bool ReadExisting(std::filesystem::path const& file, std::optional<std::string>& content, std::string& error)
    {
        content.reset();
        std::error_code statusError;
        std::filesystem::file_status const status = std::filesystem::status(file, statusError);
        if (status.type() == std::filesystem::file_type::not_found)
            return true;
        if (statusError)
        {
            error = fmt::format("cannot check {}: {}", ClientLocator::PathText(file), statusError.message());
            return false;
        }
        if (!std::filesystem::is_regular_file(status))
        {
            error = fmt::format("{} is not a regular file", ClientLocator::PathText(file));
            return false;
        }
        std::ifstream stream(file, std::ios::binary);
        if (!stream)
        {
            error = fmt::format("cannot open {} to merge the new values into it", ClientLocator::PathText(file));
            return false;
        }
        std::string text(ClientSetup::MaxSavedFileBytes + 1, '\0');
        stream.read(text.data(), static_cast<std::streamsize>(text.size()));
        if (stream.bad())
        {
            error = fmt::format("cannot read {} to merge the new values into it", ClientLocator::PathText(file));
            return false;
        }
        text.resize(static_cast<std::size_t>(stream.gcount()));
        if (text.size() > ClientSetup::MaxSavedFileBytes)
        {
            error = fmt::format("{} is larger than {} bytes, so it is not rewritten", ClientLocator::PathText(file), ClientSetup::MaxSavedFileBytes);
            return false;
        }
        content = std::move(text);
        return true;
    }

    std::optional<std::string> Merge(std::optional<std::string> const& existing, std::vector<std::pair<std::string, std::string>> const& values, std::filesystem::path const& file, std::string& error)
    {
        std::string text;
        std::set<std::string> written;
        if (existing && !Ambrose::Trim(*existing).empty())
        {
            ParsedConfig const parsed = ConfigMgr::ParseText(*existing, file, ConfigSourceKind::ModuleConfig);
            if (!parsed.Errors.empty())
            {
                error = fmt::format("{} has errors, so it is not rewritten: {}", ClientLocator::PathText(file), parsed.Errors.front().ToString());
                return std::nullopt;
            }
            std::map<std::size_t, std::size_t> replaced;
            for (auto const& [key, entry] : parsed.Entries)
                for (std::size_t index = 0; index < values.size(); ++index)
                    if (values[index].first == key)
                        replaced[entry.Line] = index;
            std::string_view const source = *existing;
            std::size_t position = 0;
            std::size_t lineNumber = 0;
            while (position < source.size())
            {
                std::size_t const end = source.find('\n', position);
                std::string_view const line = source.substr(position, end == std::string_view::npos ? std::string_view::npos : end - position);
                position = end == std::string_view::npos ? source.size() : end + 1;
                ++lineNumber;
                auto const found = replaced.find(lineNumber);
                if (found == replaced.end())
                    text.append(line);
                else
                {
                    if (lineNumber == 1 && line.starts_with("\xEF\xBB\xBF"))
                        text += "\xEF\xBB\xBF";
                    auto const& [key, value] = values[found->second];
                    text += fmt::format("{} = {}", key, Quote(value));
                    if (line.ends_with('\r'))
                        text.push_back('\r');
                    written.insert(key);
                }
                if (end != std::string_view::npos)
                    text.push_back('\n');
            }
            if (!text.empty() && text.back() != '\n')
                text.push_back('\n');
        }
        else
            text = std::string(SavedHeader);
        for (auto const& [key, value] : values)
            if (written.insert(key).second)
                text += fmt::format("{} = {}\n", key, Quote(value));
        return text;
    }

    bool ReplaceFile(std::filesystem::path const& file, std::string const& text, std::string& error)
    {
        std::filesystem::path temporary = file;
        temporary += fmt::format(".{:08x}.partial", std::random_device{}());
        {
            std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
            if (stream)
                stream.write(text.data(), static_cast<std::streamsize>(text.size()));
            stream.close();
            if (!stream)
            {
                error = fmt::format("cannot write {}", ClientLocator::PathText(temporary));
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
        return true;
    }

    bool SaveValues(std::filesystem::path const& file, std::vector<std::pair<std::string, std::string>> const& values, std::optional<std::string>& previous, std::string& error)
    {
        for (auto const& [key, value] : values)
        {
            bool const control = std::any_of(value.begin(), value.end(), [](char c)
            {
                unsigned char const byte = static_cast<unsigned char>(c);
                return (byte < 0x20 && c != '\n' && c != '\t') || byte == 0x7F;
            });
            if (control)
            {
                error = fmt::format("{} holds a control character that a configuration file cannot keep", key);
                return false;
            }
        }
        std::filesystem::path const folder = file.parent_path();
        std::error_code created;
        std::filesystem::create_directories(folder, created);
        std::error_code checked;
        if (created || !std::filesystem::is_directory(folder, checked))
        {
            error = fmt::format("cannot create the folder {}{}", ClientLocator::PathText(folder), created ? ": " + created.message() : std::string());
            return false;
        }
        if (!ReadExisting(file, previous, error))
            return false;
        std::optional<std::string> const text = Merge(previous, values, file, error);
        if (!text)
            return false;
        ParsedConfig const check = ConfigMgr::ParseText(*text, file, ConfigSourceKind::ModuleConfig);
        if (!check.Errors.empty())
        {
            error = fmt::format("the new {} would not read back: {}", ClientLocator::PathText(file), check.Errors.front().ToString());
            return false;
        }
        for (auto const& [key, value] : values)
        {
            auto const found = std::find_if(check.Entries.begin(), check.Entries.end(), [&key](auto const& entry) { return entry.first == key; });
            if (found == check.Entries.end() || found->second.Value != value)
            {
                error = fmt::format("the new {} would not read {} back as chosen", ClientLocator::PathText(file), key);
                return false;
            }
        }
        return ReplaceFile(file, *text, error);
    }

    std::filesystem::path SavedFile(std::filesystem::path const& configFile)
    {
        return configFile.parent_path() / "conf.d" / ConfigMgr::PathFromUtf8(ClientSetup::SavedFileName);
    }

    void SaveChosen(ConfigMgr& config, std::vector<std::pair<std::string, std::string>> const& chosen, std::string const& app, ClientSetupResult& result, ClientSetup::Report const& report)
    {
        std::filesystem::path const file = SavedFile(config.GetFilename());
        std::string const target = ClientLocator::PathText(file);
        std::optional<std::string> previous;
        std::string error;
        if (!SaveValues(file, chosen, previous, error))
        {
            report(true, fmt::format("Could not save {} to {}: {}. {} uses the choice for this run only; add that setting to {} yourself to keep it", JoinPairs(chosen), target, error, app, target));
            return;
        }
        result.SavedTo = file;
        ConfigLoadResult const reloaded = config.Reload();
        if (!reloaded.Succeeded())
        {
            std::string restoreError;
            bool restored = false;
            if (previous)
                restored = ReplaceFile(file, *previous, restoreError);
            else
            {
                std::error_code removed;
                restored = std::filesystem::remove(file, removed) && !removed;
                if (!restored)
                    restoreError = removed.message();
            }
            if (restored)
                config.Reload();
            for (ConfigIssue const& issue : reloaded.Errors)
                report(true, fmt::format("Reloading the configuration after saving {} to {} failed: {}", JoinPairs(chosen), target, issue.ToString()));
            report(true, restored ? fmt::format("{} was put back as it was, and {} uses the choice for this run only", target, app)
                                  : fmt::format("{} could not be put back as it was: {}; fix or delete it before the next start", target, restoreError));
            result.SavedTo.clear();
            return;
        }
        for (auto const& [key, value] : chosen)
        {
            std::optional<ConfigEntry> const entry = config.Resolve(key);
            if (entry && entry->Kind == ConfigSourceKind::ModuleConfig && SameFile(entry->File, file) && entry->Value == value)
            {
                result.Saved.emplace_back(key, value);
                report(false, fmt::format("Saved {} = {} to {}; edit or delete that file to choose again", key, Quote(value), target));
                continue;
            }
            report(true, fmt::format("{} = {} was written to {}, but {} sets {} = {} and overrides it, so {} uses the new value for this run only; remove {} there to keep the choice",
                key, Quote(value), target, SourceOf(entry), key, Quote(entry ? entry->Value : std::string()), app, key));
        }
    }

    void ReportSavedOverrides(ConfigMgr const& config, ClientSetup::Report const& report)
    {
        std::filesystem::path const appFile = config.GetFilename();
        std::optional<ParsedConfig> appConfig;
        for (std::string_view const key : { ClientSetup::ClientDirKey, ClientSetup::TypeDumpKey })
        {
            std::optional<ConfigEntry> const entry = config.Resolve(std::string(key));
            if (!entry || entry->Kind != ConfigSourceKind::ModuleConfig || entry->File.filename() != ConfigMgr::PathFromUtf8(ClientSetup::SavedFileName))
                continue;
            if (!appConfig)
            {
                std::ifstream stream(appFile, std::ios::binary);
                std::string text(MaxAppConfigBytes + 1, '\0');
                stream.read(text.data(), static_cast<std::streamsize>(text.size()));
                text.resize(static_cast<std::size_t>(stream.gcount()));
                if (text.size() > MaxAppConfigBytes)
                    text.clear();
                appConfig = ConfigMgr::ParseText(text, appFile, ConfigSourceKind::Config);
            }
            auto const own = std::find_if(appConfig->Entries.begin(), appConfig->Entries.end(), [&key](auto const& pair) { return pair.first == key; });
            if (own == appConfig->Entries.end() || Ambrose::Trim(own->second.Value).empty()
                || ClientSetup::ConfigPath(ConfigMgr::PathFromUtf8(Ambrose::Trim(own->second.Value))) == ClientSetup::ConfigPath(ConfigMgr::PathFromUtf8(Ambrose::Trim(entry->Value))))
                continue;
            report(true, fmt::format("{} = {} in {} overrides {} = {} in {}; edit or delete {} to use the value in {}", key, Quote(entry->Value), ClientLocator::PathText(entry->File), key, Quote(own->second.Value),
                ClientLocator::PathText(appFile), ClientLocator::PathText(entry->File), ClientLocator::PathText(appFile)));
        }
    }

    std::optional<std::filesystem::path> BuildDump(ClientSetup::TypeDumpProvider const& types, ClientInstall const& install, std::string& error)
    {
        if (!types)
        {
            error = "no type dump builder is available";
            return std::nullopt;
        }
        return types(install, TypeDumpBuild::IfNeeded, error);
    }

    std::optional<std::filesystem::path> FindBuiltDump(ClientSetup::TypeDumpProvider const& types, ClientInstall const& install)
    {
        if (!types)
            return std::nullopt;
        std::string ignored;
        return types(install, TypeDumpBuild::Never, ignored);
    }
}

std::optional<SetupMode> ClientSetup::ParseMode(std::string_view text)
{
    std::string const lower = Ambrose::ToLower(Ambrose::Trim(text));
    if (lower == "auto")
        return SetupMode::Auto;
    if (lower == "ask")
        return SetupMode::Ask;
    if (lower == "off")
        return SetupMode::Off;
    return std::nullopt;
}

std::string_view ClientSetup::ModeName(SetupMode mode)
{
    if (mode == SetupMode::Ask)
        return "ask";
    if (mode == SetupMode::Off)
        return "off";
    return "auto";
}

SetupMode ClientSetup::ModeFor(ConfigMgr const& config, Report const& report)
{
    std::optional<ConfigEntry> const entry = config.Resolve(std::string(ModeKey));
    if (!entry || Ambrose::Trim(entry->Value).empty())
        return SetupMode::Auto;
    if (std::optional<SetupMode> const mode = ParseMode(entry->Value))
        return *mode;
    if (report)
        report(true, fmt::format("{} '{}' is not auto, ask or off, so setup runs in auto mode", ModeKey, Ambrose::ForLog(entry->Value)));
    return SetupMode::Auto;
}

SetupMode ClientSetup::ModeForTool(ClientSystem const& system, std::ostream& err, std::string_view toolName)
{
    std::optional<std::string> const value = system.GetEnv(std::string(ModeVariable));
    if (!value || Ambrose::Trim(*value).empty())
        return SetupMode::Auto;
    if (std::optional<SetupMode> const mode = ParseMode(*value))
        return *mode;
    err << fmt::format("{}: {} '{}' is not auto, ask or off, so setup runs in auto mode\n", toolName, ModeVariable, Ambrose::ForLog(*value));
    return SetupMode::Auto;
}

ClientSetupResult ClientSetup::ForServer(ConfigMgr& config, SetupPrompt& prompt, ClientSystem const& system, TypeDumpProvider const& types, ClientSetupRequest const& request, Report const& report)
{
    Report const say = [&report](bool warning, std::string const& text)
    {
        if (report)
            report(warning, text);
    };
    std::function<void(std::string const&)> const skip = [&say](std::string const& text) { say(true, text); };
    ClientSetupResult result;
    result.Mode = ModeFor(config, say);
    ReportSavedOverrides(config, say);
    std::string const& app = request.AppName;
    bool searched = false;
    auto const search = [&result, &searched, &system, &skip]
    {
        if (!searched)
            result.Installs = UnicodeInstalls(ClientLocator::FindInstalls(system), skip);
        searched = true;
    };
    std::vector<std::pair<std::string, std::string>> chosen;
    auto const keep = [&chosen, &system, &say, &app](std::string_view key, std::filesystem::path const& path)
    {
        std::filesystem::path const absolute = ClientLocator::AbsoluteFor(system, path);
        if (std::optional<std::string> const text = UnicodeText(absolute))
            chosen.emplace_back(key, *text);
        else
            say(true, fmt::format("{} {} is not saved, because its path is not valid Unicode and cannot be written to a configuration file; {} uses it for this run only", key, ClientLocator::PathText(absolute), app));
    };

    std::string const clientDir = config.GetOption<std::string>(std::string(ClientDirKey), "", true);
    std::optional<ClientInstall> install = clientDir.empty() ? std::nullopt : ClientInstall::Inspect(system, ConfigMgr::PathFromUtf8(clientDir));
    bool inspected = install.has_value();
    bool picked = false;
    bool const clientLocked = Locked(config, ClientDirKey);
    std::string const clientReason = clientDir.empty() ? std::string("ClientDir is not set") : fmt::format("ClientDir {} holds no Wizard101 install", Ambrose::ForLog(clientDir, 512));
    if (request.NeedClient && !install)
    {
        if (!clientDir.empty() && (clientLocked || result.Mode == SetupMode::Off))
        {
            install = ClientInstall{};
            install->Root = ConfigMgr::PathFromUtf8(clientDir);
            if (clientLocked)
                say(true, fmt::format("{}, and {} sets it, so setup leaves it as it is", clientReason, SourceOf(config.Resolve(std::string(ClientDirKey)))));
        }
        else if (!clientLocked && result.Mode == SetupMode::Auto)
        {
            search();
            if (std::optional<ClientCandidate> const newest = Newest(result.Installs))
            {
                install = newest->Install;
                inspected = true;
                picked = true;
                if (clientDir.empty())
                {
                    say(false, fmt::format("{}, so {} uses the newest Wizard101 install on this machine: {}", clientReason, app, DescribeInstall(*newest)));
                    keep(ClientDirKey, install->Root);
                }
                else
                    say(true, fmt::format("{}, so {} uses the newest Wizard101 install on this machine for this run only: {}. ClientDir is not replaced in case its folder is only unavailable for now; point ClientDir in {} at the install to keep, or empty it to save the newest", clientReason, app,
                        DescribeInstall(*newest), SourceOf(config.Resolve(std::string(ClientDirKey)))));
            }
            else if (prompt.IsInteractive())
            {
                if (std::optional<ClientInstall> const answer = AskToInstallAndRetry(prompt, system, app, result.Installs, [&] { searched = false; search(); }))
                {
                    install = answer;
                    inspected = true;
                    picked = true;
                    keep(ClientDirKey, install->Root);
                }
                else
                    say(true, fmt::format("{}, and no Wizard101 install was found on this machine, so {} runs without client data; install Wizard101, or set ClientDir in conf.d/{} to the folder that holds Data and Bin", clientReason, app, SavedFileName));
            }
            else
                say(true, fmt::format("{}, and no Wizard101 install was found on this machine, so {} runs without client data; install Wizard101, or set ClientDir in conf.d/{} to the folder that holds Data and Bin", clientReason, app, SavedFileName));
        }
        else if (!clientLocked && result.Mode == SetupMode::Ask)
        {
            search();
            if (result.Installs.empty())
            {
                std::optional<ClientInstall> const answer = prompt.IsInteractive()
                    ? AskToInstallAndRetry(prompt, system, app, result.Installs, [&] { searched = false; search(); })
                    : std::nullopt;
                if (answer)
                {
                    install = answer;
                    inspected = true;
                    picked = true;
                    keep(ClientDirKey, install->Root);
                }
                else
                    say(true, fmt::format("{}, and no Wizard101 install was found on this machine; set ClientDir in conf.d/{} to the folder that holds Data and Bin", clientReason, SavedFileName));
            }
            else
            {
                if (prompt.IsInteractive())
                {
                    if (std::optional<ClientInstall> const answer = AskInstall(prompt, system, result.Installs, fmt::format("{} needs your own Wizard101 install, and {}.", app, clientReason)))
                    {
                        install = answer;
                        inspected = true;
                        picked = true;
                        keep(ClientDirKey, install->Root);
                    }
                }
                if (!install && !prompt.IsInteractive())
                    say(true, fmt::format("{}, and Wizard101 was found on this machine: {}. {}", clientReason, ListInstalls(result.Installs), ServerAdvice(prompt, app, ClientDirKey)));
            }
        }
    }

    bool dumpInUse = false;
    std::string dumpProblem;
    if (request.NeedTypeDump)
    {
        std::string const typeDump = config.GetOption<std::string>(std::string(TypeDumpKey), "", true);
        std::filesystem::path const dumpPath = ConfigMgr::PathFromUtf8(typeDump);
        bool const dumpUsable = !typeDump.empty() && ClientLocator::LooksLikeTypeDump(system, dumpPath);
        bool const dumpLocked = Locked(config, TypeDumpKey);
        std::string const dumpReason = typeDump.empty() ? std::string("TypeDumpPath is not set") : fmt::format("TypeDumpPath {} is not a type dump", Ambrose::ForLog(typeDump, 512));
        auto const useBuilt = [&](std::filesystem::path const& built)
        {
            result.TypeDump = built;
            result.TypeDumpBuilt = true;
            result.TypeDumpError.clear();
            dumpInUse = true;
            say(false, fmt::format("Using the type dump {} built from {}", ClientLocator::PathText(built), install->Describe()));
        };
        auto const build = [&]
        {
            if (!inspected)
            {
                result.TypeDumpError = install ? fmt::format("{}, so no type dump can be built from it", clientReason) : std::string("no Wizard101 install is configured or found, so no type dump can be built");
                return;
            }
            std::string error;
            if (std::optional<std::filesystem::path> const built = BuildDump(types, *install, error))
            {
                useBuilt(*built);
                return;
            }
            result.TypeDumpError = fmt::format("the type dump for {} could not be built: {}", install->Describe(), error);
            say(true, fmt::format("The type dump for {} could not be built: {}", install->Describe(), error));
        };
        auto const findBuilt = [&]
        {
            std::optional<std::filesystem::path> const built = inspected ? FindBuiltDump(types, *install) : std::nullopt;
            if (built)
                useBuilt(*built);
            return built.has_value();
        };

        if (dumpUsable)
        {
            result.TypeDump = dumpPath;
            dumpInUse = true;
        }
        else if (dumpLocked && typeDump.empty())
            result.TypeDumpError = fmt::format("TypeDumpPath is set empty by {}, so setup leaves it as it is", SourceOf(config.Resolve(std::string(TypeDumpKey))));
        else if (!typeDump.empty() && (dumpLocked || result.Mode == SetupMode::Off))
        {
            result.TypeDump = dumpPath;
            if (dumpLocked)
                say(true, fmt::format("{}, and {} sets it, so setup leaves it as it is", dumpReason, SourceOf(config.Resolve(std::string(TypeDumpKey)))));
        }
        else if (result.Mode == SetupMode::Off)
            result.TypeDumpError = "TypeDumpPath is not set, and Setup.Mode is off, so no type dump is built";
        else if (result.Mode == SetupMode::Auto)
        {
            if (!typeDump.empty())
                say(true, fmt::format("{}, so {} uses the type dump built from its install instead", dumpReason, app));
            build();
        }
        else if (!findBuilt())
        {
            search();
            std::vector<TypeDumpCandidate> const dumps = UnicodeDumps(ClientLocator::FindTypeDumps(system, WithConfigured(result.Installs, inspected ? install : std::nullopt)), skip);
            bool const unfitted = inspected && !HasDumpNamedFor(system, dumps, *install);
            std::string const question = fmt::format("{} needs the type dump made from your install, and {}.", app, dumpReason);
            if (unfitted && prompt.IsInteractive())
            {
                if (prompt.Confirm(fmt::format("{} Build it from {} now?", question, install->Describe())))
                    build();
                else
                    result.TypeDumpError = fmt::format("{}, and the type dump was not built", dumpReason);
            }
            if (!result.TypeDump && prompt.IsInteractive() && !dumps.empty())
            {
                if (std::optional<std::filesystem::path> const answer = AskTypeDump(prompt, system, dumps, unfitted ? NamedForQuestion(question, *install) : question))
                {
                    result.TypeDump = *answer;
                    result.TypeDumpError.clear();
                    dumpInUse = true;
                    keep(TypeDumpKey, *answer);
                }
                else if (result.TypeDumpError.empty())
                    result.TypeDumpError = fmt::format("{}, and no type dump was chosen", dumpReason);
            }
            if (!result.TypeDump && !prompt.IsInteractive())
            {
                if (unfitted)
                    say(true, fmt::format("{}, and no type dump was found for {}. {}", dumpReason, install->Describe(), ServerAdvice(prompt, app, TypeDumpKey)));
                else if (!dumps.empty())
                    say(true, fmt::format("{}, and a type dump was found on this machine: {}. {}", dumpReason, ListDumps(dumps), ServerAdvice(prompt, app, TypeDumpKey)));
            }
        }
        if (!result.TypeDump && result.TypeDumpError.empty())
            result.TypeDumpError = dumpReason;
        dumpProblem = result.TypeDumpError.empty() ? dumpReason : result.TypeDumpError;
    }

    if (request.NeedTypeDump && !dumpInUse)
    {
        auto const saved = std::find_if(chosen.begin(), chosen.end(), [](auto const& pair) { return pair.first == ClientDirKey; });
        std::optional<std::string> unsaved;
        if (saved != chosen.end())
        {
            unsaved = saved->second;
            chosen.erase(saved);
        }
        std::string_view const again = result.Mode == SetupMode::Ask ? "asks again" : "tries again";
        if (picked && request.ClientNeedsTypeDump)
        {
            std::string const what = unsaved ? fmt::format("ClientDir {} was not saved", Quote(*unsaved)) : fmt::format("The install {} is not used", install->Describe());
            say(true, fmt::format("{}, because {} needs a type dump with its install and {}; {} starts without client data and {} on its next start", what, app, dumpProblem, app, again));
            install.reset();
        }
        else if (unsaved)
            say(true, fmt::format("ClientDir {} was not saved, because no type dump is in use for it: {}; {} uses it for this run only and {} on its next start", Quote(*unsaved), dumpProblem, app, again));
    }
    result.Install = install;
    if (!chosen.empty())
        SaveChosen(config, chosen, app, result, say);
    return result;
}

ClientSetupResult ClientSetup::ForTool(SetupMode mode, std::optional<std::string>& client, std::optional<std::string>* typeDump, SetupPrompt& prompt, ClientSystem const& system, TypeDumpProvider const& types, std::string_view toolName, std::ostream& err)
{
    ClientSetupResult result;
    result.Mode = mode;
    std::function<void(std::string const&)> const skip = [&err, toolName](std::string const& text) { err << fmt::format("{}: {}\n", toolName, text); };
    bool searched = false;
    auto const search = [&result, &searched, &system, &skip]
    {
        if (!searched)
            result.Installs = UnicodeInstalls(ClientLocator::FindInstalls(system), skip);
        searched = true;
    };
    std::string usedInstall;
    std::string usedDump;

    if (!client)
    {
        search();
        if (mode == SetupMode::Auto)
        {
            if (std::optional<ClientCandidate> const newest = Newest(result.Installs))
            {
                client = ConfigPath(newest->Install.Root);
                result.Install = newest->Install;
                usedInstall = fmt::format("the newest install on this machine, {}", DescribeInstall(*newest));
            }
            else
                err << fmt::format("{}: no Wizard101 install was found on this machine\n", toolName);
        }
        else if (mode == SetupMode::Ask && prompt.IsInteractive() && !result.Installs.empty())
        {
            if (std::optional<ClientInstall> const picked = AskInstall(prompt, system, result.Installs, fmt::format("{} needs your own Wizard101 install, and none was named.", toolName)))
            {
                client = ConfigPath(picked->Root);
                result.Install = picked;
            }
        }
        if (!client && !result.Installs.empty() && mode != SetupMode::Auto && !prompt.IsInteractive())
            err << fmt::format("{}: Wizard101 was found on this machine: {}. {}\n", toolName, ListInstalls(result.Installs), ToolAdvice(mode, prompt, toolName, "Pass --client with one of them", "use the newest", "choose"));
    }
    else if (!client->empty())
        result.Install = ClientInstall::Inspect(system, ConfigMgr::PathFromUtf8(*client));

    if (typeDump && !*typeDump)
    {
        auto const useBuilt = [&](std::filesystem::path const& built)
        {
            *typeDump = ConfigPath(built);
            result.TypeDump = built;
            result.TypeDumpBuilt = true;
            result.TypeDumpError.clear();
            usedDump = fmt::format("the type dump {} built from {}", ClientLocator::PathText(built), usedInstall.empty() ? result.Install->Describe() : std::string("it"));
        };
        auto const build = [&]
        {
            std::string error;
            if (std::optional<std::filesystem::path> const built = BuildDump(types, *result.Install, error))
            {
                useBuilt(*built);
                return;
            }
            result.TypeDumpError = error;
            err << fmt::format("{}: cannot build the type dump for {}: {}\n", toolName, result.Install->Describe(), error);
        };
        auto const findBuilt = [&]
        {
            std::optional<std::filesystem::path> const built = result.Install ? FindBuiltDump(types, *result.Install) : std::nullopt;
            if (built)
                useBuilt(*built);
            return built.has_value();
        };
        if (mode == SetupMode::Auto)
        {
            if (result.Install)
                build();
            else if (client && !client->empty())
            {
                result.TypeDumpError = fmt::format("{} holds no Wizard101 install, so no type dump can be built from it", Ambrose::ForLog(*client, 512));
                err << fmt::format("{}: {}\n", toolName, result.TypeDumpError);
            }
        }
        else if (mode == SetupMode::Off || !findBuilt())
        {
            search();
            std::vector<TypeDumpCandidate> const dumps = UnicodeDumps(ClientLocator::FindTypeDumps(system, WithConfigured(result.Installs, result.Install)), skip);
            bool const unfitted = result.Install && !HasDumpNamedFor(system, dumps, *result.Install);
            std::string const question = fmt::format("{} needs the type dump made from your install, and none was named.", toolName);
            if (unfitted && mode == SetupMode::Ask && prompt.IsInteractive())
            {
                if (prompt.Confirm(fmt::format("{} Build it from {} now?", question, result.Install->Describe())))
                    build();
            }
            if (!*typeDump && mode == SetupMode::Ask && prompt.IsInteractive() && !dumps.empty())
            {
                if (std::optional<std::filesystem::path> const picked = AskTypeDump(prompt, system, dumps, unfitted ? NamedForQuestion(question, *result.Install) : question))
                {
                    *typeDump = ConfigPath(*picked);
                    result.TypeDump = *picked;
                    result.TypeDumpError.clear();
                }
            }
            if (!*typeDump && !prompt.IsInteractive())
            {
                if (unfitted)
                    err << fmt::format("{}: no type dump was found for {}. {}\n", toolName, result.Install->Describe(), ToolAdvice(mode, prompt, toolName, "Pass --type-dump", "build it", "be asked to build it"));
                else if (!dumps.empty())
                    err << fmt::format("{}: a type dump was found on this machine: {}. {}\n", toolName, ListDumps(dumps), ToolAdvice(mode, prompt, toolName, "Pass --type-dump with one of them", "build one from the install", "choose"));
            }
        }
    }

    if (!usedInstall.empty() || !usedDump.empty())
    {
        std::string const used = usedInstall.empty() ? usedDump : usedDump.empty() ? usedInstall : usedInstall + ", and " + usedDump;
        std::string const flags = usedInstall.empty() ? "--type-dump" : usedDump.empty() ? "--client" : "--client and --type-dump";
        err << fmt::format("{}: using {}; pass {} to choose otherwise\n", toolName, used, flags);
    }
    return result;
}

ClientSetup::TypeDumpProvider ClientSetup::BuiltTypeDumps(ClientSystem const& system, std::filesystem::path extractor, std::chrono::seconds timeout, Report report, std::function<bool()> shouldStop)
{
    if (!report)
        report = [](bool, std::string const&) {};
    if (!shouldStop)
        shouldStop = [] { return false; };
    return [&system, extractor = std::move(extractor), timeout, report = std::move(report), shouldStop = std::move(shouldStop)](ClientInstall const& install, TypeDumpBuild build, std::string& error) -> std::optional<std::filesystem::path>
    {
        std::filesystem::path const dataFolder = ClientLocator::GetDataFolder(system);
        if (build == TypeDumpBuild::Never)
            return CurrentTypeDump(install, dataFolder, error);
        TypeDumpCacheOptions options;
        options.DataFolder = dataFolder;
        options.Extractor = extractor.empty() ? TypeDumpCache::DefaultExtractor(system.GetExecutableDirectory()) : extractor;
        options.Timeout = timeout;
        options.Report = report;
        options.ShouldStop = shouldStop;
        return TypeDumpCache::Ensure(install, options, error);
    };
}

ClientSetup::TypeDumpProvider ClientSetup::ServerTypeDumps(ConfigMgr const& config, ClientSystem const& system, Report report, std::function<bool()> shouldStop)
{
    std::string const extractor(Ambrose::Trim(config.GetOption<std::string>(std::string(TypeExtractorKey), "", true)));
    uint32 const timeout = config.GetOption<uint32>(std::string(TypeExtractTimeoutKey), DefaultTypeExtractTimeoutSeconds, true);
    return BuiltTypeDumps(system, extractor.empty() ? std::filesystem::path() : ConfigMgr::PathFromUtf8(extractor), std::chrono::seconds(timeout), std::move(report), std::move(shouldStop));
}

ClientSetup::TypeDumpProvider ClientSetup::ToolTypeDumps(ClientSystem const& system, std::string_view toolName, std::ostream& err)
{
    Report report = [&err, tool = std::string(toolName)](bool, std::string const& text) { err << tool << ": " << text << '\n'; };
    TypeDumpProvider built = BuiltTypeDumps(system, {}, std::chrono::seconds(DefaultTypeExtractTimeoutSeconds), std::move(report), [] { return ToolStopSignal != 0; });
    return [built = std::move(built)](ClientInstall const& install, TypeDumpBuild build, std::string& error) -> std::optional<std::filesystem::path>
    {
        if (build == TypeDumpBuild::Never)
            return built(install, build, error);
        ToolStopSignal = 0;
        auto const previousInterrupt = std::signal(SIGINT, OnToolStopSignal);
        auto const previousTerminate = std::signal(SIGTERM, OnToolStopSignal);
        std::optional<std::filesystem::path> dump = built(install, build, error);
        std::signal(SIGINT, previousInterrupt == SIG_ERR ? SIG_DFL : previousInterrupt);
        std::signal(SIGTERM, previousTerminate == SIG_ERR ? SIG_DFL : previousTerminate);
        return dump;
    };
}

std::unique_ptr<SetupPrompt> ClientSetup::ServerPrompt(std::ostream& out, ConfigMgr const& config)
{
    SetupMode const mode = ModeFor(config, nullptr);
    uint32 const timeout = config.GetOption<uint32>(std::string(PromptTimeoutKey), DefaultTimeoutSeconds, true);
    return SetupPrompt::ForProcess(out, mode == SetupMode::Ask, std::chrono::seconds(timeout));
}

std::unique_ptr<SetupPrompt> ClientSetup::ToolPrompt(std::ostream& out, SetupMode mode)
{
    std::optional<std::string> const timeoutText = Ambrose::GetEnv(std::string(PromptTimeoutVariable));
    std::optional<unsigned> const timeout = timeoutText ? Ambrose::StringTo<unsigned>(Ambrose::Trim(*timeoutText)) : std::nullopt;
    return SetupPrompt::ForProcess(out, mode == SetupMode::Ask, std::chrono::seconds(timeout.value_or(DefaultTimeoutSeconds)));
}

std::optional<ClientCandidate> ClientSetup::Newest(std::vector<ClientCandidate> const& installs)
{
    std::optional<ClientCandidate> best;
    for (ClientCandidate const& candidate : installs)
    {
        uint64 const revision = candidate.Install.RevisionNumber();
        if (!best || revision > best->Install.RevisionNumber() || (revision == best->Install.RevisionNumber() && candidate.Install.HasProgram && !best->Install.HasProgram))
            best = candidate;
    }
    return best;
}

bool ClientSetup::Save(std::filesystem::path const& configFile, std::vector<std::pair<std::string, std::string>> const& values, std::filesystem::path& savedTo, std::string& error)
{
    std::filesystem::path const file = SavedFile(configFile);
    std::optional<std::string> previous;
    if (!SaveValues(file, values, previous, error))
        return false;
    savedTo = file;
    return true;
}

std::string ClientSetup::ConfigPath(std::filesystem::path const& path)
{
    std::optional<std::string> text = UnicodeText(path);
    return text ? std::move(*text) : ClientLocator::PathText(path.lexically_normal());
}

std::filesystem::path ClientSetup::TypedPath(ClientSystem const& system, std::string_view typed)
{
    std::string_view text = Ambrose::Trim(typed);
    if (text.size() >= 2 && (text.front() == '"' || text.front() == '\'') && text.back() == text.front())
        text = Ambrose::Trim(text.substr(1, text.size() - 2));
    if (!system.IsWindows() && (text == "~" || text.starts_with("~/")))
    {
        std::optional<std::string> const home = system.GetEnv("HOME");
        if (home && !home->empty())
            return ConfigMgr::PathFromUtf8(*home + std::string(text.substr(1)));
    }
    return ConfigMgr::PathFromUtf8(text);
}
