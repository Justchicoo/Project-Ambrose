/*
 * Project Ambrose by Imjustchico
 * Charges an upper bound of the parsed document against the memory budget before parsing it as UTF-8 with pugixml, which skips a byte order mark and keeps whitespace-only values; refuses text or a second element beside the root; walks each Class element into a default object of its class, fills each property from its element or, for containers, appends each element of that name in document order on its own, joins a value's text and CDATA across comments, reads numbers, bools, option names and flag lists, UTF-8 text, AARRGGBB colors and comma or space separated math types, reads objects from a single nested Class element, credits a default inline object an explicit one replaces, and charges every object, element and string against the decode limits; anything that cannot be read is reported with its property path and line and the property keeps its default or earlier value.
 */

#include "XmlObjectReader.h"
#include "PropertyEnums.h"
#include "StringHash.h"
#include "StringUtil.h"
#include "Utf.h"

#include <pugixml.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <limits>
#include <new>
#include <string>
#include <vector>

namespace
{
    constexpr std::string_view ObjectsElement = "Objects";
    constexpr std::string_view ClassElement = "Class";
    constexpr char const* NameAttribute = "Name";
    constexpr std::size_t ValueBytes = sizeof(PropertyValue);
    constexpr std::size_t ExcerptBytes = 64;
    constexpr std::size_t ParseNodeBytes = 64;
    constexpr std::size_t ParseAttributeBytes = 40;
    constexpr std::string_view Utf8Bom = "\xEF\xBB\xBF";

    bool IsTextNumber(PropertyInfo const& property) noexcept
    {
        if (property.Kind == ValueKind::Enum)
            return true;
        return (property.Kind == ValueKind::Int32 || property.Kind == ValueKind::UInt32) && (property.HasFlag(PropertyFlag::Bits) || property.HasFlag(PropertyFlag::Enum));
    }

    template<typename T, std::size_t N>
    std::optional<std::array<T, N>> ParseTuple(std::string_view text)
    {
        std::array<T, N> values{};
        std::size_t count = 0;
        std::size_t position = 0;
        while (position < text.size())
        {
            while (position < text.size() && (text[position] == ',' || std::isspace(static_cast<unsigned char>(text[position]))))
                ++position;
            if (position == text.size())
                break;
            std::size_t end = position;
            while (end < text.size() && text[end] != ',' && !std::isspace(static_cast<unsigned char>(text[end])))
                ++end;
            if (count == N)
                return std::nullopt;
            std::optional<T> const value = Ambrose::StringTo<T>(text.substr(position, end - position));
            if (!value)
                return std::nullopt;
            values[count++] = *value;
            position = end;
        }
        if (count != N)
            return std::nullopt;
        return values;
    }

    std::optional<PropertyTypes::Color> ParseColor(std::string_view text)
    {
        if (text.size() != 8)
            return std::nullopt;
        std::optional<uint32> const packed = Ambrose::StringTo<uint32>(text, 16);
        if (!packed)
            return std::nullopt;
        return PropertyTypes::Color{ static_cast<uint8>(*packed >> 16), static_cast<uint8>(*packed >> 8), static_cast<uint8>(*packed), static_cast<uint8>(*packed >> 24) };
    }

    template<typename T>
    std::optional<PropertyValue> Scalar(std::string_view text)
    {
        std::optional<T> const value = Ambrose::StringTo<T>(Ambrose::Trim(text));
        return value ? std::optional<PropertyValue>(PropertyValue(*value)) : std::nullopt;
    }

