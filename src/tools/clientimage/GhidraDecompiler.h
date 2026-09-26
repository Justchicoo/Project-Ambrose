/*
 * Project Ambrose by Imjustchico
 * Decompiles functions of the client program through the user's own Ghidra. It starts Ghidra's headless analyzer itself, with the Java that Ghidra's LaunchSupport picks and the virtual machine arguments it asks for, rather than through Ghidra's launch scripts, so no batch file parses a path a second time; it works over a project that holds the program, importing and analyzing the program into a new project the first time, which takes a long while once; it runs a script it writes itself, which prints each program's SHA-256 and each function asked for between markers, so output from a project holding another build of the client is refused; and it keeps each function's C in a cache folder named for the program, so a function is decompiled once.
 */

#ifndef AMBROSE_GHIDRADECOMPILER_H
#define AMBROSE_GHIDRADECOMPILER_H

#include "Types.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct GhidraProject
{
    std::filesystem::path Location;
    std::string Name;

    static std::optional<GhidraProject> FromPath(std::filesystem::path const& path);
    std::filesystem::path File() const;
    bool Exists() const;
};

struct GhidraSettings
{
    std::filesystem::path Install;
    GhidraProject Project;
    bool CreateProject = false;
    std::filesystem::path Program;
    std::string ProgramSha256;
    std::filesystem::path WorkFolder;
    std::filesystem::path CacheFolder;
};

struct DecompiledFunction
{
    uint64 Address = 0;
    std::string Name;
    std::string Code;
    std::string Error;

    bool Ok() const noexcept { return Error.empty(); }
};

class GhidraDecompiler
{
public:
    using Progress = std::function<void(std::string_view line)>;

    static constexpr std::string_view ScriptName = "AmbroseDecompile.py";
    static constexpr std::chrono::minutes DecompileTimeout{ 60 };

    explicit GhidraDecompiler(GhidraSettings settings);

    std::vector<DecompiledFunction> Decompile(std::vector<uint64> const& functions, Progress const& progress, std::string& error);

    static std::optional<std::filesystem::path> FindJava(std::string& error);
    static std::vector<DecompiledFunction> ParseOutput(std::vector<std::string> const& lines, std::vector<uint64> const& functions, std::string_view sha256, std::string& error);
    static std::string_view Script() noexcept;
    static std::string AddressText(uint64 address);

private:
    bool Prepare(std::string& error);
    bool EnsureProject(Progress const& progress, std::string& error);
    bool RunAnalyzer(std::vector<std::string> const& arguments, std::chrono::milliseconds timeout, Progress const& progress, std::vector<std::string>& lines, std::string& error);
    std::optional<DecompiledFunction> ReadCache(uint64 address) const;
    void WriteCache(DecompiledFunction const& function) const;

    GhidraSettings _settings;
    std::filesystem::path _java;
    std::vector<std::string> _vmArguments;
};

#endif
