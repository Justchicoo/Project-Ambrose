/*
 * Project Ambrose by Imjustchico
 * Sets one property after another on an object and keeps the first refusal, naming the class, the property and why, so a builder can chain its fields and check once.
 */

#ifndef AMBROSE_PROPERTYFILLER_H
#define AMBROSE_PROPERTYFILLER_H

#include "PropertyObject.h"

#include <fmt/format.h>

#include <string>
#include <string_view>
#include <utility>

class PropertyFiller
{
public:
    PropertyFiller(PropertyObject& object, std::string& problem) : _object(object), _problem(problem)
    {
    }

    PropertyFiller& Set(std::string_view name, PropertyValue&& value)
    {
        if (!_problem.empty())
            return *this;
        PropertySetResult const result = _object.Set(name, std::move(value));
        if (result != PropertySetResult::Ok)
            _problem = fmt::format("{} property {} refused its value: {}", _object.GetClass().Name, name, PropertyObject::GetResultName(result));
        return *this;
    }

private:
    PropertyObject& _object;
    std::string& _problem;
};

#endif
