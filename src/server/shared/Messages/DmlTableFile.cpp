/*
 * Project Ambrose by Imjustchico
 * Parses the document with pugixml, which skips a byte order mark and the declaration, reads the table names from the _TableList's records, and for each takes the element of that name, every RECORD in it and every field element of each record with its text joined across comments; a field's KEY counts only when it says TRUE.
 */

#include "DmlTableFile.h"

#include <pugixml.hpp>

#include <fmt/format.h>

#include <algorithm>

namespace
{
    constexpr std::string_view TableListElement = "_TableList";
    constexpr std::string_view RecordElement = "RECORD";
    constexpr std::string_view NameField = "Name";

    std::string Text(pugi::xml_node element)
    {
        std::string text;
        for (pugi::xml_node child = element.first_child(); child; child = child.next_sibling())
            if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata)
                text += child.value();
        return text;
    }

    DmlTableRecord ReadRecord(pugi::xml_node record)
    {
        DmlTableRecord read;
        for (pugi::xml_node field = record.first_child(); field; field = field.next_sibling())
        {
            if (field.type() != pugi::node_element)
                continue;
            read.Fields.push_back({ field.name(), field.attribute("TYPE").value(), std::string_view(field.attribute("KEY").value()) == "TRUE", Text(field) });
        }
        return read;
    }
}

DmlTableField const* DmlTableRecord::Find(std::string_view name) const noexcept
{
    auto const found = std::find_if(Fields.begin(), Fields.end(), [name](DmlTableField const& field) { return field.Name == name; });
    return found == Fields.end() ? nullptr : &*found;
}

std::optional<std::vector<DmlTable>> DmlTableFile::Parse(std::string_view text, std::string& error)
{
    pugi::xml_document document;
    pugi::xml_parse_result const parsed = document.load_buffer(text.data(), text.size(), pugi::parse_default, pugi::encoding_utf8);
    if (!parsed)
    {
        error = fmt::format("is not well-formed XML: {} at byte {}", parsed.description(), parsed.offset);
        return std::nullopt;
    }
    pugi::xml_node const root = document.document_element();
    pugi::xml_node const list = root.child(std::string(TableListElement).c_str());
    if (!list)
    {
        error = fmt::format("has no {} under its root element <{}>", TableListElement, root.name());
        return std::nullopt;
    }
    std::vector<DmlTable> tables;
    for (pugi::xml_node record = list.child(std::string(RecordElement).c_str()); record; record = record.next_sibling(std::string(RecordElement).c_str()))
    {
        std::string const name = Text(record.child(std::string(NameField).c_str()));
        pugi::xml_node const table = root.child(name.c_str());
        if (name.empty() || !table)
        {
            error = fmt::format("lists the table '{}', which it does not hold", name);
            return std::nullopt;
        }
        DmlTable read;
        read.Name = name;
        for (pugi::xml_node row = table.child(std::string(RecordElement).c_str()); row; row = row.next_sibling(std::string(RecordElement).c_str()))
            read.Records.push_back(ReadRecord(row));
        tables.push_back(std::move(read));
    }
    return tables;
}

DmlTable const* DmlTableFile::Find(std::vector<DmlTable> const& tables, std::string_view name) noexcept
{
    auto const found = std::find_if(tables.begin(), tables.end(), [name](DmlTable const& table) { return table.Name == name; });
    return found == tables.end() ? nullptr : &*found;
}
