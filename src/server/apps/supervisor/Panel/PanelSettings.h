/*
 * Project Ambrose by Imjustchico
 * The typed panel settings kept in the supervisor store: defaults and bounds are declared here, secret values are never serialized, listener-owned values report their lock layer, and writes are validated before the panel records them.
 */

#ifndef AMBROSE_PANELSETTINGS_H
#define AMBROSE_PANELSETTINGS_H

#include "Types.h"

#include <nlohmann/json_fwd.hpp>

#include <string>
#include <string_view>
#include <vector>

class PanelStore;

class PanelSettings
{
public:
    explicit PanelSettings(PanelStore& store) : _store(store) {}

    nlohmann::json Answer(std::string_view group, std::string& error) const;
    bool Update(nlohmann::json const& values, int64 userId, std::string& error);
    bool TestMail(std::string_view address, std::string& error) const;

private:
    PanelStore& _store;
};

#endif
