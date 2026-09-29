/*
 * Project Ambrose by Imjustchico
 * Keeps one type dump per client revision in the Ambrose data folder: names types/<revision>.json only for a plain revision and an absolute folder, reads the revision, executable SHA-256 and extractor from a dump's first bytes whatever order its root keys take, hashes the client program as a stream, and builds a missing or stale dump, reported as the step a starting server is on for the extractor's time, by running the type extractor, resolved against the working directory, and on Windows given .exe when only that file exists, to the one absolute path that is checked and run, on the install's absolute path with an input that ends when this process does, forwarding its output lines, while holding the build lock on a lock file beside the dump; other callers wait for the dump to become current or the lock to be released, however long the holder runs and whatever the file holds. A lock file that still cannot be opened after two seconds of tries fails naming why, a file system that cannot lock builds unlocked with a warning, a run with no exit code that still leaves a current dump is used with a warning, and every failure names its cause, including why no exit code was read.
 */

#include "TypeDumpCache.h"

#include "BuildLock.h"
#include "TypeDumpLoader.h"
#include "TypeRegistryBinary.h"
#include "ConfigMgr.h"
#include "SHA256.h"
#include "StartProgress.h"
#include "StringUtil.h"
#include "Types.h"
#include "Utf.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <deque>
#include <fstream>
#include <mutex>
#include <span>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace
{
    using Json = nlohmann::json;
    using SteadyClock = std::chrono::steady_clock;

    constexpr std::size_t ReadChunkBytes = 1024 * 1024;
    constexpr std::size_t MaxErrorLines = 5;
    constexpr std::size_t MaxQuotedLineBytes = 512;
    constexpr std::chrono::milliseconds MinimumPollInterval{ 1 };
    constexpr std::chrono::seconds LockFailureGrace{ 2 };
    constexpr int MaxLockFailures = 3;

    std::string PathText(std::filesystem::path const& path)
    {
        return ClientLocator::PathText(path);
    }

    std::optional<std::string> ArgumentText(std::filesystem::path const& path)
    {
#ifdef _WIN32
        std::wstring const& native = path.native();
        return Utf::Utf16ToUtf8(std::u16string_view(reinterpret_cast<char16_t const*>(native.data()), native.size()), Utf::InvalidPolicy::Reject);
#else
        return ConfigMgr::PathToUtf8(path);
#endif
    }

    bool IsPlainRevision(std::string_view revision)
    {
        if (revision.empty() || revision == "." || revision == "..")
            return false;
        return std::all_of(revision.begin(), revision.end(), [](char c)
        {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        });
    }

    class HeaderReader final : public nlohmann::json_sax<Json>
    {
    public:
        bool null() override
        {
            return Value();
        }

        bool boolean(bool) override
        {
            return Value();
        }

        bool number_integer(number_integer_t) override
        {
            return Value();
        }

        bool number_unsigned(number_unsigned_t) override
        {
            return Value();
        }

        bool number_float(number_float_t, string_t const&) override
        {
            return Value();
        }

        bool string(string_t& value) override
        {
            if (_depth != 1)
                return Value();
            if (_key == "revision")
                _header.Revision = value;
            else if (_key == "executable_sha256")
                _header.ExecutableSha256 = value;
            else if (_key == "extractor")
                _header.Extractor = value;
            return !Complete();
        }

        bool binary(binary_t&) override
        {
            return Value();
        }

        bool start_object(std::size_t) override
        {
            if (_depth == 0)
            {
                if (_started)
                    return false;
                _started = true;
            }
            ++_depth;
            return true;
        }

        bool key(string_t& name) override
        {
            if (_depth == 1)
                _key = name;
            return true;
        }

        bool end_object() override
        {
            --_depth;
            if (_depth == 0)
                _ended = true;
            return _depth != 0;
        }

        bool start_array(std::size_t) override
        {
            if (_depth == 0)
                return false;
            ++_depth;
            return true;
        }

        bool end_array() override
        {
            --_depth;
            return true;
        }

        bool parse_error(std::size_t position, std::string const&, nlohmann::detail::exception const&) override
        {
            _parseError = true;
            _errorPosition = position;
            return false;
        }

        std::optional<TypeDumpHeader> Result(std::size_t inputBytes, bool truncated) const
        {
            if (!_started)
                return std::nullopt;
            if (_ended)
                return _header;
            if (_parseError && !Complete() && !(truncated && _errorPosition >= inputBytes))
                return std::nullopt;
            return _header;
        }

    private:
        bool Value() const noexcept
        {
            return _depth != 0;
        }

        bool Complete() const noexcept
        {
            return !_header.Revision.empty() && !_header.ExecutableSha256.empty() && !_header.Extractor.empty();
        }

        TypeDumpHeader _header;
        std::string _key;
        std::size_t _depth = 0;
        std::size_t _errorPosition = 0;
        bool _started = false;
        bool _ended = false;
        bool _parseError = false;
    };

    std::string WhyNotCurrent(ClientInstall const& install, std::filesystem::path const& dump, std::string_view executableSha256)
    {
        std::error_code status;
        if (!std::filesystem::is_regular_file(dump, status))
            return "it was not written";
        std::optional<TypeDumpHeader> const header = TypeDumpCache::ReadHeader(dump);
        if (!header)
            return "it does not start as a JSON object";
        if (header->Revision != install.Revision)
            return header->Revision.empty() ? std::string("it records no revision") : fmt::format("it records the revision {}", Ambrose::ForLog(header->Revision));
        return fmt::format("it records the client program SHA-256 {} instead of {}", header->ExecutableSha256.empty() ? std::string("(none)") : Ambrose::ForLog(header->ExecutableSha256), executableSha256);
    }

    constexpr std::chrono::seconds ReportedMargin{ 60 };

    void Pause(TypeDumpCacheOptions const& options)
    {
        std::this_thread::sleep_for(std::max(options.LockPollInterval, MinimumPollInterval));
    }

    std::optional<std::filesystem::path> Build(ClientInstall const& install, std::filesystem::path const& dump, std::string const& executableSha256, std::filesystem::path const& extractorPath, TypeDumpCacheOptions const& options, std::string& error)
    {
        if (TypeDumpCache::IsCurrent(install, dump, executableSha256))
            return dump;
        std::mutex linesMutex;
        std::deque<std::string> errorLines;
        auto const report = [&options](bool warning, std::string const& text)
        {
            if (options.Report)
                options.Report(warning, text);
        };
        std::string const extractor = PathText(extractorPath);
        report(false, fmt::format("Building the type dump for revision {} from {} into {} with {}; this can take a minute or two", install.Revision, PathText(install.Root), PathText(dump), extractor));

        std::error_code absoluteError;
        std::filesystem::path root = std::filesystem::absolute(install.Root, absoluteError);
        if (absoluteError)
            root = install.Root;
        std::optional<std::string> const rootText = ArgumentText(root);
        std::optional<std::string> const dumpText = ArgumentText(dump);
        if (!rootText || !dumpText)
        {
            error = fmt::format("the path {} cannot be passed to the type extractor because it is not valid Unicode", PathText(rootText ? dump : root));
            return std::nullopt;
        }
        ChildProcessOptions child;
        child.Program = extractorPath;
        child.Arguments = { "--client", *rootText, "--out", *dumpText, "--quiet", "--exit-when-input-ends" };
        child.InputEndsWithParent = true;
        child.Timeout = std::chrono::duration_cast<std::chrono::milliseconds>(options.Timeout);
        child.OnLine = [&](std::string_view line, bool isError)
        {
            std::lock_guard<std::mutex> const lock(linesMutex);
            if (isError)
            {
                errorLines.push_back(Ambrose::ForLog(line, MaxQuotedLineBytes));
                if (errorLines.size() > MaxErrorLines)
                    errorLines.pop_front();
            }
            report(isError, std::string(line));
        };
        child.ShouldStop = options.ShouldStop;

        StartProgress::Report(fmt::format("building the type dump for revision {}", install.Revision),
            std::chrono::duration_cast<std::chrono::seconds>(options.Timeout) + ReportedMargin);
        SteadyClock::time_point const started = SteadyClock::now();
        ChildProcessResult const run = options.Run ? options.Run(child) : ChildProcess::Run(child);
        std::string const revision = install.Revision;
        if (!run.Started)
        {
            error = fmt::format("the type extractor {} could not be started to build the type dump for revision {}: {}", extractor, revision, run.Error.empty() ? std::string("no reason was given") : run.Error);
            return std::nullopt;
        }
        if (run.TimedOut)
        {
            error = fmt::format("the type extractor {} did not finish the type dump for revision {} within {} seconds and was ended", extractor, revision, options.Timeout.count());
            return std::nullopt;
        }
        if (run.Stopped)
        {
            error = fmt::format("building the type dump for revision {} was stopped", revision);
            return std::nullopt;
        }
        if (run.ExitCode != 0)
        {
            std::string printed;
            {
                std::lock_guard<std::mutex> const lock(linesMutex);
                for (std::string const& line : errorLines)
                    printed += (printed.empty() ? "" : "; ") + line;
            }
            if (printed.empty())
                printed = "it printed no error";
            if (run.ExitCode)
            {
                error = fmt::format("the type extractor {} failed to build the type dump for revision {} with exit code {}: {}", extractor, revision, *run.ExitCode, printed);
                return std::nullopt;
            }
            std::string const cause = run.Error.empty() ? std::string("its exit code could not be read") : run.Error;
            if (!TypeDumpCache::IsCurrent(install, dump, executableSha256))
            {
                error = fmt::format("the type extractor {} failed to build the type dump for revision {} with no exit code ({}): {}", extractor, revision, cause, printed);
                return std::nullopt;
            }
            report(true, fmt::format("The type extractor {} reported no exit code ({}), but the type dump {} it left is current for revision {}, so it is used", extractor, cause, PathText(dump), revision));
        }
        if (!TypeDumpCache::IsCurrent(install, dump, executableSha256))
        {
            error = fmt::format("the type extractor {} finished, but {} is still not the type dump for revision {} of {}: {}", extractor, PathText(dump), revision, PathText(install.Root), WhyNotCurrent(install, dump, executableSha256));
            return std::nullopt;
        }
        report(false, fmt::format("Built the type dump {} in {} seconds", PathText(dump), std::chrono::duration_cast<std::chrono::seconds>(SteadyClock::now() - started).count()));
        return dump;
    }
}

