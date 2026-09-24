/*
 * Project Ambrose by Imjustchico
 * Reads the client's plain-XML ObjectProperty files, an Objects element holding Class elements named by class, each property an element named by the property with its value as text or a nested Class, and containers as repeated elements, into property objects; unknown classes, unknown properties and values that do not parse are skipped and reported with their path and line, while a document that is not such XML or breaks the decode limits is refused.
 */

#ifndef AMBROSE_XMLOBJECTREADER_H
#define AMBROSE_XMLOBJECTREADER_H

#include "ObjectSerializer.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class XmlReadStatus : uint8
{
    Ok,
    BadXml,
    NotObjects,
    TooDeep,
    TooManyObjects,
    BudgetExceeded,
    OutOfMemory
};

struct XmlReadResult
{
    XmlReadStatus Status = XmlReadStatus::Ok;
    std::vector<PropertyObjectPtr> Objects;
    std::vector<DecodeIssue> Issues;
    std::string Detail;

    bool Ok() const noexcept { return Status == XmlReadStatus::Ok; }
};

class XmlObjectReader
{
public:
    XmlObjectReader() = delete;

    static XmlReadResult Read(TypeCatalogPtr const& catalog, std::string_view text, std::optional<SerializerLimits> limits = std::nullopt);
    static std::string_view GetStatusName(XmlReadStatus status) noexcept;
};

#endif
