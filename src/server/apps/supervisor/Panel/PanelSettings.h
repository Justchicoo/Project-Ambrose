/*
 * Project Ambrose by Imjustchico
 * The typed panel settings kept in the supervisor store: defaults and bounds are declared here, secret values are never serialized, listener-owned values report their lock layer, the options the panel enforces itself, such as the two-factor requirement, show the value in force and the layer that set it, and writes are validated before the panel records them.
 */

#ifndef AMBROSE_PANELSETTINGS_H
#define AMBROSE_PANELSETTINGS_H

#include "Types.h"

#include <nlohmann/json_fwd.hpp>

#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class PanelStore;

class PanelSettings
{
public:
    explicit PanelSettings(PanelStore& store) : _store(store) {}

    nlohmann::json Answer(std::string_view group, std::string& error) const;
    bool Update(nlohmann::json const& values, int64 userId, std::string& error);
    void SetOwned(std::string_view key, std::string value, std::string layer);
    std::string ValueOf(std::string_view key) const;

private:
    PanelStore& _store;
    mutable std::mutex _ownedMutex;
    std::map<std::string, std::pair<std::string, std::string>, std::less<>> _owned;
};

#endif