std::optional<std::filesystem::path> TypeDumpCache::PathFor(std::filesystem::path const& dataFolder, std::string_view revision)
{
    if (!IsPlainRevision(revision) || dataFolder.empty() || !dataFolder.is_absolute())
        return std::nullopt;
    return dataFolder / std::filesystem::path(FolderName) / ConfigMgr::PathFromUtf8(std::string(revision) + ".json");
}

std::optional<TypeDumpHeader> TypeDumpCache::ReadHeader(std::filesystem::path const& dump)
{
    std::error_code status;
    if (!std::filesystem::is_regular_file(dump, status))
        return std::nullopt;
    std::ifstream stream(dump, std::ios::binary);
    if (!stream)
        return std::nullopt;
    std::string text(HeaderBytes + 1, '\0');
    stream.read(text.data(), static_cast<std::streamsize>(text.size()));
    if (stream.bad())
        return std::nullopt;
    std::size_t const read = static_cast<std::size_t>(stream.gcount());
    bool const truncated = read > HeaderBytes;
    text.resize(std::min(read, HeaderBytes));
    HeaderReader reader;
    Json::sax_parse(text.begin(), text.end(), &reader);
    return reader.Result(text.size(), truncated);
}

std::optional<std::string> TypeDumpCache::ExecutableSha256(ClientInstall const& install, std::string& error)
{
    std::filesystem::path const program = install.Root / ConfigMgr::PathFromUtf8(ExecutableRelativePath);
    std::error_code status;
    if (!std::filesystem::is_regular_file(program, status))
    {
        error = fmt::format("there is no {} in the install {}", ExecutableRelativePath, PathText(install.Root));
        return std::nullopt;
    }
    std::ifstream stream(program, std::ios::binary);
    if (!stream)
    {
        error = fmt::format("cannot open the client program {}", PathText(program));
        return std::nullopt;
    }
    SHA256 hash;
    std::vector<char> buffer(ReadChunkBytes);
    while (stream)
    {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        std::streamsize const read = stream.gcount();
        if (read > 0)
            hash.Update(std::span<uint8 const>(reinterpret_cast<uint8 const*>(buffer.data()), static_cast<std::size_t>(read)));
    }
    if (stream.bad())
    {
        error = fmt::format("cannot read the client program {}", PathText(program));
        return std::nullopt;
    }
    std::string hex;
    hex.reserve(SHA256::DigestLength * 2);
    for (uint8 const byte : hash.Finalize())
        hex += fmt::format("{:02x}", byte);
    return hex;
}

