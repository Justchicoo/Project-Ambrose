/*
 * Project Ambrose by Imjustchico
 * Resolves a property's __DEFAULT from the type dump into the value a new object starts with, or explains why the default does not fit the property.
 */

#ifndef AMBROSE_PROPERTYDEFAULTS_H
#define AMBROSE_PROPERTYDEFAULTS_H

#include "PropertyValue.h"
#include "TypeInfo.h"

#include <optional>
#include <string>

namespace PropertyDefaults
{
    std::optional<PropertyValue> Resolve(PropertyInfo const& property, std::string& problem);
}

#endif
