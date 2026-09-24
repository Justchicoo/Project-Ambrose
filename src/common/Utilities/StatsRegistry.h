/*
 * Project Ambrose by Imjustchico
 * The registry subsystems publish named live values into, each behind a provider called at read time, so a status page can list every number the process knows without the admin layer knowing every subsystem.
 */

#ifndef AMBROSE_STATSREGISTRY_H
#define AMBROSE_STATSREGISTRY_H

#include "Types.h"

#include <functional>
#include <map>
#include <optional>
#include <shared_mutex>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Ambrose
{
    using StatValue = std::variant<int64, double, std::string, bool>;

    class StatsRegistry
    {
    public:
        using Provider = std::function<StatValue()>;

        static StatsRegistry& Instance();

        StatsRegistry() = default;
        StatsRegistry(StatsRegistry const&) = delete;
        StatsRegistry& operator=(StatsRegistry const&) = delete;

        void Publish(std::string name, Provider provider);
        void Unpublish(std::string const& name);
        bool Has(std::string const& name) const;
        std::optional<StatValue> Get(std::string const& name) const;
        std::vector<std::string> Names() const;
        std::vector<std::pair<std::string, StatValue>> Collect() const;
        void Clear();

    private:
        mutable std::shared_mutex _mutex;
        std::map<std::string, Provider> _providers;
    };
}

#define sStats Ambrose::StatsRegistry::Instance()

#endif