    std::optional<PropertyValue> ParseText(PropertyInfo const& property, std::string_view text)
    {
        if (IsTextNumber(property))
        {
            std::optional<int64> const value = PropertyEnums::Parse(property, text);
            if (!value || *value < 0 || *value > static_cast<int64>(std::numeric_limits<uint32>::max()))
                return std::nullopt;
            if (property.Kind == ValueKind::Int32)
                return PropertyValue(static_cast<int32>(static_cast<uint32>(*value)));
            if (property.Kind == ValueKind::UInt32)
                return PropertyValue(static_cast<uint32>(*value));
            return PropertyValue(*value);
        }
        switch (property.Kind)
        {
            case ValueKind::Bool: return Scalar<bool>(text);
            case ValueKind::Int8: return Scalar<int8>(text);
            case ValueKind::UInt8: return Scalar<uint8>(text);
            case ValueKind::Int16: return Scalar<int16>(text);
            case ValueKind::UInt16: return Scalar<uint16>(text);
            case ValueKind::Int32:
            case ValueKind::SignedBits:
            case ValueKind::S24: return Scalar<int32>(text);
            case ValueKind::UInt32:
            case ValueKind::UnsignedBits:
            case ValueKind::U24: return Scalar<uint32>(text);
            case ValueKind::Int64: return Scalar<int64>(text);
            case ValueKind::UInt64:
            case ValueKind::Gid: return Scalar<uint64>(text);
            case ValueKind::Float: return Scalar<float>(text);
            case ValueKind::Double: return Scalar<double>(text);
            case ValueKind::String: return PropertyValue(std::string(text));
            case ValueKind::WideString:
            {
                std::optional<std::u16string> wide = Utf::Utf8ToUtf16(text, Utf::InvalidPolicy::Reject);
                return wide ? std::optional<PropertyValue>(PropertyValue(std::move(*wide))) : std::nullopt;
            }
            case ValueKind::WideChar:
            {
                std::optional<std::u16string> const wide = Utf::Utf8ToUtf16(text, Utf::InvalidPolicy::Reject);
                return wide && wide->size() == 1 ? std::optional<PropertyValue>(PropertyValue((*wide)[0])) : std::nullopt;
            }
            case ValueKind::Color:
            {
                std::optional<PropertyTypes::Color> const color = ParseColor(Ambrose::Trim(text));
                return color ? std::optional<PropertyValue>(PropertyValue(*color)) : std::nullopt;
            }
            case ValueKind::Vector3D:
            {
                std::optional<std::array<float, 3>> const values = ParseTuple<float, 3>(text);
                return values ? std::optional<PropertyValue>(PropertyValue(PropertyTypes::Vector3D{ (*values)[0], (*values)[1], (*values)[2] })) : std::nullopt;
            }
            case ValueKind::Quaternion:
            {
                std::optional<std::array<float, 4>> const values = ParseTuple<float, 4>(text);
                return values ? std::optional<PropertyValue>(PropertyValue(PropertyTypes::Quaternion{ (*values)[0], (*values)[1], (*values)[2], (*values)[3] })) : std::nullopt;
            }
            case ValueKind::Euler:
            {
                std::optional<std::array<float, 3>> const values = ParseTuple<float, 3>(text);
                return values ? std::optional<PropertyValue>(PropertyValue(PropertyTypes::Euler{ (*values)[0], (*values)[1], (*values)[2] })) : std::nullopt;
            }
            case ValueKind::Matrix3x3:
            {
                std::optional<std::array<float, 9>> const values = ParseTuple<float, 9>(text);
                if (!values)
                    return std::nullopt;
                PropertyTypes::Matrix3x3 matrix;
                std::copy(values->begin(), values->end(), matrix.Values.begin());
                return PropertyValue(matrix);
            }
            case ValueKind::PointInt:
            {
                std::optional<std::array<int32, 2>> const values = ParseTuple<int32, 2>(text);
                return values ? std::optional<PropertyValue>(PropertyValue(PropertyTypes::PointInt{ (*values)[0], (*values)[1] })) : std::nullopt;
            }
            case ValueKind::PointFloat:
            {
                std::optional<std::array<float, 2>> const values = ParseTuple<float, 2>(text);
                return values ? std::optional<PropertyValue>(PropertyValue(PropertyTypes::PointFloat{ (*values)[0], (*values)[1] })) : std::nullopt;
            }
            case ValueKind::SizeInt:
            {
                std::optional<std::array<int32, 2>> const values = ParseTuple<int32, 2>(text);
                return values ? std::optional<PropertyValue>(PropertyValue(PropertyTypes::SizeInt{ (*values)[0], (*values)[1] })) : std::nullopt;
            }
            case ValueKind::RectInt:
            {
                std::optional<std::array<int32, 4>> const values = ParseTuple<int32, 4>(text);
                return values ? std::optional<PropertyValue>(PropertyValue(PropertyTypes::RectInt{ (*values)[0], (*values)[1], (*values)[2], (*values)[3] })) : std::nullopt;
            }
            case ValueKind::RectFloat:
            {
                std::optional<std::array<float, 4>> const values = ParseTuple<float, 4>(text);
                return values ? std::optional<PropertyValue>(PropertyValue(PropertyTypes::RectFloat{ (*values)[0], (*values)[1], (*values)[2], (*values)[3] })) : std::nullopt;
            }
            case ValueKind::Enum:
            case ValueKind::Object:
            case ValueKind::SerializedBuffer:
            case ValueKind::SimpleVert:
            case ValueKind::SimpleFace: return std::nullopt;
        }
        return std::nullopt;
    }

