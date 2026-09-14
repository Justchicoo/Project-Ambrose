/*
 * Project Ambrose by Imjustchico
 * Reads _ProtocolInfo, message records, metadata, and typed fields with pugixml, then assigns explicit or tag-sorted wire orders.
 */

#include "MessageDefinitionParser.h"
#include "StringUtil.h"

#include <fmt/format.h>
#include <pugixml.hpp>

#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace
{
    class LineIndex
    {
    public:
        explicit LineIndex(std::string_view text)
        {
            for (std::size_t i = 0; i < text.size(); ++i)
                if (text[i] == '\n')
                    _newlines.push_back(i);
        }

        std::size_t LineOf(std::ptrdiff_t offset) const
        {
            if (offset < 0)
                return 0;
            return static_cast<std::size_t>(std::lower_bound(_newlines.begin(), _newlines.end(), static_cast<std::size_t>(offset)) - _newlines.begin()) + 1;
        }

    private:
        std::vector<std::size_t> _newlines;
    };

    class Context
    {
    public:
        Context(std::string_view xml, std::string_view sourceFile, MessageParseResult& result) : _lines(xml), _source(sourceFile), _result(result)
        {
        }

        std::size_t Line(pugi::xml_node node) const
        {
            return _lines.LineOf(node.offset_debug());
        }

        std::size_t LineOfOffset(std::ptrdiff_t offset) const
        {
            return _lines.LineOf(offset);
        }

        void Error(std::size_t line, std::string message)
        {
            _result.Errors.push_back({ _source, line, std::move(message) });
        }

        void Warning(std::size_t line, std::string message)
        {
            _result.Warnings.push_back({ _source, line, std::move(message) });
        }

    private:
        LineIndex _lines;
        std::string _source;
        MessageParseResult& _result;
    };

    std::string_view Text(pugi::xml_node node)
    {
        return Ambrose::Trim(node.child_value());
    }

    bool IsElement(pugi::xml_node node)
    {
        return node.type() == pugi::node_element;
    }

    std::optional<uint8> ParseByte(std::string_view text)
    {
        std::optional<uint32> const value = Ambrose::StringTo<uint32>(text);
        if (!value || *value > 255)
            return std::nullopt;
        return static_cast<uint8>(*value);
    }

    std::optional<std::size_t> FindNulReference(std::string_view text)
    {
        for (std::size_t start = text.find("&#"); start != std::string_view::npos; start = text.find("&#", start + 2))
        {
            std::size_t position = start + 2;
            bool const hex = position < text.size() && (text[position] == 'x' || text[position] == 'X');
            if (hex)
                ++position;
            std::size_t const digitsStart = position;
            bool nonZero = false;
            while (position < text.size() && (hex ? std::isxdigit(static_cast<unsigned char>(text[position])) != 0 : (text[position] >= '0' && text[position] <= '9')))
            {
                nonZero = nonZero || text[position] != '0';
                ++position;
            }
            if (position > digitsStart && position < text.size() && text[position] == ';' && !nonZero)
                return start;
        }
        return std::nullopt;
    }

    bool SameLayout(MessageDef const& left, MessageDef const& right)
    {
        return std::equal(left.Fields.begin(), left.Fields.end(), right.Fields.begin(), right.Fields.end(), [](FieldDef const& a, FieldDef const& b)
        {
            return a.Name == b.Name && a.Type == b.Type;
        });
    }

    std::optional<FieldDef> ParseField(pugi::xml_node node, std::string_view messageTag, Context& context)
    {
        FieldDef field;
        field.Name = node.name();
        field.Line = context.Line(node);
        for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling())
        {
            if (IsElement(child))
            {
                context.Error(field.Line, fmt::format("{}.{} contains a nested element <{}>", messageTag, field.Name, child.name()));
                return std::nullopt;
            }
        }

        std::string_view typeName;
        pugi::xml_attribute const declared = node.attribute("TYPE");
        if (declared)
            typeName = declared.value();
        else if (pugi::xml_attribute const tpye = node.attribute("TPYE"))
        {
            typeName = tpye.value();
            field.TypeSource = FieldTypeSource::MisspelledTpye;
            context.Warning(field.Line, fmt::format("{}.{} spells TYPE as TPYE; using {}", messageTag, field.Name, typeName));
        }
        else if (pugi::xml_attribute const typ = node.attribute("TYP"))
        {
            typeName = typ.value();
            field.TypeSource = FieldTypeSource::MisspelledTyp;
            context.Warning(field.Line, fmt::format("{}.{} spells TYPE as TYP; using {}", messageTag, field.Name, typeName));
        }
        else if (field.Name == "GlobalID")
        {
            field.Type = DmlType::Gid;
            field.TypeSource = FieldTypeSource::InferredGlobalId;
            context.Warning(field.Line, fmt::format("{}.GlobalID has no TYPE; treating it as GID", messageTag));
        }
        else
        {
            context.Error(field.Line, fmt::format("{}.{} has no TYPE attribute", messageTag, field.Name));
            return std::nullopt;
        }

        if (field.TypeSource != FieldTypeSource::InferredGlobalId)
        {
            std::optional<DmlTypeName> const parsed = Dml::ParseType(Ambrose::Trim(typeName));
            if (!parsed)
            {
                context.Error(field.Line, fmt::format("{}.{} has unknown type '{}'", messageTag, field.Name, typeName));
                return std::nullopt;
            }
            field.Type = parsed->Type;
            if (parsed->IsAlias && field.TypeSource == FieldTypeSource::Declared)
            {
                field.TypeSource = FieldTypeSource::Alias;
                context.Warning(field.Line, fmt::format("{}.{} uses the type alias '{}' for {}", messageTag, field.Name, typeName, Dml::GetTypeName(parsed->Type)));
            }
        }

        if (std::string_view const value = Text(node); !value.empty())
            field.DefaultValue = std::string(value);
        if (pugi::xml_attribute const noTransfer = node.attribute("NOXFER"); noTransfer && Ambrose::EqualsIgnoreCase(noTransfer.value(), "TRUE"))
            context.Warning(field.Line, fmt::format("{}.{} is marked NOXFER but is not metadata; it stays on the wire", messageTag, field.Name));
        return field;
    }

    struct ParsedMessage
    {
        MessageDef Definition;
        std::optional<uint32> ExplicitOrder;
        bool HasOrderElement = false;
    };

    std::optional<ParsedMessage> ParseMessage(pugi::xml_node node, Context& context)
    {
        ParsedMessage parsed;
        MessageDef& message = parsed.Definition;
        message.Tag = node.name();
        message.Line = context.Line(node);

        pugi::xml_node record;
        for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling())
        {
            if (!IsElement(child))
                continue;
            if (std::string_view(child.name()) != "RECORD")
            {
                context.Error(context.Line(child), fmt::format("{} contains <{}>; only one RECORD is allowed", message.Tag, child.name()));
                return std::nullopt;
            }
            if (record)
            {
                context.Error(context.Line(child), fmt::format("{} has more than one RECORD", message.Tag));
                return std::nullopt;
            }
            record = child;
        }
        if (!record)
        {
            context.Error(message.Line, fmt::format("{} has no RECORD", message.Tag));
            return std::nullopt;
        }

        std::set<std::string, std::less<>> fieldNames;
        std::set<std::string, std::less<>> metadataNames;
        bool failed = false;
        for (pugi::xml_node child = record.first_child(); child; child = child.next_sibling())
        {
            if (!IsElement(child))
                continue;
            std::string_view const name = child.name();
            std::size_t const line = context.Line(child);
            if (MessageDefinitionParser::IsMetadataName(name))
            {
                if (!metadataNames.emplace(name).second)
                {
                    context.Error(line, fmt::format("{} repeats <{}>", message.Tag, name));
                    failed = true;
                    continue;
                }
                std::string_view const value = Text(child);
                if (name == "_MsgName")
                    message.Name = std::string(value);
                else if (name == "_MsgDescription")
                    message.Description = std::string(value);
                else if (name == "_MsgHandler")
                    message.Handler = std::string(value);
                else if (name == "_MsgAccessLvl")
                {
                    std::optional<uint8> const level = ParseByte(value);
                    if (!level)
                    {
                        context.Error(line, fmt::format("{} has access level '{}'; use 0-255", message.Tag, value));
                        failed = true;
                    }
                    message.AccessLevel = level;
                }
                else if (name == "_MsgOrder" || name == "_MsgType")
                {
                    if (parsed.HasOrderElement)
                    {
                        context.Error(line, fmt::format("{} has both _MsgOrder and _MsgType", message.Tag));
                        failed = true;
                        continue;
                    }
                    parsed.HasOrderElement = true;
                    std::optional<uint32> const order = Ambrose::StringTo<uint32>(value);
                    if (!order)
                    {
                        context.Error(line, fmt::format("{} has {} '{}', which is not a number", message.Tag, name, value));
                        failed = true;
                    }
                    parsed.ExplicitOrder = order;
                }
                else
                    context.Warning(line, fmt::format("{} has unknown metadata <{}>", message.Tag, name));
                continue;
            }
            std::optional<FieldDef> field = ParseField(child, message.Tag, context);
            if (!field)
            {
                failed = true;
                continue;
            }
            if (!fieldNames.emplace(field->Name).second)
            {
                context.Error(line, fmt::format("{} declares field {} more than once", message.Tag, field->Name));
                failed = true;
                continue;
            }
            message.Fields.push_back(std::move(*field));
        }
        if (failed)
            return std::nullopt;
        return parsed;
    }
}

