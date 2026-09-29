/*
 * Project Ambrose by Imjustchico
 * Names a class file the way the type dump beside it is named, so a revision that cannot name one cannot name the other; a file is current once it is at least as new as the type dump it was probed with and says it was found the way this build finds classes, a file written before the XML object files were read carrying no such number, and building one takes the build lock beside the file, or waits while another process holds it until the file is current or the lock is free, as the type dump cache does, then runs schemaprobe with the install, the type dump and every archive unless the file became current meanwhile, passes each line it writes on to the caller, and moves the finished file into place; reading one takes its classes through the type dump's own parser, which keeps each class's evidence.
 */

#include "ServerClassCache.h"
#include "BuildLock.h"
#include "ConfigMgr.h"
#include "TypeDumpCache.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>
#include <thread>
#include <vector>

namespace
{
    constexpr std::chrono::milliseconds MinimumPollInterval{ 1 };
    constexpr std::chrono::seconds LockFailureGrace{ 2 };
    constexpr int MaxLockFailures = 3;

    std::optional<std::filesystem::path> Build(ClientInstall const& install, std::filesystem::path const& typeDump, std::filesystem::path const& classes, ServerClassCacheOptions const& options,
        std::string& error)
    {
        if (ServerClassCache::IsCurrent(classes, typeDump))
            return classes;
        std::error_code status;
        std::filesystem::path partial = classes;
        partial += ".partial";
        std::filesystem::remove(partial, status);

        ChildProcessOptions child;
        child.Program = options.Program;
        child.Arguments = { "--client", ConfigMgr::PathToUtf8(install.Root), "--type-dump", ConfigMgr::PathToUtf8(typeDump), "--all-wads", "--server-classes", ConfigMgr::PathToUtf8(partial) };
        child.Timeout = std::chrono::duration_cast<std::chrono::milliseconds>(options.Timeout);
        child.InputEndsWithParent = true;
        child.OnLine = [&options](std::string_view line, bool)
        {
            if (options.Report)
                options.Report(std::string(line));
        };
        child.ShouldStop = options.ShouldStop;
        ChildProcessResult const ran = options.Run ? options.Run(child) : ChildProcess::Run(child);
        if (!ran.Succeeded())
        {
            std::filesystem::remove(partial, status);
            std::string why = ran.Error;
            if (ran.TimedOut)
                why = fmt::format("it ran longer than {} seconds", options.Timeout.count());
            else if (ran.Stopped)
                why = "it was stopped";
            else if (ran.Started)
                why = fmt::format("it exited with {}", ran.ExitCode.value_or(-1));
            error = fmt::format("schemaprobe could not build the class file for revision {}: {}", install.Revision, why);
            return std::nullopt;
        }
        std::filesystem::rename(partial, classes, status);
        if (status)
        {
            error = fmt::format("the class file for revision {} was built but cannot be moved into place: {}", install.Revision, status.message());
            return std::nullopt;
        }
        return classes;
    }
}

std::optional<std::filesystem::path> ServerClassCache::PathFor(std::filesystem::path const& dataFolder, std::string_view revision)
{
    std::optional<std::filesystem::path> const dump = TypeDumpCache::PathFor(dataFolder, revision);
    if (!dump)
        return std::nullopt;
    return dataFolder / std::filesystem::path(FolderName) / dump->filename();
}

bool ServerClassCache::IsCurrent(std::filesystem::path const& classes, std::filesystem::path const& typeDump)
{
    std::error_code error;
    if (!std::filesystem::is_regular_file(classes, error))
        return false;
    std::filesystem::file_time_type const built = std::filesystem::last_write_time(classes, error);
    if (error)
        return false;
    std::filesystem::file_time_type const probed = std::filesystem::last_write_time(typeDump, error);
    return !error && built >= probed && ReadExtractionVersion(classes) == ExtractionVersion;
}

std::optional<uint32> ServerClassCache::ReadExtractionVersion(std::filesystem::path const& classes)
{
    std::ifstream stream(classes, std::ios::binary);
    if (!stream)
        return std::nullopt;
    nlohmann::json const file = nlohmann::json::parse(stream, nullptr, false);
    if (!file.is_object())
        return std::nullopt;
    auto const found = file.find(std::string(ExtractionKey));
    if (found == file.end() || !found->is_number_unsigned())
        return std::nullopt;
    return found->get<uint32>();
}

