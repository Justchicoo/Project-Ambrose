/*
 * Project Ambrose by Imjustchico
 * Builds the versioned, privacy-first report file from selected error groups, adding rendered log text and its preceding context only after an operator explicitly includes them.
 */

#ifndef AMBROSE_PANELERRORREPORT_H
#define AMBROSE_PANELERRORREPORT_H

#include "PanelErrors.h"

#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string>
#include <vector>

namespace PanelErrorReport
{
    std::optional<nlohmann::json> Build(std::vector<PanelErrorGroup> const& groups, bool includeRendered, std::string& error);
}

#endif
