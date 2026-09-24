/*
 * Project Ambrose by Imjustchico
 * Keeps the registered typed views, the built-in ones first, refusing new views once a type dump load has sealed the list, and binds a view to a catalog: its class must be a listed property class, and each field must name a property of that class with the field's dump type and be stored as the C++ type the field declares, every mismatch reported with the view's name.
 */

#include "TypedView.h"
#include "ObjectViews.h"

#include <fmt/format.h>

#include <algorithm>

TypedViewRegistry& TypedViewRegistry::Instance()
{
    static TypedViewRegistry& instance = []() -> TypedViewRegistry&
    {
        static TypedViewRegistry registry;
        ObjectViews::RegisterAll(registry);
        return registry;
    }();
    return instance;
}

bool TypedViewRegistry::Add(ViewDefinition const& definition)
{
    std::lock_guard const lock(_mutex);
    if (_sealed)
        return false;
    if (std::find(_views.begin(), _views.end(), &definition) == _views.end())
        _views.push_back(&definition);
    return true;
}

std::vector<ViewDefinition const*> TypedViewRegistry::GetViews() const
{
    std::lock_guard const lock(_mutex);
    return _views;
}

std::vector<ViewDefinition const*> TypedViewRegistry::Seal()
{
    std::lock_guard const lock(_mutex);
    _sealed = true;
    return _views;
}

bool TypedViewRegistry::IsSealed() const
{
    std::lock_guard const lock(_mutex);
    return _sealed;
}

bool TypedViewRegistry::Bind(TypeCatalog const& catalog, ViewDefinition const& definition, ViewBinding& binding, std::vector<std::string>& errors)
{
    ClassInfo const* const type = catalog.FindClass(definition.ClassName);
    if (!type || type->Kind != ClassKind::PropertyClass)
    {
        errors.push_back(fmt::format("view {} names {}, which the type dump does not list as a property class", definition.Name, definition.ClassName));
        return false;
    }
    std::size_t const before = errors.size();
    binding.Definition = &definition;
    binding.Class = type;
    binding.Ordinals.clear();
    binding.Ordinals.reserve(definition.Fields.size());
    for (ViewField const& field : definition.Fields)
    {
        PropertyInfo const* const property = type->FindProperty(field.Hash);
        if (!property || property->Name != field.Name)
        {
            PropertyInfo const* const named = type->FindProperty(field.Name);
            if (named)
                errors.push_back(fmt::format("view {}: {} property {} has type {}, not {}", definition.Name, type->Name, field.Name, named->TypeName, field.TypeName));
            else
                errors.push_back(fmt::format("view {}: {} has no property {}", definition.Name, type->Name, field.Name));
            continue;
        }
        if (PropertyObject::StorageIndexOf(*property) != field.StorageIndex)
        {
            errors.push_back(fmt::format("view {}: {} property {} of type {} is not stored as the C++ type the field declares", definition.Name, type->Name, field.Name, field.TypeName));
            continue;
        }
        binding.Ordinals.push_back(property->Id);
    }
    return errors.size() == before;
}
