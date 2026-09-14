/*
 * Project Ambrose by Imjustchico
 * Looks up messages by order and tag, fields by name, and sums the fixed part of a message's wire size.
 */

#include "MessageDefinition.h"

#include <algorithm>

FieldDef const* MessageDef::FindField(std::string_view name) const noexcept
{
    auto const it = std::find_if(Fields.begin(), Fields.end(), [name](FieldDef const& field) { return field.Name == name; });
    return it == Fields.end() ? nullptr : &*it;
}

std::size_t MessageDef::GetFixedSize() const noexcept
{
    std::size_t size = 0;
    for (FieldDef const& field : Fields)
        size += Dml::GetFixedSize(field.Type);
    return size;
}

bool MessageDef::HasVariableSize() const noexcept
{
    return std::any_of(Fields.begin(), Fields.end(), [](FieldDef const& field) { return Dml::GetFixedSize(field.Type) == 0; });
}

MessageDef const* ProtocolDef::FindByOrder(uint8 order) const noexcept
{
    auto const it = std::lower_bound(Messages.begin(), Messages.end(), order, [](MessageDef const& message, uint8 value) { return message.Order < value; });
    return it != Messages.end() && it->Order == order ? &*it : nullptr;
}

MessageDef const* ProtocolDef::FindByTag(std::string_view tag) const noexcept
{
    auto const it = std::find_if(Messages.begin(), Messages.end(), [tag](MessageDef const& message) { return message.Tag == tag; });
    return it == Messages.end() ? nullptr : &*it;
}
