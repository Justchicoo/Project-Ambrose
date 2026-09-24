/*
 * Project Ambrose by Imjustchico
 * The message fields that carry ObjectProperty objects: for each, the classes its object may be, whether the blob sits in the 4-byte envelope, and whether it may hold no object.
 */

#ifndef AMBROSE_OBJECTFIELDS_H
#define AMBROSE_OBJECTFIELDS_H

#include <span>
#include <string_view>

struct ObjectField
{
    std::string_view Message;
    std::string_view Field;
    std::span<std::string_view const> Classes;
    bool Enveloped = false;
    bool AllowNull = false;
};

namespace ObjectFields
{
    std::span<ObjectField const> GetAll() noexcept;
    ObjectField const* Find(std::string_view message, std::string_view field) noexcept;
}

#endif
