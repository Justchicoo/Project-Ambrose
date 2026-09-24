/*
 * Project Ambrose by Imjustchico
 * The type dumps Ambrose builds itself, one per client revision, in the Ambrose data folder's types folder: a dump is current when the revision and executable SHA-256 recorded in its header match the install, and Ensure returns a current dump, running typeextract as a child process that ends with its parent when it is missing or stale, while an operating system lock on a lock file, released however its holder ends, keeps processes started together from extracting the same revision twice. Beside each dump lives the fast copy every server and tool actually reads, the same data in the binary form that loads in a fraction of the time; EnsureFastCopy writes it once from the JSON, so the cost of parsing seventeen megabytes of text is paid on the first run after an extraction and by nobody afterwards.
 */

#ifndef AMBROSE_TYPEDUMPCACHE_H
#define AMBROSE_TYPEDUMPCACHE_H

#include "ChildProcess.h"
#include "ClientLocator.h"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

struct TypeDumpCacheOptions
{
    std::filesystem::path DataFolder;
    std::filesystem::path Extractor;
    std::chrono::seconds Timeout{ 900 };
    std::chrono::milliseconds LockPollInterval{ 500 };
    std::function<ChildProcessResult(ChildProcessOptions const&)> Run;
    std::function<void(bool warning, std::string const& text)> Report;
    std::function<bool()> ShouldStop;
};

struct TypeDumpHeader
{
    std::string Revision;
    std::string ExecutableSha256;
    std::string Extractor;
};

class TypeDumpCache
{
public:
    static constexpr std::string_view FolderName = "types";
    static constexpr std::size_t HeaderBytes = 4096;
    static constexpr std::string_view ExecutableRelativePath = "Bin/WizardGraphicalClient.exe";

    TypeDumpCache() = delete;

    static std::optional<std::filesystem::path> PathFor(std::filesystem::path const& dataFolder, std::string_view revision);
    static std::optional<TypeDumpHeader> ReadHeader(std::filesystem::path const& dump);
    static std::optional<std::string> ExecutableSha256(ClientInstall const& install, std::string& error);
    static bool IsCurrent(ClientInstall const& install, std::filesystem::path const& dump, std::string_view executableSha256);
    static std::optional<std::filesystem::path> Ensure(ClientInstall const& install, TypeDumpCacheOptions const& options, std::string& error);
    static std::filesystem::path DefaultExtractor(std::filesystem::path const& executableDirectory);
    static std::filesystem::path FastCopyOf(std::filesystem::path const& dump);
    static bool EnsureFastCopy(std::filesystem::path const& dump, std::string& error);
};

#endif
