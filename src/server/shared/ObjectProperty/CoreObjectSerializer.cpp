/*
 * Project Ambrose by Imjustchico
 * Builds the core object table a row at a time, naming every core type that is the plain byte or repeats, every class the catalog does not list or that is not a CoreObject, and every template class that is not a CoreTemplate, repeats or gives a core type no row builds, and hands back no table unless every row passed; the header an object made from a template carries is that template class's core type and template type with the template's id. The encode and decode calls bind the table into the serializer's options and leave the rest to it.
 */

#include "CoreObjectSerializer.h"

#include <fmt/format.h>

#include <algorithm>
#include <utility>

std::shared_ptr<CoreObjectTypeTable const> CoreObjectTypeTable::Build(std::vector<CoreObjectType> types, std::vector<CoreTemplateType> templates, TypeCatalog const& catalog,
    std::vector<std::string>& errors)
{
    std::size_t const before = errors.size();
    ClassInfo const* const core = catalog.FindClass(CoreObjectClass);
    if (!core)
        errors.push_back(fmt::format("the type dump does not list {}, so no class can be checked to be a game object", CoreObjectClass));
    ClassInfo const* const coreTemplate = catalog.FindClass(CoreTemplateClass);
    if (!coreTemplate && !templates.empty())
        errors.push_back(fmt::format("the type dump does not list {}, so no class can be checked to be a template", CoreTemplateClass));
    std::shared_ptr<CoreObjectTypeTable> table(new CoreObjectTypeTable());
    table->_types.reserve(types.size());
    for (CoreObjectType& entry : types)
    {
        if (entry.CoreType == 0)
        {
            errors.push_back(fmt::format("core type 0 is the byte that says a plain class hash follows, so it cannot stand for {}", entry.ClassName));
            continue;
        }
        std::string const where = fmt::format("core type {}", entry.CoreType);
        ClassInfo const* const type = catalog.FindClass(entry.ClassName);
        if (!type)
        {
            errors.push_back(fmt::format("{} names {}, which the type dump does not list", where, entry.ClassName));
            continue;
        }
        if (type->Kind != ClassKind::PropertyClass || (core && !type->IsA(*core)))
        {
            errors.push_back(fmt::format("{} names {}, which is not a {}", where, type->Name, CoreObjectClass));
            continue;
        }
        entry.ClassName = type->Name;
        entry.ClassHash = type->Hash;
        if (auto const [existing, inserted] = table->_byCoreType.emplace(entry.CoreType, table->_types.size()); !inserted)
        {
            errors.push_back(fmt::format("{} is listed for both {} and {}", where, table->_types[existing->second].ClassName, entry.ClassName));
            continue;
        }
        table->_types.push_back(std::move(entry));
    }
    table->_templates.reserve(templates.size());
    for (CoreTemplateType& entry : templates)
    {
        ClassInfo const* const type = catalog.FindClass(entry.TemplateClass);
        if (!type)
        {
            errors.push_back(fmt::format("template class {} is not listed in the type dump", entry.TemplateClass));
            continue;
        }
        if (type->Kind != ClassKind::PropertyClass || (coreTemplate && !type->IsA(*coreTemplate)))
        {
            errors.push_back(fmt::format("{} is not a {}", type->Name, CoreTemplateClass));
            continue;
        }
        if (!table->_byCoreType.contains(entry.CoreType))
        {
            errors.push_back(fmt::format("{} gives core type {}, which no row says a class for", type->Name, entry.CoreType));
            continue;
        }
        entry.TemplateClass = type->Name;
        entry.TemplateClassHash = type->Hash;
        if (auto const [existing, inserted] = table->_byTemplateClass.emplace(entry.TemplateClassHash, table->_templates.size()); !inserted)
        {
            errors.push_back(fmt::format("{} is listed twice", type->Name));
            continue;
        }
        table->_templates.push_back(std::move(entry));
    }
    if (errors.size() != before)
        return nullptr;
    return table;
}

CoreObjectType const* CoreObjectTypeTable::Find(uint8 coreType) const noexcept
{
    auto const found = _byCoreType.find(coreType);
    return found == _byCoreType.end() ? nullptr : &_types[found->second];
}

bool CoreObjectTypeTable::Builds(uint8 coreType, uint32 classHash) const noexcept
{
    CoreObjectType const* const found = Find(coreType);
    return found && found->ClassHash == classHash;
}

bool CoreObjectTypeTable::IsCoreClass(uint32 classHash) const noexcept
{
    return std::any_of(_types.begin(), _types.end(), [classHash](CoreObjectType const& type) { return type.ClassHash == classHash; });
}

CoreTemplateType const* CoreObjectTypeTable::FindTemplate(ClassInfo const& templateClass) const noexcept
{
    auto const found = _byTemplateClass.find(templateClass.Hash);
    return found == _byTemplateClass.end() ? nullptr : &_templates[found->second];
}

bool CoreObjectTypeTable::IsTemplatePair(uint8 coreType, uint8 templateType) const noexcept
{
    return std::any_of(_templates.begin(), _templates.end(), [coreType, templateType](CoreTemplateType const& entry)
    {
        return entry.CoreType == coreType && entry.TemplateType == templateType;
    });
}

std::optional<CoreObjectHeader> CoreObjectTypeTable::HeaderFor(ClassInfo const& templateClass, uint32 templateId) const noexcept
{
    CoreTemplateType const* const entry = FindTemplate(templateClass);
    if (!entry)
        return std::nullopt;
    return CoreObjectHeader{ entry->CoreType, entry->TemplateType, templateId };
}

EncodeResult CoreObjectSerializer::Encode(PropertyObject const& object, CoreObjectTypeTable const& types, SerializerOptions options)
{
    options.CoreObjects = &types;
    return ObjectSerializer::Encode(&object, options);
}

DecodeResult CoreObjectSerializer::Decode(TypeCatalogPtr const& catalog, std::span<uint8 const> bytes, CoreObjectTypeTable const& types, SerializerOptions options)
{
    options.CoreObjects = &types;
    return ObjectSerializer::Decode(catalog, bytes, options);
}

EncodeResult CoreObjectSerializer::EncodeField(ObjectField const& field, PropertyObject const& object, CoreObjectTypeTable const& types, SerializerOptions options)
{
    options.CoreObjects = &types;
    return ObjectSerializer::EncodeField(field, &object, options);
}

DecodeResult CoreObjectSerializer::DecodeField(TypeCatalogPtr const& catalog, ObjectField const& field, std::span<uint8 const> bytes, CoreObjectTypeTable const& types,
    SerializerOptions options)
{
    options.CoreObjects = &types;
    return ObjectSerializer::DecodeField(catalog, field, bytes, options);
}
