/*
 * Project Ambrose by Imjustchico
 * How the supervisor launches one app: the build it runs, command-line overrides that become the settled command-line override layer, environment variables with secret ones named, the working folder, start and stop timeouts, the restart policy with its backoff and crash loop limits, autostart with the supervisor, process priority and opt-in hard limits on memory and CPU, each unset field inheriting the app definition the config file gave, with the validation that refuses a missing build, a working folder outside the install and values outside their bounds, and the preview of the exact command line and environment with secret values masked.
 */

#ifndef AMBROSE_LAUNCHSETTINGS_H
#define AMBROSE_LAUNCHSETTINGS_H

#include "Types.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

enum class LaunchRestartPolicy : uint8
{
    Always,
    OnCrash,
    Never
};

struct LaunchSettings
{
    static constexpr uint32 MinTimeoutSeconds = 1;
    static constexpr uint32 MaxTimeoutSeconds = 3600;
    static constexpr uint32 MinBackoffSeconds = 1;
    static constexpr uint32 MaxBackoffSeconds = 3600;
    static constexpr uint32 MaxRestartsLimit = 100;
    static constexpr uint32 MaxCrashWindowSeconds = 86400;
    static constexpr int MinPriority = -2;
    static constexpr int MaxPriority = 2;
    static constexpr uint64 MinMemoryLimitBytes = 16ull * 1024ull * 1024ull;
    static constexpr uint32 MinCpuLimitPercent = 1;
    static constexpr uint32 MaxCpuLimitPercent = 100;
    static constexpr std::string_view SecretMask = "********";

    std::string Build;
    std::string WorkingDirectory;
    std::optional<uint32> StartTimeoutSeconds;
    std::optional<uint32> StopTimeoutSeconds;
    std::optional<LaunchRestartPolicy> RestartPolicy;
    std::optional<uint32> BackoffSeconds;
    std::optional<uint32> MaxRestarts;
    std::optional<uint32> CrashWindowSeconds;
    std::optional<bool> Autostart;
    std::optional<int> Priority;
    std::optional<uint64> MemoryLimitBytes;
    std::optional<uint32> CpuLimitPercent;
    std::map<std::string, std::string, std::less<>> Overrides;
    std::map<std::string, std::string, std::less<>> Environment;
    std::set<std::string, std::less<>> SecretEnvironment;

    static std::string_view PolicyName(LaunchRestartPolicy policy) noexcept;
    static std::optional<LaunchRestartPolicy> ParsePolicy(std::string_view text) noexcept;
    static bool IsValidEnvironmentName(std::string_view name) noexcept;
    static bool IsValidOverrideKey(std::string_view key) noexcept;

    std::vector<std::string> Validate(std::filesystem::path const& installFolder) const;
    std::vector<std::string> OverrideArguments() const;
    std::string PreviewCommandLine(std::filesystem::path const& program, std::vector<std::string> const& baseArguments) const;
    std::vector<std::pair<std::string, std::string>> PreviewEnvironment() const;
};

#endif
