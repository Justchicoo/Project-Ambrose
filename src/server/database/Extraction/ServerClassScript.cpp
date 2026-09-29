/*
 * Project Ambrose by Imjustchico
 * Deletes the install classes first, whose bases, properties and options go with them, then writes each class with the evidence its file carries, its bases in order, its properties by id and each enum property's options in the order its file lists them; a value the file leaves out is written as the dump would read it, a container as Static and a number as zero, and the same defaults stand for it when classes read back from the tables are compared with a file, class by class in hash order and property by property in id order.
 */

#include "ServerClassScript.h"

#include <algorithm>
#include <map>
#include <string>
#include <tuple>
#include <utility>

namespace
{
    using Option = std::pair<std::string, int64>;
    using PropertyRow = std::tuple<uint64, std::string, std::string, uint64, std::string, uint64, uint64, bool, bool, bool, std::vector<Option>>;
    using ClassRow = std::tuple<std::string, std::vector<std::string>, std::vector<PropertyRow>>;

    std::map<uint64, ClassRow> Rows(TypeDumpLoader::RawDump const& classes)
    {
        std::map<uint64, ClassRow> rows;
        for (TypeDumpLoader::RawClass const& type : classes.Classes)
        {
            std::vector<PropertyRow> properties;
            for (TypeDumpLoader::RawProperty const& property : type.Properties)
            {
                std::vector<Option> options;
                for (auto const& [name, value] : property.Options)
                    options.emplace_back(name, std::holds_alternative<int64>(value) ? std::get<int64>(value) : 0);
                properties.emplace_back(uint64{ property.Id.value_or(0) }, property.Name, property.Type.value_or(""), uint64{ property.Hash.value_or(0) }, property.Container.value_or("Static"),
                    uint64{ property.Offset.value_or(0) }, uint64{ property.Flags.value_or(0) }, property.Dynamic.value_or(false), property.Singleton.value_or(false),
                    property.Pointer.value_or(false), std::move(options));
            }
            std::sort(properties.begin(), properties.end(), [](PropertyRow const& left, PropertyRow const& right) { return std::get<0>(left) < std::get<0>(right); });
            rows.emplace(type.Hash.value_or(0), ClassRow{ type.Name.value_or(""), type.Bases, std::move(properties) });
        }
        return rows;
    }
}

bool ServerClassScript::Matches(TypeDumpLoader::RawDump const& written, TypeDumpLoader::RawDump const& classes)
{
    return Rows(written) == Rows(classes);
}

WorldSqlScript ServerClassScript::Build(TypeDumpLoader::RawDump const& classes)
{
    auto const flag = [](std::optional<bool> const& value) { return WorldSqlScript::Value{ uint64{ value.value_or(false) ? 1u : 0u } }; };
    std::vector<WorldSqlScript::Row> rows;
    std::vector<WorldSqlScript::Row> bases;
    std::vector<WorldSqlScript::Row> properties;
    std::vector<WorldSqlScript::Row> options;
    for (TypeDumpLoader::RawClass const& type : classes.Classes)
    {
        uint64 const hash = type.Hash.value_or(0);
        rows.push_back({ hash, type.Name.value_or(""), std::string(InstallSource), type.Evidence.value_or("") });
        for (std::size_t position = 0; position < type.Bases.size(); ++position)
            bases.push_back({ hash, uint64{ position }, type.Bases[position] });
        for (TypeDumpLoader::RawProperty const& property : type.Properties)
        {
            uint64 const id = property.Id.value_or(0);
            properties.push_back({ hash, id, property.Name, property.Type.value_or(""), property.Hash.value_or(0), property.Container.value_or("Static"), property.Offset.value_or(0),
                property.Flags.value_or(0), flag(property.Dynamic), flag(property.Singleton), flag(property.Pointer) });
            for (std::size_t position = 0; position < property.Options.size(); ++position)
            {
                auto const& [name, value] = property.Options[position];
                int64 const number = std::holds_alternative<int64>(value) ? std::get<int64>(value) : 0;
                options.push_back({ hash, id, uint64{ position }, name, number });
            }
        }
    }

    std::vector<std::string_view> const tables = GetTables();
    WorldSqlScript script;
    script.ReplaceRows(tables[0], "source", std::string(InstallSource), { "hash", "name", "source", "evidence" }, rows);
    script.InsertRows(tables[1], { "class_hash", "position", "base_name" }, bases);
    script.InsertRows(tables[2], { "class_hash", "property_id", "name", "type", "hash", "container", "offset", "flags", "dynamic", "singleton", "pointer" }, properties);
    script.InsertRows(tables[3], { "class_hash", "property_id", "position", "name", "value" }, options);
    return script;
}

std::vector<std::string_view> ServerClassScript::GetTables()
{
    return { "server_class", "server_class_base", "server_class_property", "server_class_property_option" };
}
