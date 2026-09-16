/*
 * Project Ambrose by Imjustchico
 * What a typed view declares, a class name and fields given as position, C++ storage type, dump type and property name with the property hash computed at compile time, and what a type catalog records when it binds one: the class and each field's property ordinal.
 */

#ifndef AMBROSE_VIEWDEFINITION_H
#define AMBROSE_VIEWDEFINITION_H

#include "PropertyValue.h"
#include "StringHash.h"

#include <cstddef>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

struct ClassInfo;

struct ViewField
{
    std::size_t Index = 0;
    std::string_view TypeName;
    std::string_view Name;
    uint32 Hash = 0;
    std::size_t StorageIndex = 0;

    template<typename T>
    static constexpr ViewField Of(std::size_t index, std::string_view typeName, std::string_view name) noexcept
    {
        static_assert(PropertyValue::IndexOf<T>() != std::variant_npos, "a view field must name one of PropertyValue's storage types");
        return ViewField{ index, typeName, name, StringHash::PropertyHash(typeName, name), PropertyValue::IndexOf<T>() };
    }
};

struct ViewDefinition
{
    std::string_view Name;
    std::string_view ClassName;
    std::span<ViewField const> Fields;
};

struct ViewBinding
{
    ViewDefinition const* Definition = nullptr;
    ClassInfo const* Class = nullptr;
    std::vector<uint32> Ordinals;
};

#endif
