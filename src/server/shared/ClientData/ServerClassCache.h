/*
 * Project Ambrose by Imjustchico
 * The class files Ambrose builds itself, one per client revision, in the Ambrose data folder's classes folder: a file holds the classes the revision's archives hold that its type dump does not describe and every object of which decodes cleanly, in the type dump's own format with the evidence for each, and Ensure returns it, running schemaprobe over every archive of the install as a child process when it is missing, older than the type dump or written by an earlier way of finding the classes, which ExtractionVersion numbers, while holding the build lock on a lock file beside it, so processes started together build it once and the others wait for it, writing to a file beside it that replaces the old one only once it is whole; Read loads a class file's classes with the evidence each carries.
 */

#ifndef AMBROSE_SERVERCLASSCACHE_H
#define AMBROSE_SERVERCLASSCACHE_H

#include "ChildProcess.h"
#include "ClientLocator.h"
#include "TypeDumpLoader.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

struct ServerClassCacheOptions
{
    std::filesystem::path DataFolder;
    std::filesystem::path Program;
    std::chrono::seconds Timeout{ 3600 };
    std::chrono::milliseconds LockPollInterval{ 500 };
    std::function<ChildProcessResult(ChildProcessOptions const&)> Run;
    std::function<void(std::string const& line)> Report;
    std::function<bool()> ShouldStop;
};

class ServerClassCache
{
public:
    static constexpr std::string_view FolderName = "classes";
    static constexpr std::string_view ExtractionKey = "extraction";
    static constexpr uint32 ExtractionVersion = 2;

    ServerClassCache() = delete;

    static std::optional<std::filesystem::path> PathFor(std::filesystem::path const& dataFolder, std::string_view revision);
    static bool IsCurrent(std::filesystem::path const& classes, std::filesystem::path const& typeDump);
    static std::optional<uint32> ReadExtractionVersion(std::filesystem::path const& classes);
    static std::optional<std::filesystem::path> Ensure(ClientInstall const& install, std::filesystem::path const& typeDump, ServerClassCacheOptions const& options, std::string& error);
    static std::filesystem::path DefaultProgram(std::filesystem::path const& executableDirectory);
    static bool Read(std::filesystem::path const& classes, TypeDumpLoader::RawDump& dump, std::string& error);
};

#endif
