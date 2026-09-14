/*
 * Project Ambrose by Imjustchico
 * Protocol, message, and field definitions read from the client's message XML, with lookup by wire order and tag.
 */

#ifndef AMBROSE_MESSAGEDEFINITION_H
#define AMBROSE_MESSAGEDEFINITION_H

#include "DmlTypes.h"
#include "Types.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class FieldTypeSource : uint8
{
    Declared,
    Alias,
    MisspelledTpye,
    MisspelledTyp,
    InferredGlobalId
};

struct FieldDef
{
    std::string Name;
    DmlType Type = DmlType::Ubyt;
    FieldTypeSource TypeSource = FieldTypeSource::Declared;
    std::optional<std::string> DefaultValue;
    std::size_t Line = 0;
};

struct MessageDef
{
    std::string Tag;
    std::string Name;
    std::string Handler;
    std::string Description;
    std::optional<uint8> AccessLevel;
    uint8 Order = 0;
    std::vector<FieldDef> Fields;
    std::size_t RecordCount = 1;
    std::size_t Line = 0;

    FieldDef const* FindField(std::string_view name) const noexcept;
    std::size_t GetFixedSize() const noexcept;
    bool HasVariableSize() const noexcept;
};

enum class MessageOrdering : uint8
{
    Explicit,
    SortedByTag
};

struct ProtocolDef
{
    uint8 ServiceId = 0;
    std::string ProtocolType;
    int32 Version = 0;
    std::string Description;
    std::string SourceFile;
    std::string RootElement;
    MessageOrdering Ordering = MessageOrdering::SortedByTag;
    std::vector<MessageDef> Messages;
    std::size_t RecordCount = 0;

    MessageDef const* FindByOrder(uint8 order) const noexcept;
    MessageDef const* FindByTag(std::string_view tag) const noexcept;
};

#endif
