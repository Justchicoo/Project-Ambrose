/*
 * Project Ambrose by Imjustchico
 * Holds the published providers behind a shared mutex and calls them outside it, so a slow or throwing provider never blocks another publisher and never takes the registry down with it.
 */

#include "StatsRegistry.h"

#include <exception>
#include <mutex>

namespace Ambrose
{
    StatsRegistry& StatsRegistry::Instance()
    {
        static StatsRegistry instance;
        return instance;
    }

    void StatsRegistry::Publish(std::string name, Provider provider)
    {
        std::unique_lock const lock(_mutex);
        _providers[std::move(name)] = std::move(provider);
    }

    void StatsRegistry::Unpublish(std::string const& name)
    {
        std::unique_lock const lock(_mutex);
        _providers.erase(name);
    }

    bool StatsRegistry::Has(std::string const& name) const
    {
        std::shared_lock const lock(_mutex);
        return _providers.contains(name);
    }

    std::optional<StatValue> StatsRegistry::Get(std::string const& name) const
    {
        Provider provider;
        {
            std::shared_lock const lock(_mutex);
            auto const found = _providers.find(name);
            if (found == _providers.end())
                return std::nullopt;
            provider = found->second;
        }
        try
        {
            return provider();
        }
        catch (std::exception const&)
        {
            return std::nullopt;
        }
    }

    std::vector<std::string> StatsRegistry::Names() const
    {
        std::shared_lock const lock(_mutex);
        std::vector<std::string> names;
        names.reserve(_providers.size());
        for (auto const& [name, provider] : _providers)
            names.push_back(name);
        return names;
    }

    std::vector<std::pair<std::string, StatValue>> StatsRegistry::Collect() const
    {
        std::vector<std::pair<std::string, Provider>> providers;
        {
            std::shared_lock const lock(_mutex);
            providers.assign(_providers.begin(), _providers.end());
        }
        std::vector<std::pair<std::string, StatValue>> values;
        values.reserve(providers.size());
        for (auto const& [name, provider] : providers)
        {
            try
            {
                values.emplace_back(name, provider());
            }
            catch (std::exception const&)
            {
            }
        }
        return values;
    }

    void StatsRegistry::Clear()
    {
        std::unique_lock const lock(_mutex);
        _providers.clear();
    }
}
