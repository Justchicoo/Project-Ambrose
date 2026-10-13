/*
 * Project Ambrose by Imjustchico
 * Turns a requirement the client's own data holds as a decoded Requirement or RequirementList object into the requirement engine's rows, so an equip slot's rules and a zone object's spawn requirements are read the one same way.
 */

#ifndef AMBROSE_REQUIREMENTOBJECTS_H
#define AMBROSE_REQUIREMENTOBJECTS_H

#include "RequirementMgr.h"
#include "Types.h"

#include <optional>
#include <string>
#include <string_view>

class PropertyObject;

namespace RequirementObjects
{
    constexpr std::string_view ListClass = "class RequirementList";
    constexpr std::string_view UnnamedRequirement = "unnamed requirement";
    constexpr int64 OperatorOr = 1;

    std::string TypeOf(std::string_view className);
    std::string OperatorName(int64 listOperator);
    RequirementRow RowOf(PropertyObject const* requirement);
    std::optional<RequirementTree> TreeOf(PropertyObject const* list);
}

#endif