    class Reader
    {
    public:
        Reader(TypeCatalogPtr const& catalog, std::string_view text, SerializerLimits const& limits)
            : _catalog(catalog), _text(text), _limits(limits), _depthLimit(std::min(limits.MaxDepth, SerializerLimits::DepthCeiling))
        {
        }

        XmlReadResult Run()
        {
            std::size_t const estimate = EstimateParseBytes();
            if (estimate > _limits.MaxDecodedBytes)
                return Refuse(XmlReadStatus::BudgetExceeded, fmt::format("needs about {} bytes of memory to parse, more than the {} a read may use", estimate, _limits.MaxDecodedBytes));
            _charged = estimate;

            pugi::xml_document document;
            pugi::xml_parse_result const parsed = document.load_buffer(_text.data(), _text.size(), pugi::parse_default | pugi::parse_ws_pcdata_single, pugi::encoding_utf8);
            if (parsed.status == pugi::status_out_of_memory)
                return Refuse(XmlReadStatus::OutOfMemory, "ran out of memory while parsing");
            if (!parsed)
                return Refuse(XmlReadStatus::BadXml, fmt::format("is not well-formed XML: {} on line {}", parsed.description(), LineAt(static_cast<std::size_t>(parsed.offset))));
            std::string_view body = Ambrose::Trim(_text.starts_with(Utf8Bom) ? _text.substr(Utf8Bom.size()) : _text);
            if (!body.starts_with('<') || !body.ends_with('>'))
                return Refuse(XmlReadStatus::BadXml, "holds text outside its root element");
            pugi::xml_node root;
            for (pugi::xml_node node = document.first_child(); node; node = node.next_sibling())
            {
                if (IsText(node))
                    return Refuse(XmlReadStatus::BadXml, fmt::format("holds text outside its root element on line {}", Line(node)));
                if (node.type() != pugi::node_element)
                    continue;
                if (root)
                    return Refuse(XmlReadStatus::BadXml, fmt::format("holds a second root element <{}> on line {}", node.name(), Line(node)));
                root = node;
            }
            if (!root || std::string_view(root.name()) != ObjectsElement)
                return Refuse(XmlReadStatus::NotObjects, fmt::format("has the root element <{}> where <Objects> belongs", root ? root.name() : ""));

            for (pugi::xml_node child = root.first_child(); child; child = child.next_sibling())
            {
                if (IsText(child))
                {
                    if (!Report(DecodeIssueKind::InvalidValue, 0, "Objects", child, "holds text where a Class element belongs"))
                        return Finish();
                    continue;
                }
                if (child.type() != pugi::node_element)
                    continue;
                if (std::string_view(child.name()) != ClassElement)
                {
                    if (!Report(DecodeIssueKind::UnknownProperty, 0, "Objects", child, fmt::format("holds <{}> where a Class element belongs", child.name())))
                        return Finish();
                    continue;
                }
                PropertyObjectPtr object;
                if (!ReadClass(child, 1, nullptr, "Objects", object))
                    return Finish();
                if (object)
                    _result.Objects.push_back(std::move(object));
            }
            return Finish();
        }

