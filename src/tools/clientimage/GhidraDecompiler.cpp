/*
 * Project Ambrose by Imjustchico
 * Finds a Java through JAVA_HOME or the PATH to ask LaunchSupport for the JDK Ghidra runs on and the virtual machine arguments it wants, starts ghidra.Ghidra with the AnalyzeHeadless class over the project, writes the decompile script into the work folder when it is missing or differs, reads the markers the script prints back into each function asked for, taking only what it printed under a program whose SHA-256 is the client's, and keeps the cache as one file per function, its name on the first line and its C after it.
 */

#include "GhidraDecompiler.h"
#include "ChildProcess.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "StringUtil.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <sstream>
#include <system_error>
#include <utility>

namespace
{
    constexpr std::string_view ScriptText = R"(from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

print("AMBROSE-PROGRAM %s" % currentProgram.getExecutableSHA256())
decompiler = DecompInterface()
decompiler.openProgram(currentProgram)
monitor = ConsoleTaskMonitor()
for text in getScriptArgs():
    address = toAddr(text)
    function = getFunctionAt(address)
    if function is None:
        function = getFunctionContaining(address)
    if function is None:
        function = createFunction(address, None)
    if function is None:
        print("AMBROSE-FAILED %s no function is found or can be made there" % text)
        continue
    result = decompiler.decompileFunction(function, 600, monitor)
    if result is None or not result.decompileCompleted():
        reason = "the decompiler gave no result" if result is None else (result.getErrorMessage() or "the decompiler did not finish")
        print("AMBROSE-FAILED %s %s" % (text, reason.strip().replace("\n", " ")))
        continue
    print("AMBROSE-BEGIN %s %s" % (text, function.getName()))
    print(result.getDecompiledFunction().getC())
    print("AMBROSE-END %s" % text)
)";

    constexpr std::string_view ProgramMarker = "AMBROSE-PROGRAM ";
    constexpr std::string_view BeginMarker = "AMBROSE-BEGIN ";
    constexpr std::string_view EndMarker = "AMBROSE-END ";
    constexpr std::string_view FailedMarker = "AMBROSE-FAILED ";
    constexpr std::size_t ReportedLines = 5;

#ifdef _WIN32
    constexpr std::string_view JavaExecutable = "java.exe";
    constexpr char PathSeparator = ';';
#else
    constexpr std::string_view JavaExecutable = "java";
    constexpr char PathSeparator = ':';
#endif

    std::pair<std::string_view, std::string_view> SplitFirst(std::string_view text)
    {
        std::size_t const space = text.find(' ');
        if (space == std::string_view::npos)
            return { text, {} };
        return { text.substr(0, space), Ambrose::Trim(text.substr(space + 1)) };
    }

    bool IsFile(std::filesystem::path const& path)
    {
        std::error_code error;
        return std::filesystem::is_regular_file(path, error);
    }

    std::string LastProblems(std::vector<std::string> const& lines)
    {
        std::vector<std::string_view> problems;
        for (std::string const& line : lines)
            if (line.find("ERROR") != std::string::npos || line.find("Exception") != std::string::npos)
                problems.push_back(Ambrose::Trim(line));
        if (problems.empty())
            for (auto line = lines.rbegin(); line != lines.rend() && problems.size() < ReportedLines; ++line)
                if (!Ambrose::Trim(*line).empty())
                    problems.insert(problems.begin(), Ambrose::Trim(*line));
        if (problems.size() > ReportedLines)
            problems.erase(problems.begin(), problems.end() - ReportedLines);
        std::string text;
        for (std::string_view const problem : problems)
            text += fmt::format("{}{}", text.empty() ? "" : " | ", problem);
        return text.empty() ? std::string("it printed nothing") : text;
    }
}

std::optional<GhidraProject> GhidraProject::FromPath(std::filesystem::path const& path)
{
    if (path.empty())
        return std::nullopt;
    GhidraProject project;
    project.Location = path.parent_path();
    project.Name = ConfigMgr::PathToUtf8(path.extension() == ".gpr" ? path.stem() : path.filename());
    if (project.Name.empty())
        return std::nullopt;
    return project;
}

std::filesystem::path GhidraProject::File() const
{
    return Location / ConfigMgr::PathFromUtf8(Name + ".gpr");
}

bool GhidraProject::Exists() const
{
    return IsFile(File());
}

GhidraDecompiler::GhidraDecompiler(GhidraSettings settings) : _settings(std::move(settings))
{
}

std::string_view GhidraDecompiler::Script() noexcept
{
    return ScriptText;
}

