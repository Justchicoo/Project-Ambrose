/*
 * Project Ambrose by Imjustchico
 * The class files Ambrose builds itself, one per client revision, in the Ambrose data folder's classes folder: a file holds the classes the revision's archives hold that its type dump does not describe and every object of which decodes cleanly, in the type dump's own format with the evidence for each, after the authored classes it was built on, which the sweep reads the install with and the file records by digest, and Ensure returns it, running schemaprobe over every archive of the install as a child process, handing it the authored classes as a supplement, when it is missing, older than the type dump, built on other authored classes or written by an earlier way of finding the classes, which ExtractionVersion numbers, while holding the build lock on a lock file beside it, so processes started together build it once and the others wait for it, writing to a file beside it that replaces the old one only once it is whole; Read loads a class file's classes with the evidence each carries.
 */

#ifndef AMBROSE_SERVERCLASSCACHE_H
#define AMBROSE_SERVERCLASSCACHE_H

#include "ChildProcess.h"
#include "ClientLocator.h"
#include "TypeDumpLoader.h"

#include <nlohmann/json_fwd.hpp>

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct ServerClassCacheOptions
{
    std::filesystem::path DataFolder;
    std::filesystem::path Program;
    std::chrono::seconds Timeout{ 3600 };
    std::chrono::milliseconds LockPollInterval{ 500 };
    std::function<ChildProcessResult(ChildProcessOptions const&)> Run;
    std::function<void(std::string const& line)> Report;
    std::function<bool()> ShouldStop;
    TypeDumpLoader::RawDump Authored;
};

class ServerClassCache
{
public:
    static constexpr std::string_view FolderName = "classes";
    static constexpr std::string_view ExtractionKey = "extraction";
    static constexpr std::string_view BuiltOnKey = "built_on";
    static constexpr std::string_view InstallSource = "install";
    static constexpr std::string_view AuthoredSource = "authored";
    static constexpr uint32 ExtractionVersion = 3;

    ServerClassCache() = delete;

    static std::optional<std::filesystem::path> PathFor(std::filesystem::path const& dataFolder, std::string_view revision);
    static bool IsCurrent(std::filesystem::path const& classes, std::filesystem::path const& typeDump, std::string_view builtOn = {});
    static std::optional<uint32> ReadExtractionVersion(std::filesystem::path const& classes);
    static std::optional<std::string> ReadBuiltOn(std::filesystem::path const& classes);
    static nlohmann::json ClassesJson(std::vector<TypeDumpLoader::RawClass> const& classes);
    static std::string Digest(TypeDumpLoader::RawDump const& classes);
    static bool WriteClasses(std::filesystem::path const& path, TypeDumpLoader::RawDump const& classes, std::string& error);
    static std::optional<std::filesystem::path> Ensure(ClientInstall const& install, std::filesystem::path const& typeDump, ServerClassCacheOptions const& options, std::string& error);
    static std::filesystem::path DefaultProgram(std::filesystem::path const& executableDirectory);
    static bool Read(std::filesystem::path const& classes, TypeDumpLoader::RawDump& dump, std::string& error);
};

#endif