bool TypeDumpCache::IsCurrent(ClientInstall const& install, std::filesystem::path const& dump, std::string_view executableSha256)
{
    if (install.Revision.empty() || executableSha256.empty())
        return false;
    std::optional<TypeDumpHeader> const header = ReadHeader(dump);
    return header && header->Revision == install.Revision && Ambrose::EqualsIgnoreCase(header->ExecutableSha256, executableSha256);
}

std::optional<std::filesystem::path> TypeDumpCache::Ensure(ClientInstall const& install, TypeDumpCacheOptions const& options, std::string& error)
{
    auto const report = [&options](bool warning, std::string const& text)
    {
        if (options.Report)
            options.Report(warning, text);
    };
    auto const stopRequested = [&options] { return options.ShouldStop && options.ShouldStop(); };

    if (options.DataFolder.empty())
    {
        error = "the Ambrose data folder cannot be found on this machine, so there is nowhere to keep the type dump";
        return std::nullopt;
    }
    if (!options.DataFolder.is_absolute())
    {
        error = fmt::format("the Ambrose data folder {} is not an absolute path, so the type dump cannot be kept there", PathText(options.DataFolder));
        return std::nullopt;
    }
    if (install.Revision.empty())
    {
        error = fmt::format("the install {} has no readable Bin/revision.dat, so its type dump cannot be named", PathText(install.Root));
        return std::nullopt;
    }
    std::optional<std::filesystem::path> const dump = PathFor(options.DataFolder, install.Revision);
    if (!dump)
    {
        error = fmt::format("the install {} names the revision {}, which cannot name a type dump file: only letters, digits, '.', '_' and '-' are allowed", PathText(install.Root), Ambrose::ForLog(install.Revision));
        return std::nullopt;
    }
    std::string hashError;
    std::optional<std::string> const executableSha256 = ExecutableSha256(install, hashError);
    if (!executableSha256)
    {
        error = fmt::format("the type dump for revision {} cannot be checked: {}", install.Revision, hashError);
        return std::nullopt;
    }
    if (IsCurrent(install, *dump, *executableSha256))
        return dump;

    if (options.Extractor.empty())
    {
        error = fmt::format("the type dump for revision {} needs building, but the type extractor (none named) was not found", install.Revision);
        return std::nullopt;
    }
    std::filesystem::path extractor = options.Extractor;
    if (!extractor.is_absolute())
    {
        std::error_code absoluteError;
        extractor = std::filesystem::absolute(options.Extractor, absoluteError);
        if (absoluteError)
        {
            error = fmt::format("the type dump for revision {} needs building, but the type extractor {} cannot be found from the working directory: {}", install.Revision, PathText(options.Extractor), absoluteError.message());
            return std::nullopt;
        }
    }
    std::error_code status;
#ifdef _WIN32
    if (!extractor.has_extension() && !std::filesystem::is_regular_file(extractor, status))
    {
        std::filesystem::path withExtension = extractor;
        withExtension += L".exe";
        if (std::filesystem::is_regular_file(withExtension, status))
            extractor = std::move(withExtension);
    }
#endif
    if (!std::filesystem::is_regular_file(extractor, status))
    {
        error = fmt::format("the type dump for revision {} needs building, but the type extractor {} was not found", install.Revision, PathText(extractor));
        return std::nullopt;
    }
    std::error_code created;
    std::filesystem::create_directories(dump->parent_path(), created);
    std::error_code folder;
    if (created || !std::filesystem::is_directory(dump->parent_path(), folder))
    {
        error = fmt::format("cannot create the type dump folder {}: {}", PathText(dump->parent_path()), created ? created.message() : std::string("a file of that name is in the way"));
        return std::nullopt;
    }

    std::filesystem::path lockPath = *dump;
    lockPath += ".lock";
    BuildLock lock;
    bool waitReported = false;
    int failures = 0;
    SteadyClock::time_point failingSince;
    while (true)
    {
        if (stopRequested())
        {
            error = fmt::format("building the type dump for revision {} was stopped", install.Revision);
            return std::nullopt;
        }
        std::string lockError;
        BuildLockOutcome const outcome = lock.Take(lockPath, lockError);
        if (outcome == BuildLockOutcome::Taken)
            return Build(install, *dump, *executableSha256, extractor, options, error);
        if (outcome == BuildLockOutcome::Unsupported)
        {
            report(true, fmt::format("The lock file {} cannot be locked on its file system ({}), so the type dump for revision {} is built without keeping other processes from building it at the same time", PathText(lockPath), lockError, install.Revision));
            return Build(install, *dump, *executableSha256, extractor, options, error);
        }
        if (IsCurrent(install, *dump, *executableSha256))
            return dump;
        if (outcome == BuildLockOutcome::Failed)
        {
            SteadyClock::time_point const now = SteadyClock::now();
            if (failures++ == 0)
                failingSince = now;
            if (failures >= MaxLockFailures && now - failingSince >= LockFailureGrace)
            {
                error = fmt::format("cannot open the lock file {} to build the type dump for revision {}: {}", PathText(lockPath), install.Revision, lockError);
                return std::nullopt;
            }
        }
        else
        {
            failures = 0;
            if (!waitReported)
            {
                report(false, fmt::format("Waiting for another process to build the type dump for revision {} (lock file {})", install.Revision, PathText(lockPath)));
                StartProgress::Report(fmt::format("waiting for another process to build the type dump for revision {}", install.Revision),
                    std::chrono::duration_cast<std::chrono::seconds>(options.Timeout) + ReportedMargin);
                waitReported = true;
            }
        }
        Pause(options);
    }
}