std::string GhidraDecompiler::AddressText(uint64 address)
{
    return fmt::format("0x{:x}", address);
}

std::optional<std::filesystem::path> GhidraDecompiler::FindJava(std::string& error)
{
    if (std::optional<std::string> const home = Ambrose::GetEnv("JAVA_HOME"); home && !home->empty())
    {
        std::filesystem::path const java = ConfigMgr::PathFromUtf8(*home) / "bin" / JavaExecutable;
        if (IsFile(java))
            return java;
    }
    if (std::optional<std::string> const path = Ambrose::GetEnv("PATH"))
    {
        for (std::string_view entry : Ambrose::Tokenize(*path, PathSeparator, false))
        {
            entry = Ambrose::Trim(entry);
            if (entry.size() >= 2 && entry.front() == '"' && entry.back() == '"')
                entry = entry.substr(1, entry.size() - 2);
            if (entry.empty())
                continue;
            std::filesystem::path const java = ConfigMgr::PathFromUtf8(entry) / JavaExecutable;
            if (IsFile(java))
                return java;
        }
    }
    error = "no Java was found through JAVA_HOME or the PATH, and Ghidra needs one to start, a JDK 21 for Ghidra 11";
    return std::nullopt;
}

bool GhidraDecompiler::Prepare(std::string& error)
{
    if (!_java.empty())
        return true;
    std::filesystem::path const support = _settings.Install / "support" / "LaunchSupport.jar";
    std::filesystem::path const utility = _settings.Install / "Ghidra" / "Framework" / "Utility" / "lib" / "Utility.jar";
    if (!IsFile(support) || !IsFile(utility))
    {
        error = fmt::format("{} is not a Ghidra install: it holds no support/LaunchSupport.jar and Ghidra/Framework/Utility/lib/Utility.jar", ConfigMgr::PathToUtf8(_settings.Install));
        return false;
    }
    std::optional<std::filesystem::path> const bootstrap = FindJava(error);
    if (!bootstrap)
        return false;

    auto const ask = [&](std::filesystem::path const& java, std::vector<std::string> question, std::vector<std::string>& answer) -> bool
    {
        std::mutex mutex;
        ChildProcessOptions options;
        options.Program = java;
        options.Arguments = { "-cp", ConfigMgr::PathToUtf8(support), "LaunchSupport", ConfigMgr::PathToUtf8(_settings.Install) };
        options.Arguments.insert(options.Arguments.end(), question.begin(), question.end());
        options.Timeout = std::chrono::minutes(2);
        options.OnLine = [&mutex, &answer](std::string_view line, bool isError)
        {
            if (isError)
                return;
            std::string_view const trimmed = Ambrose::Trim(line);
            if (trimmed.empty())
                return;
            std::lock_guard const lock(mutex);
            answer.emplace_back(trimmed);
        };
        ChildProcessResult const result = ChildProcess::Run(options);
        if (result.Succeeded())
            return true;
        error = fmt::format("Ghidra's LaunchSupport {} did not answer through {}: {}", fmt::join(question, " "), ConfigMgr::PathToUtf8(java),
            !result.Started ? result.Error : result.TimedOut ? std::string("it timed out") : fmt::format("it exited with {}", result.ExitCode.value_or(-1)));
        return false;
    };

    std::vector<std::string> home;
    if (!ask(*bootstrap, { "-jdk_home", "-save" }, home))
        return false;
    if (home.empty())
    {
        error = "Ghidra's LaunchSupport found no JDK that Ghidra supports; install the JDK its install guide names, JDK 21 for Ghidra 11";
        return false;
    }
    std::filesystem::path const java = ConfigMgr::PathFromUtf8(home.back()) / "bin" / JavaExecutable;
    if (!IsFile(java))
    {
        error = fmt::format("Ghidra's LaunchSupport chose the JDK {}, which holds no bin/{}", home.back(), JavaExecutable);
        return false;
    }
    std::vector<std::string> arguments;
    if (!ask(java, { "-vmargs" }, arguments))
        return false;
    _java = java;
    _vmArguments = std::move(arguments);
    return true;
}

