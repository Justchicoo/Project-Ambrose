/*
 * Project Ambrose by Imjustchico
 * The register of everything that can be rebuilt without stopping the server: each target says how to build itself again and which targets it must follow, and a reload runs them in that order, keeps what was already serving when a build fails, and reports every error it found rather than the first. A target that has never succeeded is still registered, because an operator needs to be told a thing exists and is broken rather than that it is missing.
 */

#ifndef AMBROSE_RELOADMGR_H
#define AMBROSE_RELOADMGR_H

#include "Types.h"

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct ReloadOutcome
{
    std::string Target;
    bool Ok = false;
    uint64 Generation = 0;
    std::vector<std::string> Errors;
};

class ReloadMgr
{
public:
    using Loader = std::function<bool(std::vector<std::string>&)>;

    static ReloadMgr& Instance();

    ReloadMgr(ReloadMgr const&) = delete;
    ReloadMgr& operator=(ReloadMgr const&) = delete;

    bool Register(std::string name, Loader loader, std::vector<std::string> dependsOn = {});
    bool Unregister(std::string_view name);
    bool IsRegistered(std::string_view name) const;
    void Clear();

    std::vector<std::string> GetTargets() const;
    std::vector<std::string> GetOrderedTargets() const;
    std::optional<ReloadOutcome> GetLastOutcome(std::string_view name) const;
    uint64 GetGeneration(std::string_view name) const;

    ReloadOutcome Reload(std::string_view name);
    std::vector<ReloadOutcome> ReloadAll();

    static std::vector<std::string> Describe(ReloadOutcome const& outcome);
    std::vector<std::string> DescribeTargets() const;

private:
    ReloadMgr() = default;

    struct Target
    {
        std::string Name;
        Loader Load;
        std::vector<std::string> DependsOn;
        uint64 Generation = 0;
        bool Ran = false;
        ReloadOutcome Last;
    };

    ReloadOutcome RunLocked(Target& target);
    std::vector<std::string> OrderLocked() const;

    mutable std::mutex _mutex;
    std::unordered_map<std::string, Target> _targets;
};

#define sReloadMgr ReloadMgr::Instance()

#endif