    private:
        static bool IsText(pugi::xml_node node)
        {
            return (node.type() == pugi::node_pcdata || node.type() == pugi::node_cdata) && !Ambrose::Trim(node.value()).empty();
        }

        std::size_t EstimateParseBytes() const
        {
            std::size_t const markup = static_cast<std::size_t>(std::count(_text.begin(), _text.end(), '<'));
            std::size_t const attributes = static_cast<std::size_t>(std::count(_text.begin(), _text.end(), '='));
            std::size_t const lines = static_cast<std::size_t>(std::count(_text.begin(), _text.end(), '\n'));
            return _text.size() + markup * 2 * ParseNodeBytes + attributes * ParseAttributeBytes + lines * sizeof(std::size_t);
        }

        XmlReadResult Refuse(XmlReadStatus status, std::string detail)
        {
            XmlReadResult result;
            result.Status = status;
            result.Detail = std::move(detail);
            return result;
        }

        XmlReadResult Finish()
        {
            if (_result.Status != XmlReadStatus::Ok)
            {
                _result.Objects.clear();
                _result.Issues.clear();
            }
            return std::move(_result);
        }

        std::size_t LineAt(std::size_t offset)
        {
            if (!_lines)
            {
                _lines.emplace();
                for (std::size_t index = 0; index < _text.size(); ++index)
                    if (_text[index] == '\n')
                        _lines->push_back(index);
            }
            return static_cast<std::size_t>(std::lower_bound(_lines->begin(), _lines->end(), offset) - _lines->begin()) + 1;
        }

        std::size_t Line(pugi::xml_node node)
        {
            std::size_t offset = static_cast<std::size_t>(std::max<std::ptrdiff_t>(node.offset_debug(), 0));
            if (node.type() == pugi::node_pcdata)
            {
                std::string_view const value = node.value();
                offset += std::min(value.find_first_not_of(" \t\r\n"), value.size());
            }
            return LineAt(offset);
        }

        bool Fail(XmlReadStatus status, std::string_view path, pugi::xml_node node, std::string_view what)
        {
            if (_result.Status == XmlReadStatus::Ok)
            {
                _result.Status = status;
                _result.Detail = fmt::format("{} {} on line {}", path, what, Line(node));
            }
            return false;
        }

        bool Charge(std::size_t bytes, std::string_view path, pugi::xml_node node)
        {
            if (bytes > _limits.MaxDecodedBytes - _charged)
                return Fail(XmlReadStatus::BudgetExceeded, path, node, fmt::format("needs more than the {} bytes of memory a read may use", _limits.MaxDecodedBytes));
            _charged += bytes;
            return true;
        }

        bool Report(DecodeIssueKind kind, uint32 hash, std::string path, pugi::xml_node node, std::string what)
        {
            std::string detail = fmt::format("{} on line {}", what, Line(node));
            if (!Charge(sizeof(DecodeIssue) + path.size() + detail.size(), path, node))
                return false;
            _result.Issues.push_back(DecodeIssue{ kind, hash, 0, std::move(path), std::move(detail) });
            return true;
        }