bool GhidraDecompiler::RunAnalyzer(std::vector<std::string> const& arguments, std::chrono::milliseconds timeout, Progress const& progress, std::vector<std::string>& lines, std::string& error)
{
    std::mutex mutex;
    ChildProcessOptions options;
    options.Program = _java;
    options.Arguments = _vmArguments;
#ifdef _WIN32
    if (std::optional<std::string> const profile = Ambrose::GetEnv("USERPROFILE"); profile && !profile->empty())
        options.Arguments.push_back("-Duser.home=" + *profile);
#endif
    options.Arguments.insert(options.Arguments.end(), { "-cp", ConfigMgr::PathToUtf8(_settings.Install / "Ghidra" / "Framework" / "Utility" / "lib" / "Utility.jar"), "ghidra.Ghidra",
        "ghidra.app.util.headless.AnalyzeHeadless" });
    options.Arguments.insert(options.Arguments.end(), arguments.begin(), arguments.end());
    options.Timeout = timeout;
    options.OnLine = [&mutex, &lines, &progress](std::string_view line, bool)
    {
        std::lock_guard const lock(mutex);
        std::string_view const text = Ambrose::TrimRight(line);
        lines.emplace_back(text);
        if (progress)
            progress(text);
    };
    ChildProcessResult const result = ChildProcess::Run(options);
    if (!result.Started)
    {
        error = fmt::format("Ghidra's analyzer could not be started through {}: {}", ConfigMgr::PathToUtf8(_java), result.Error);
        return false;
    }
    if (result.TimedOut)
    {
        error = fmt::format("Ghidra's analyzer did not finish within {} minutes", std::chrono::duration_cast<std::chrono::minutes>(timeout).count());
        return false;
    }
    if (result.ExitCode != 0)
    {
        error = fmt::format("Ghidra's analyzer exited with {}: {}", result.ExitCode.value_or(-1), LastProblems(lines));
        return false;
    }
    return true;
}

bool GhidraDecompiler::EnsureProject(Progress const& progress, std::string& error)
{
    if (_settings.Project.Exists())
        return true;
    if (!_settings.CreateProject)
    {
        error = fmt::format("there is no Ghidra project at {}", ConfigMgr::PathToUtf8(_settings.Project.File()));
        return false;
    }
    std::error_code created;
    std::filesystem::create_directories(_settings.Project.Location, created);
    if (created)
    {
        error = fmt::format("{} cannot be made: {}", ConfigMgr::PathToUtf8(_settings.Project.Location), created.message());
        return false;
    }
    std::vector<std::string> lines;
    if (!RunAnalyzer({ ConfigMgr::PathToUtf8(_settings.Project.Location), _settings.Project.Name, "-import", ConfigMgr::PathToUtf8(_settings.Program) }, std::chrono::milliseconds(0), progress,
            lines, error))
        return false;
    if (!_settings.Project.Exists())
    {
        error = fmt::format("Ghidra's analyzer finished without making the project {}: {}", ConfigMgr::PathToUtf8(_settings.Project.File()), LastProblems(lines));
        return false;
    }
    return true;
}

std::vector<DecompiledFunction> GhidraDecompiler::Decompile(std::vector<uint64> const& functions, Progress const& progress, std::string& error)
{
    std::vector<DecompiledFunction> results(functions.size());
    std::vector<uint64> missing;
    for (std::size_t index = 0; index < functions.size(); ++index)
    {
        if (std::optional<DecompiledFunction> cached = ReadCache(functions[index]))
            results[index] = std::move(*cached);
        else if (std::find(missing.begin(), missing.end(), functions[index]) == missing.end())
            missing.push_back(functions[index]);
    }
    if (missing.empty())
        return results;

    if (!Prepare(error) || !EnsureProject(progress, error))
        return {};
    std::error_code created;
    std::filesystem::create_directories(_settings.WorkFolder, created);
    std::filesystem::path const script = _settings.WorkFolder / ScriptName;
    std::ifstream existing(script, std::ios::binary);
    std::string const current((std::istreambuf_iterator<char>(existing)), std::istreambuf_iterator<char>());
    existing.close();
    if (current != ScriptText)
    {
        std::ofstream written(script, std::ios::binary | std::ios::trunc);
        written.write(ScriptText.data(), static_cast<std::streamsize>(ScriptText.size()));
        if (!written)
        {
            error = fmt::format("{} cannot be written", ConfigMgr::PathToUtf8(script));
            return {};
        }
    }

    std::vector<std::string> arguments = { ConfigMgr::PathToUtf8(_settings.Project.Location), _settings.Project.Name, "-process", "-noanalysis", "-readOnly", "-scriptPath",
        ConfigMgr::PathToUtf8(_settings.WorkFolder), "-postScript", std::string(ScriptName) };
    for (uint64 const address : missing)
        arguments.push_back(AddressText(address));
    std::vector<std::string> lines;
    if (!RunAnalyzer(arguments, DecompileTimeout, progress, lines, error))
        return {};
    std::vector<DecompiledFunction> decompiled = ParseOutput(lines, missing, _settings.ProgramSha256, error);
    if (decompiled.empty())
        return {};
    for (DecompiledFunction const& function : decompiled)
    {
        if (function.Ok())
            WriteCache(function);
        for (std::size_t index = 0; index < functions.size(); ++index)
            if (functions[index] == function.Address)
                results[index] = function;
    }
    return results;
}

