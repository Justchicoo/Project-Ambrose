/*
 * Project Ambrose by Imjustchico
 * Validates one app's launch settings and previews what they do: the refusal naming a build that does not exist, a working folder outside the install or a value outside its bounds, the --set arguments the command-line overrides become, and the exact command line and environment an operator sees before applying, with every secret environment value masked.
 */

#include "LaunchSettings.h"

#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>

std::string_view LaunchSettings::PolicyName(LaunchRestartPolicy policy) noexcept
{
    switch (policy)
    {
    case LaunchRestartPolicy::Always: return "always";
    case LaunchRestartPolicy::OnCrash: return "on-crash";
    case LaunchRestartPolicy::Never: return "never";
    }
    return "on-crash";
}

std::optional<LaunchRestartPolicy> LaunchSettings::ParsePolicy(std::string_view text) noexcept
{
    if (text == "always")
        return LaunchRestartPolicy::Always;
    if (text == "on-crash")
        return LaunchRestartPolicy::OnCrash;
    if (text == "never")
        return LaunchRestartPolicy::Never;
    return std::nullopt;
}

bool LaunchSettings::IsValidEnvironmentName(std::string_view name) noexcept
{
    if (name.empty() || name.size() > 128)
        return false;
    if (name.front() == '=' || name.find('=') != std::string_view::npos)
        return false;
    return std::all_of(name.begin(), name.end(), [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

bool LaunchSettings::IsValidOverrideKey(std::string_view key) noexcept
{
    if (key.empty() || key.size() > 128)
        return false;
    if (key.front() == '.' || key.back() == '.')
        return false;
    return std::all_of(key.begin(), key.end(), [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_'; });
}

namespace
{
    std::filesystem::path Normalized(std::filesystem::path const& path)
    {
        std::error_code code;
        std::filesystem::path absolute = std::filesystem::absolute(path, code);
        if (code)
            return path.lexically_normal();
        return absolute.lexically_normal();
    }

    bool IsInside(std::filesystem::path const& path, std::filesystem::path const& folder)
    {
        std::filesystem::path const normalPath = Normalized(path);
        std::filesystem::path const normalFolder = Normalized(folder);
        auto [folderEnd, pathEnd] = std::mismatch(normalFolder.begin(), normalFolder.end(), normalPath.begin(), normalPath.end());
        return folderEnd == normalFolder.end() && pathEnd != normalPath.end();
    }

    void CheckRange(std::vector<std::string>& problems, std::string_view name, uint64 value, uint64 minimum, uint64 maximum, std::string_view unit)
    {
        if (value < minimum || value > maximum)
            problems.push_back(fmt::format("{} is {}{}, outside {} to {}{}", name, value, unit, minimum, maximum, unit));
    }
}

std::vector<std::string> LaunchSettings::Validate(std::filesystem::path const& installFolder) const
{
    std::vector<std::string> problems;
    if (!Build.empty())
    {
        std::error_code code;
        if (!std::filesystem::exists(Build, code) || code)
            problems.push_back(fmt::format("the build '{}' does not exist", Build));
    }
    if (!WorkingDirectory.empty() && !IsInside(WorkingDirectory, installFolder))
        problems.push_back(fmt::format("the working folder '{}' is outside the install folder '{}'", WorkingDirectory, installFolder.string()));
    if (StartTimeoutSeconds)
        CheckRange(problems, "the start timeout", *StartTimeoutSeconds, MinTimeoutSeconds, MaxTimeoutSeconds, " s");
    if (StopTimeoutSeconds)
        CheckRange(problems, "the stop timeout", *StopTimeoutSeconds, MinTimeoutSeconds, MaxTimeoutSeconds, " s");
    if (BackoffSeconds)
        CheckRange(problems, "the backoff", *BackoffSeconds, MinBackoffSeconds, MaxBackoffSeconds, " s");
    if (MaxRestarts && *MaxRestarts > MaxRestartsLimit)
        problems.push_back(fmt::format("the crash loop limit is {}, outside 0 to {}", *MaxRestarts, MaxRestartsLimit));
    if (CrashWindowSeconds)
        CheckRange(problems, "the crash window", *CrashWindowSeconds, MinBackoffSeconds, MaxCrashWindowSeconds, " s");
    if (Priority)
        CheckRange(problems, "the process priority", static_cast<uint64>(*Priority + 10), static_cast<uint64>(MinPriority + 10), static_cast<uint64>(MaxPriority + 10), "");
    if (MemoryLimitBytes && *MemoryLimitBytes < MinMemoryLimitBytes)
        problems.push_back(fmt::format("the memory limit is {} bytes, below the {} byte minimum", *MemoryLimitBytes, MinMemoryLimitBytes));
    if (CpuLimitPercent)
        CheckRange(problems, "the CPU limit", *CpuLimitPercent, MinCpuLimitPercent, MaxCpuLimitPercent, "%");
    for (auto const& [key, value] : Overrides)
    {
        if (!IsValidOverrideKey(key))
            problems.push_back(fmt::format("the command-line override '{}' is not a valid setting key", key));
        static_cast<void>(value);
    }
    for (auto const& [name, value] : Environment)
    {
        if (!IsValidEnvironmentName(name))
            problems.push_back(fmt::format("the environment variable '{}' is not a valid name", name));
        static_cast<void>(value);
    }
    for (std::string const& name : SecretEnvironment)
        if (!Environment.contains(name))
            problems.push_back(fmt::format("the secret environment variable '{}' has no value", name));
    return problems;
}

std::vector<std::string> LaunchSettings::OverrideArguments() const
{
    std::vector<std::string> arguments;
    for (auto const& [key, value] : Overrides)
    {
        arguments.push_back("--set");
        arguments.push_back(fmt::format("{}={}", key, value));
    }
    return arguments;
}

std::string LaunchSettings::PreviewCommandLine(std::filesystem::path const& program, std::vector<std::string> const& baseArguments) const
{
    std::string preview = program.string();
    for (std::string const& argument : baseArguments)
    {
        preview += ' ';
        preview += argument;
    }
    for (std::string const& argument : OverrideArguments())
    {
        preview += ' ';
        preview += argument;
    }
    return preview;
}

std::vector<std::pair<std::string, std::string>> LaunchSettings::PreviewEnvironment() const
{
    std::vector<std::pair<std::string, std::string>> preview;
    for (auto const& [name, value] : Environment)
        preview.emplace_back(name, SecretEnvironment.contains(name) ? std::string(SecretMask) : value);
    return preview;
}