        bool ReadClass(pugi::xml_node node, uint32 depth, ClassInfo const* expected, std::string const& path, PropertyObjectPtr& out)
        {
            std::string_view const name = node.attribute(NameAttribute).value();
            ClassInfo const* const type = _catalog->FindClass(name);
            if (!type)
                return Report(DecodeIssueKind::UnknownClass, StringHash::KiStringHash(name), path, node, fmt::format("names class '{}', which the type dump does not list", std::string_view(name).substr(0, ExcerptBytes)));
            if (type->Kind != ClassKind::PropertyClass)
                return Report(DecodeIssueKind::InvalidObject, type->Hash, path, node, fmt::format("names {}, which is not a property class", type->Name));
            if (expected && !type->IsA(*expected))
                return Report(DecodeIssueKind::InvalidObject, type->Hash, path, node, fmt::format("holds a {}, which is not a {}", type->Name, expected->Name));
            if (depth + type->DefaultDepth - 1 > _depthLimit)
                return Fail(XmlReadStatus::TooDeep, path, node, fmt::format("nests objects deeper than {}", _depthLimit));
            if (type->DefaultObjects > _limits.MaxObjects - std::min(_objects, _limits.MaxObjects))
                return Fail(XmlReadStatus::TooManyObjects, path, node, fmt::format("holds more than {} objects", _limits.MaxObjects));
            _objects += type->DefaultObjects;
            if (!Charge(type->DefaultBytes, path, node))
                return false;

            PropertyObjectPtr object = PropertyObject::Create(_catalog, *type);
            if (!object)
                return Report(DecodeIssueKind::InvalidObject, type->Hash, path, node, fmt::format("names {}, which cannot be created", type->Name));
            std::string const objectPath = depth == 1 ? type->Name : path;
            std::vector<bool> seen(type->Properties.size(), false);
            std::vector<std::size_t> positions(type->Properties.size(), 0);
            for (pugi::xml_node element = node.first_child(); element; element = element.next_sibling())
            {
                if (IsText(element))
                {
                    if (!Report(DecodeIssueKind::InvalidValue, 0, objectPath, element, "holds text outside any property"))
                        return false;
                    continue;
                }
                if (element.type() != pugi::node_element)
                    continue;
                std::string_view const propertyName = element.name();
                PropertyInfo const* const property = type->FindProperty(propertyName);
                if (!property)
                {
                    if (!Report(DecodeIssueKind::UnknownProperty, 0, objectPath, element, fmt::format("holds <{}>, which {} does not list", propertyName.substr(0, ExcerptBytes), type->Name)))
                        return false;
                    continue;
                }
                std::size_t const ordinal = static_cast<std::size_t>(property - type->Properties.data());
                std::string propertyPath = objectPath + '.' + property->Name;
                if (property->Container != ContainerKind::Static)
                {
                    if (!seen[ordinal])
                    {
                        object->SetAt(ordinal, PropertyValue::List());
                        seen[ordinal] = true;
                    }
                    propertyPath += fmt::format("[{}]", positions[ordinal]++);
                    std::size_t const index = object->GetAt(ordinal)->GetList()->size();
                    if (index >= _limits.MaxContainerCount)
                        return Fail(XmlReadStatus::TooManyObjects, propertyPath, element, fmt::format("lists more than {} elements", _limits.MaxContainerCount));
                    if (!Charge(ValueBytes, propertyPath, element))
                        return false;
                    PropertyValue value;
                    bool read = false;
                    if (!ReadElement(*property, element, depth, propertyPath, value, read))
                        return false;
                    if (!read)
                        continue;
                    if (PropertySetResult const set = object->SetElementAt(ordinal, index, std::move(value)); set != PropertySetResult::Ok
                        && !Report(DecodeIssueKind::InvalidValue, property->Hash, propertyPath, element, fmt::format("was refused and left out: {}", PropertyObject::GetResultName(set))))
                        return false;
                    continue;
                }

                bool const replacesDefault = property->Kind == ValueKind::Object && !property->Pointer && property->Type && !seen[ordinal];
                if (replacesDefault)
                {
                    _objects -= std::min(_objects, property->Type->DefaultObjects);
                    _charged -= std::min(_charged, property->DefaultBytes);
                }
                PropertyValue value;
                bool read = false;
                if (!ReadElement(*property, element, depth, propertyPath, value, read))
                    return false;
                bool applied = false;
                if (read)
                {
                    PropertySetResult const set = object->SetAt(ordinal, std::move(value));
                    applied = set == PropertySetResult::Ok;
                    if (!applied && !Report(DecodeIssueKind::InvalidValue, property->Hash, propertyPath, element,
                            fmt::format("was refused: {}; the property keeps {}", PropertyObject::GetResultName(set), seen[ordinal] ? "its earlier value" : "its default")))
                        return false;
                }
                if (replacesDefault && !applied)
                {
                    _objects += property->Type->DefaultObjects;
                    _charged += property->DefaultBytes;
                }
                if (!applied)
                    continue;
                if (seen[ordinal] && !Report(DecodeIssueKind::InvalidValue, property->Hash, propertyPath, element, "appears more than once; the last one is kept"))
                    return false;
                seen[ordinal] = true;
            }
            out = std::move(object);
            return true;
        }