std::vector<DecompiledFunction> GhidraDecompiler::ParseOutput(std::vector<std::string> const& lines, std::vector<uint64> const& functions, std::string_view sha256, std::string& error)
{
    std::map<std::string, uint64, std::less<>> wanted;
    for (uint64 const address : functions)
        wanted.emplace(AddressText(address), address);
    std::map<uint64, DecompiledFunction> found;
    bool sawProgram = false;
    bool matching = false;
    std::string otherProgram;
    DecompiledFunction* current = nullptr;
    std::string code;
    for (std::string const& line : lines)
    {
        std::string_view const text = Ambrose::TrimRight(line);
        if (current != nullptr)
        {
            std::size_t const end = text.find(EndMarker);
            if (end == std::string_view::npos)
            {
                code.append(text);
                code.push_back('\n');
                continue;
            }
            current->Code = std::string(Ambrose::Trim(code));
            current = nullptr;
            code.clear();
            continue;
        }
        std::size_t const marker = text.find("AMBROSE-");
        if (marker == std::string_view::npos)
            continue;
        std::string_view const tagged = text.substr(marker);
        if (tagged.starts_with(ProgramMarker))
        {
            std::string_view const program = Ambrose::Trim(tagged.substr(ProgramMarker.size()));
            sawProgram = true;
            matching = Ambrose::EqualsIgnoreCase(program, sha256);
            if (!matching && otherProgram.empty())
                otherProgram = program;
            continue;
        }
        if (!matching)
            continue;
        bool const begins = tagged.starts_with(BeginMarker);
        if (!begins && !tagged.starts_with(FailedMarker))
            continue;
        auto const [address, rest] = SplitFirst(tagged.substr(begins ? BeginMarker.size() : FailedMarker.size()));
        auto const asked = wanted.find(address);
        if (asked == wanted.end())
            continue;
        DecompiledFunction& function = found[asked->second];
        function.Address = asked->second;
        if (begins)
        {
            function.Name = rest;
            function.Error.clear();
            current = &function;
        }
        else
            function.Error = rest.empty() ? std::string("Ghidra could not decompile it") : std::string(rest);
    }
    if (!sawProgram)
    {
        error = fmt::format("Ghidra's analyzer ran without the decompile script printing anything: {}", LastProblems(lines));
        return {};
    }
    if (found.empty() && !otherProgram.empty())
    {
        error = fmt::format("the Ghidra project holds another build of the client, SHA-256 {}, not this one, {}", otherProgram, sha256);
        return {};
    }
    std::vector<DecompiledFunction> results;
    for (uint64 const address : functions)
    {
        DecompiledFunction function = found.contains(address) ? found[address] : DecompiledFunction{ address, {}, {}, "Ghidra printed nothing for it" };
        if (function.Ok() && function.Code.empty())
            function.Error = "Ghidra's output for it ended before its code did";
        results.push_back(std::move(function));
    }
    return results;
}

std::optional<DecompiledFunction> GhidraDecompiler::ReadCache(uint64 address) const
{
    if (_settings.CacheFolder.empty())
        return std::nullopt;
    std::ifstream stream(_settings.CacheFolder / fmt::format("{:x}.c", address), std::ios::binary);
    if (!stream)
        return std::nullopt;
    DecompiledFunction function;
    function.Address = address;
    if (!std::getline(stream, function.Name) || function.Name.empty())
        return std::nullopt;
    std::ostringstream code;
    code << stream.rdbuf();
    function.Code = code.str();
    if (function.Code.empty())
        return std::nullopt;
    return function;
}

void GhidraDecompiler::WriteCache(DecompiledFunction const& function) const
{
    if (_settings.CacheFolder.empty())
        return;
    std::error_code created;
    std::filesystem::create_directories(_settings.CacheFolder, created);
    std::filesystem::path const file = _settings.CacheFolder / fmt::format("{:x}.c", function.Address);
    std::filesystem::path const partial = _settings.CacheFolder / fmt::format("{:x}.c.partial", function.Address);
    {
        std::ofstream stream(partial, std::ios::binary | std::ios::trunc);
        stream << function.Name << '\n' << function.Code;
        if (!stream)
            return;
    }
    std::error_code renamed;
    std::filesystem::rename(partial, file, renamed);
}