std::filesystem::path TypeDumpCache::DefaultExtractor(std::filesystem::path const& executableDirectory)
{
#ifdef _WIN32
    std::filesystem::path const native = executableDirectory / "typeextract.exe";
    std::filesystem::path const other = executableDirectory / "typeextract";
#else
    std::filesystem::path const native = executableDirectory / "typeextract";
    std::filesystem::path const other = executableDirectory / "typeextract.exe";
#endif
    std::error_code error;
    if (!std::filesystem::is_regular_file(native, error) && std::filesystem::is_regular_file(other, error))
        return other;
    return native;
}

std::filesystem::path TypeDumpCache::FastCopyOf(std::filesystem::path const& dump)
{
    std::filesystem::path binary = dump;
    binary.replace_extension(".bin");
    return binary;
}

bool TypeDumpCache::EnsureFastCopy(std::filesystem::path const& dump, std::string& error)
{
    std::filesystem::path const binary = FastCopyOf(dump);
    std::error_code code;
    if (std::filesystem::exists(binary, code))
    {
        std::filesystem::file_time_type const built = std::filesystem::last_write_time(binary, code);
        std::filesystem::file_time_type const source = std::filesystem::last_write_time(dump, code);
        if (!code && built >= source)
            return true;
    }
    if (!std::filesystem::exists(dump, code))
    {
        error = "there is no type dump to build a fast copy of";
        return false;
    }

    std::ifstream stream(dump, std::ios::binary);
    if (!stream)
    {
        error = "the type dump cannot be opened";
        return false;
    }
    std::string const text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad())
    {
        error = "the type dump cannot be read";
        return false;
    }

    TypeDumpLoader::RawDump raw;
    std::vector<std::string> errors;
    if (!TypeDumpLoader::Parse(text, raw, errors))
    {
        error = errors.empty() ? std::string("the type dump is not valid") : errors.front();
        return false;
    }
    return TypeRegistryBinary::Write(binary, raw, dump.stem().string(), error);
}