        bool ReadElement(PropertyInfo const& property, pugi::xml_node element, uint32 depth, std::string const& path, PropertyValue& out, bool& read)
        {
            std::string text;
            std::vector<pugi::xml_node> children;
            for (pugi::xml_node child = element.first_child(); child; child = child.next_sibling())
            {
                if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata)
                    text += child.value();
                else if (child.type() == pugi::node_element)
                    children.push_back(child);
            }
            if (!Charge(text.size(), path, element))
                return false;

            if (property.Kind == ValueKind::Object)
            {
                if (!Ambrose::Trim(text).empty() || children.size() > 1 || (children.size() == 1 && std::string_view(children.front().name()) != ClassElement))
                    return Report(DecodeIssueKind::InvalidValue, property.Hash, path, element, "holds something other than a single Class element or nothing");
                if (children.empty())
                {
                    if (property.Pointer)
                    {
                        out = PropertyObjectPtr();
                        read = true;
                    }
                    else if (property.Container != ContainerKind::Static)
                        return Report(DecodeIssueKind::InvalidObject, property.Hash, path, element, "holds a null inline object");
                    return true;
                }
                PropertyObjectPtr child;
                if (!ReadClass(children.front(), depth + 1, property.Type, path, child))
                    return false;
                if (!child)
                {
                    if (property.Pointer)
                    {
                        out = PropertyObjectPtr();
                        read = true;
                    }
                    return true;
                }
                out = std::move(child);
                read = true;
                return true;
            }
            if (!children.empty())
                return Report(DecodeIssueKind::InvalidValue, property.Hash, path, element, fmt::format("holds the element <{}> where a {} value belongs", children.front().name(), property.TypeName));
            std::optional<PropertyValue> value = ParseText(property, text);
            if (!value)
            {
                DecodeIssueKind const kind = IsTextNumber(property) ? DecodeIssueKind::UnknownEnumName
                    : property.Kind == ValueKind::SerializedBuffer || property.Kind == ValueKind::SimpleVert || property.Kind == ValueKind::SimpleFace ? DecodeIssueKind::UnsupportedType
                                                                                                                                                   : DecodeIssueKind::InvalidValue;
                return Report(kind, property.Hash, path, element, fmt::format("holds '{}', which does not read as {}", std::string_view(text).substr(0, ExcerptBytes), property.TypeName));
            }
            out = std::move(*value);
            read = true;
            return true;
        }

        TypeCatalogPtr const& _catalog;
        std::string_view _text;
        SerializerLimits _limits;
        uint32 _depthLimit;
        uint32 _objects = 0;
        std::size_t _charged = 0;
        std::optional<std::vector<std::size_t>> _lines;
        XmlReadResult _result;
    };
}

XmlReadResult XmlObjectReader::Read(TypeCatalogPtr const& catalog, std::string_view text, std::optional<SerializerLimits> limits)
{
    if (!catalog)
    {
        XmlReadResult result;
        result.Status = XmlReadStatus::NotObjects;
        result.Detail = "cannot be read without a type catalog";
        return result;
    }
    try
    {
        return Reader(catalog, text, limits.value_or(SerializerLimits::Current())).Run();
    }
    catch (std::bad_alloc const&)
    {
        XmlReadResult result;
        result.Status = XmlReadStatus::OutOfMemory;
        result.Detail = "memory ran out while reading";
        return result;
    }
}

std::string_view XmlObjectReader::GetStatusName(XmlReadStatus status) noexcept
{
    switch (status)
    {
        case XmlReadStatus::Ok: return "ok";
        case XmlReadStatus::BadXml: return "the document is not well-formed XML";
        case XmlReadStatus::NotObjects: return "the document is not an Objects document";
        case XmlReadStatus::TooDeep: return "objects nest deeper than the limit";
        case XmlReadStatus::TooManyObjects: return "the document holds more objects or elements than the limit";
        case XmlReadStatus::BudgetExceeded: return "the objects need more memory than the limit";
        case XmlReadStatus::OutOfMemory: return "memory ran out";
    }
    return "unknown";
}