std::string MessageIssue::ToString() const
{
    if (Line == 0)
        return fmt::format("{}: {}", SourceFile, Message);
    return fmt::format("{}:{}: {}", SourceFile, Line, Message);
}

bool MessageDefinitionParser::IsMetadataName(std::string_view name) noexcept
{
    return !name.empty() && name.front() == '_';
}

bool MessageDefinitionParser::TagLess(std::string_view left, std::string_view right) noexcept
{
    return std::lexicographical_compare(left.begin(), left.end(), right.begin(), right.end(), [](char a, char b)
    {
        return static_cast<unsigned char>(a) < static_cast<unsigned char>(b);
    });
}

MessageParseResult MessageDefinitionParser::Parse(std::string_view xml, std::string_view sourceFile)
{
    MessageParseResult result;
    Context context(xml, sourceFile, result);

    if (std::size_t const nul = xml.find('\0'); nul != std::string_view::npos)
    {
        context.Error(context.LineOfOffset(static_cast<std::ptrdiff_t>(nul)), "the XML contains a NUL byte");
        return result;
    }
    if (std::optional<std::size_t> const reference = FindNulReference(xml))
    {
        context.Error(context.LineOfOffset(static_cast<std::ptrdiff_t>(*reference)), "the XML contains a character reference to NUL");
        return result;
    }

    pugi::xml_document document;
    pugi::xml_parse_result const loaded = document.load_buffer(xml.data(), xml.size(), pugi::parse_default | pugi::parse_fragment, pugi::encoding_utf8);
    if (!loaded)
    {
        context.Error(context.LineOfOffset(loaded.offset), fmt::format("the XML is malformed: {}", loaded.description()));
        return result;
    }
    pugi::xml_node const root = document.document_element();
    if (!root)
    {
        context.Error(0, "the XML has no root element");
        return result;
    }
    for (pugi::xml_node node = document.first_child(); node; node = node.next_sibling())
    {
        if (node == root)
            continue;
        bool const text = node.type() == pugi::node_pcdata || node.type() == pugi::node_cdata;
        if (IsElement(node) || (text && !Ambrose::Trim(node.value()).empty()))
        {
            context.Error(IsElement(node) ? context.Line(node) : context.Line(root), fmt::format("the XML has content outside the root element <{}>", root.name()));
            return result;
        }
    }

    ProtocolDef protocol;
    protocol.SourceFile = std::string(sourceFile);
    protocol.RootElement = root.name();

    pugi::xml_node const info = root.child("_ProtocolInfo").child("RECORD");
    if (!info)
    {
        context.Error(context.Line(root), "the file has no _ProtocolInfo RECORD");
        return result;
    }
    pugi::xml_node const serviceNode = info.child("ServiceID");
    std::optional<uint8> const serviceId = serviceNode ? ParseByte(Text(serviceNode)) : std::nullopt;
    if (!serviceId)
    {
        context.Error(context.Line(serviceNode ? serviceNode : info), fmt::format("ServiceID '{}' is missing or not 0-255", serviceNode ? Text(serviceNode) : std::string_view()));
        return result;
    }
    protocol.ServiceId = *serviceId;
    protocol.ProtocolType = std::string(Text(info.child("ProtocolType")));
    protocol.Description = std::string(Text(info.child("ProtocolDescription")));
    if (pugi::xml_node const versionNode = info.child("ProtocolVersion"))
    {
        std::optional<int32> const version = Ambrose::StringTo<int32>(Text(versionNode));
        if (!version)
            context.Error(context.Line(versionNode), fmt::format("ProtocolVersion '{}' is not a number", Text(versionNode)));
        else
            protocol.Version = *version;
    }

    std::vector<ParsedMessage> parsed;
    bool anyFailed = false;
    for (pugi::xml_node child = root.first_child(); child; child = child.next_sibling())
    {
        if (!IsElement(child) || IsMetadataName(child.name()))
            continue;
        ++protocol.RecordCount;
        std::optional<ParsedMessage> message = ParseMessage(child, context);
        if (!message)
        {
            anyFailed = true;
            continue;
        }
        parsed.push_back(std::move(*message));
    }
    if (anyFailed || !result.Errors.empty())
        return result;
    if (parsed.empty())
    {
        context.Error(context.Line(root), "the file defines no messages");
        return result;
    }

    protocol.Ordering = parsed.front().HasOrderElement ? MessageOrdering::Explicit : MessageOrdering::SortedByTag;
    std::map<std::string, std::size_t, std::less<>> byTag;
    std::vector<MessageDef> unique;
    for (ParsedMessage& message : parsed)
    {
        auto const existing = byTag.find(message.Definition.Tag);
        if (existing != byTag.end())
        {
            MessageDef& first = unique[existing->second];
            if (!SameLayout(first, message.Definition))
            {
                context.Error(message.Definition.Line, fmt::format("{} was first defined on line {} with different fields", message.Definition.Tag, first.Line));
                continue;
            }
            if (protocol.Ordering == MessageOrdering::Explicit && (!message.ExplicitOrder || *message.ExplicitOrder != first.Order))
            {
                context.Error(message.Definition.Line, fmt::format("{} was first defined on line {} with a different order", message.Definition.Tag, first.Line));
                continue;
            }
            ++first.RecordCount;
            continue;
        }
        if (protocol.Ordering == MessageOrdering::Explicit)
        {
            if (!message.ExplicitOrder)
            {
                context.Error(message.Definition.Line, fmt::format("{} has no _MsgOrder or _MsgType, but this file numbers messages explicitly", message.Definition.Tag));
                continue;
            }
            if (*message.ExplicitOrder == 0 || *message.ExplicitOrder > 255)
            {
                context.Error(message.Definition.Line, fmt::format("{} has order {}; message orders are 1-255", message.Definition.Tag, *message.ExplicitOrder));
                continue;
            }
            message.Definition.Order = static_cast<uint8>(*message.ExplicitOrder);
        }
        else if (message.HasOrderElement)
            context.Warning(message.Definition.Line, fmt::format("{} has an explicit order, but the first message does not, so the file is numbered by tag", message.Definition.Tag));
        byTag.emplace(message.Definition.Tag, unique.size());
        unique.push_back(std::move(message.Definition));
    }
    if (!result.Errors.empty())
        return result;

    if (protocol.Ordering == MessageOrdering::SortedByTag)
    {
        if (unique.size() > MaxMessagesPerProtocol)
        {
            context.Error(context.Line(root), fmt::format("{} distinct messages exceed the {} a protocol can number", unique.size(), MaxMessagesPerProtocol));
            return result;
        }
        std::stable_sort(unique.begin(), unique.end(), [](MessageDef const& left, MessageDef const& right) { return TagLess(left.Tag, right.Tag); });
        for (std::size_t i = 0; i < unique.size(); ++i)
            unique[i].Order = static_cast<uint8>(i + 1);
    }
    else
    {
        std::stable_sort(unique.begin(), unique.end(), [](MessageDef const& left, MessageDef const& right) { return left.Order < right.Order; });
        for (std::size_t i = 1; i < unique.size(); ++i)
        {
            if (unique[i].Order == unique[i - 1].Order)
                context.Error(unique[i].Line, fmt::format("{} and {} (line {}) both use order {}", unique[i].Tag, unique[i - 1].Tag, unique[i - 1].Line, unique[i].Order));
        }
        if (!result.Errors.empty())
            return result;
    }

    protocol.Messages = std::move(unique);
    result.Protocol = std::move(protocol);
    return result;
}
