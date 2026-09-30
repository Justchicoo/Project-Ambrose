/*
 * Project Ambrose by Imjustchico
 * Reads server_class, its bases, properties and options in one snapshot of the world database, keyed by hash as the dump keys a class, where a base list with a gap or a row for a class the read did not take is a refusal unless one source was asked for, whose other classes' rows are passed over. Deletes the install classes first, whose bases, properties and options go with them, then writes each class with the evidence its file carries, its bases in order, its properties by id and each enum property's options in the order its file lists them; a value the file leaves out is written as the dump would read it, a container as Static and a number as zero, and the same defaults stand for it when classes read back from the tables are compared with a file, class by class in hash order and property by property in id order.
 */

#include "ServerClassScript.h"
#include "DatabaseEnv.h"
#include "WorldDatabase.h"

#include <fmt/format.h>

#include <algorithm>
#include <map>
#include <string>
#include <tuple>
#include <utility>

namespace
{
    bool ReadClassRows(PreparedResultSet* classes, PreparedResultSet* bases, PreparedResultSet* properties, PreparedResultSet* options, TypeDumpLoader::RawDump& dump,
        std::vector<std::string>& errors, bool selected = false)
    {
        std::size_t const before = errors.size();
        std::map<uint32, TypeDumpLoader::RawClass> byHash;
        if (classes && classes->GetRowCount() > 0)
        {
            do
            {
                Field const* const row = classes->Fetch();
                uint32 const hash = row[0].Get<uint32>();
                TypeDumpLoader::RawClass type;
                type.Key = fmt::format("{}", hash);
                type.Name = row[1].Get<std::string>();
                type.Hash = hash;
                type.Source = row[2].Get<std::string>();
                type.Evidence = row[3].Get<std::string>();
                byHash.emplace(hash, std::move(type));
            } while (classes->NextRow());
        }
        if (bases && bases->GetRowCount() > 0)
        {
            do
            {
                Field const* const row = bases->Fetch();
                uint32 const hash = row[0].Get<uint32>();
                uint32 const position = row[1].Get<uint32>();
                auto const found = byHash.find(hash);
                if (found == byHash.end())
                {
                    if (!selected)
                        errors.push_back(fmt::format("server_class_base gives a base to class hash {}, which server_class does not hold", hash));
                    continue;
                }
                std::vector<std::string>& list = found->second.Bases;
                if (position != list.size())
                {
                    errors.push_back(fmt::format("server_class_base gives {} a base at position {} where position {} comes next", *found->second.Name, position, list.size()));
                    continue;
                }
                list.push_back(row[2].Get<std::string>());
            } while (bases->NextRow());
        }
        if (properties && properties->GetRowCount() > 0)
        {
            do
            {
                Field const* const row = properties->Fetch();
                uint32 const hash = row[0].Get<uint32>();
                auto const found = byHash.find(hash);
                if (found == byHash.end())
                {
                    if (!selected)
                        errors.push_back(fmt::format("server_class_property gives a property to class hash {}, which server_class does not hold", hash));
                    continue;
                }
                TypeDumpLoader::RawProperty property;
                property.Id = row[1].Get<uint32>();
                property.Name = row[2].Get<std::string>();
                property.Type = row[3].Get<std::string>();
                property.Hash = row[4].Get<uint32>();
                property.Container = row[5].Get<std::string>();
                property.Offset = row[6].Get<uint32>();
                property.Flags = row[7].Get<uint32>();
                property.Dynamic = row[8].Get<bool>();
                property.Singleton = row[9].Get<bool>();
                property.Pointer = row[10].Get<bool>();
                found->second.Properties.push_back(std::move(property));
            } while (properties->NextRow());
        }
        if (options && options->GetRowCount() > 0)
        {
            do
            {
                Field const* const row = options->Fetch();
                uint32 const hash = row[0].Get<uint32>();
                uint32 const id = row[1].Get<uint32>();
                auto const found = byHash.find(hash);
                TypeDumpLoader::RawProperty* property = nullptr;
                if (found != byHash.end())
                    for (TypeDumpLoader::RawProperty& candidate : found->second.Properties)
                        if (candidate.Id == id)
                            property = &candidate;
                if (!property)
                {
                    if (!selected || found != byHash.end())
                        errors.push_back(fmt::format("server_class_property_option gives an option to property {} of class hash {}, which server_class_property does not hold", id, hash));
                    continue;
                }
                property->Options.emplace_back(row[3].Get<std::string>(), row[4].Get<int64>());
            } while (options->NextRow());
        }
        dump.Version = TypeDumpLoader::SupportedVersion;
        dump.HasClasses = true;
        for (auto& [hash, type] : byHash)
            dump.Classes.push_back(std::move(type));
        return errors.size() == before;
    }

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

bool ServerClassScript::IsAuthored(TypeDumpLoader::RawClass const& type)
{
    return type.Source == AuthoredSource;
}

TypeDumpLoader::RawDump ServerClassScript::InstallClasses(TypeDumpLoader::RawDump const& classes)
{
    TypeDumpLoader::RawDump install;
    install.Version = classes.Version;
    install.HasClasses = classes.HasClasses;
    for (TypeDumpLoader::RawClass const& type : classes.Classes)
        if (!IsAuthored(type))
            install.Classes.push_back(type);
    return install;
}

bool ServerClassScript::Read(TypeDumpLoader::RawDump& dump, std::vector<std::string>& errors, std::optional<std::string_view> source)
{
    WorldDatabaseStatements const which = !source ? WORLD_SEL_SERVER_CLASSES : *source == AuthoredSource ? WORLD_SEL_SERVER_CLASSES_AUTHORED : WORLD_SEL_SERVER_CLASSES_INSTALLED;
    if (source && *source != AuthoredSource && *source != InstallSource)
    {
        errors.push_back(fmt::format("server_class has no source {}", *source));
        return false;
    }
    auto const classes = WorldDatabase.IsOpen() ? WorldDatabase.GetPreparedStatement(which) : nullptr;
    auto const bases = WorldDatabase.IsOpen() ? WorldDatabase.GetPreparedStatement(WORLD_SEL_SERVER_CLASS_BASES) : nullptr;
    auto const properties = WorldDatabase.IsOpen() ? WorldDatabase.GetPreparedStatement(WORLD_SEL_SERVER_CLASS_PROPERTIES) : nullptr;
    auto const options = WorldDatabase.IsOpen() ? WorldDatabase.GetPreparedStatement(WORLD_SEL_SERVER_CLASS_PROPERTY_OPTIONS) : nullptr;
    if (!classes || !bases || !properties || !options)
    {
        errors.emplace_back("the world database is not open, so the classes the type dump does not describe cannot be read");
        return false;
    }
    std::vector<PreparedQueryResult> results;
    if (!WorldDatabase.QuerySnapshot({ classes.get(), bases.get(), properties.get(), options.get() }, results))
    {
        errors.emplace_back("server_class, server_class_base, server_class_property and server_class_property_option cannot be read from the world database");
        return false;
    }
    return ReadClassRows(results[0].get(), results[1].get(), results[2].get(), results[3].get(), dump, errors, source.has_value());
}

bool ServerClassScript::Matches(TypeDumpLoader::RawDump const& written, TypeDumpLoader::RawDump const& classes)
{
    return Rows(written) == Rows(InstallClasses(classes));
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
        if (IsAuthored(type))
            continue;
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