std::optional<std::filesystem::path> ServerClassCache::Ensure(ClientInstall const& install, std::filesystem::path const& typeDump, ServerClassCacheOptions const& options,
    std::string& error)
{
    std::optional<std::filesystem::path> const classes = PathFor(options.DataFolder, install.Revision);
    if (!classes)
    {
        error = fmt::format("the install {} and the data folder {} cannot name a class file for revision {}", ClientLocator::PathText(install.Root),
            ClientLocator::PathText(options.DataFolder), install.Revision);
        return std::nullopt;
    }
    if (IsCurrent(*classes, typeDump))
        return classes;

    std::error_code status;
    if (options.Program.empty() || !std::filesystem::is_regular_file(options.Program, status))
    {
        error = fmt::format("the class file for revision {} needs building, but schemaprobe {} was not found", install.Revision, ClientLocator::PathText(options.Program));
        return std::nullopt;
    }
    std::filesystem::create_directories(classes->parent_path(), status);
    if (!std::filesystem::is_directory(classes->parent_path(), status))
    {
        error = fmt::format("cannot create the class folder {}", ClientLocator::PathText(classes->parent_path()));
        return std::nullopt;
    }

    std::filesystem::path lockPath = *classes;
    lockPath += ".lock";
    BuildLock lock;
    bool waitReported = false;
    int failures = 0;
    std::chrono::steady_clock::time_point failingSince;
    while (true)
    {
        if (options.ShouldStop && options.ShouldStop())
        {
            error = fmt::format("building the class file for revision {} was stopped", install.Revision);
            return std::nullopt;
        }
        std::string lockError;
        BuildLockOutcome const outcome = lock.Take(lockPath, lockError);
        if (outcome == BuildLockOutcome::Taken || outcome == BuildLockOutcome::Unsupported)
        {
            if (outcome == BuildLockOutcome::Unsupported && options.Report)
                options.Report(fmt::format("The lock file {} cannot be locked on its file system ({}), so the class file for revision {} is built without keeping other processes from building it at the same time",
                    ClientLocator::PathText(lockPath), lockError, install.Revision));
            return Build(install, typeDump, *classes, options, error);
        }
        if (IsCurrent(*classes, typeDump))
            return classes;
        if (outcome == BuildLockOutcome::Failed)
        {
            std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
            if (failures++ == 0)
                failingSince = now;
            if (failures >= MaxLockFailures && now - failingSince >= LockFailureGrace)
            {
                error = fmt::format("cannot open the lock file {} to build the class file for revision {}: {}", ClientLocator::PathText(lockPath), install.Revision, lockError);
                return std::nullopt;
            }
        }
        else
        {
            failures = 0;
            if (!waitReported && options.Report)
                options.Report(fmt::format("Waiting for another process to build the class file for revision {} (lock file {})", install.Revision, ClientLocator::PathText(lockPath)));
            waitReported = true;
        }
        std::this_thread::sleep_for(std::max(options.LockPollInterval, MinimumPollInterval));
    }
}

bool ServerClassCache::Read(std::filesystem::path const& classes, TypeDumpLoader::RawDump& dump, std::string& error)
{
    std::ifstream stream(classes, std::ios::binary);
    if (!stream)
    {
        error = fmt::format("the class file {} cannot be opened", ClientLocator::PathText(classes));
        return false;
    }
    std::string const text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    std::vector<std::string> errors;
    if (stream.bad() || !TypeDumpLoader::Parse(text, dump, errors))
    {
        error = fmt::format("the class file {} cannot be read: {}", ClientLocator::PathText(classes), errors.empty() ? std::string("a read failed") : errors.front());
        return false;
    }
    return true;
}

std::filesystem::path ServerClassCache::DefaultProgram(std::filesystem::path const& executableDirectory)
{
#ifdef _WIN32
    std::filesystem::path const native = executableDirectory / "schemaprobe.exe";
    std::filesystem::path const other = executableDirectory / "schemaprobe";
#else
    std::filesystem::path const native = executableDirectory / "schemaprobe";
    std::filesystem::path const other = executableDirectory / "schemaprobe.exe";
#endif
    std::error_code error;
    if (!std::filesystem::is_regular_file(native, error) && std::filesystem::is_regular_file(other, error))
        return other;
    return native;
}
