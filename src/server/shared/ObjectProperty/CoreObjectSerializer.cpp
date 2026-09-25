/*
 * Project Ambrose by Imjustchico
 * Builds the core object table a pair at a time, naming every pair that is plain, repeats, names a class the catalog does not list or a class that is not a CoreObject, and every class given two pairs, and hands back no table unless every row passed; the encode and decode calls bind the table into the serializer's options and leave the rest to it.
 */

#include "CoreObjectSerializer.h"

#include <fmt/format.h>

#include <utility>

std::shared_ptr<CoreObjectTypeTable const> CoreObjectTypeTable::Build(std::vector<CoreObjectType> types, TypeCatalog const& catalog, std::vector<std::string>& errors)
{
    std::size_t const before = errors.size();
    ClassInfo const* const core = catalog.FindClass(CoreObjectClass);
    if (!core)
        errors.push_back(fmt::format("the type dump does not list {}, so no class can be checked to be a game object", CoreObjectClass));
    std::shared_ptr<CoreObjectTypeTable> table(new CoreObjectTypeTable());
    table->_types.reserve(types.size());
    for (CoreObjectType& entry : types)
    {
        std::string const where = fmt::format("block {} type {}", entry.Block, entry.Type);
        if (entry.Block == 0 && entry.Type == 0)
        {
            errors.push_back(fmt::format("{} is the pair that says a plain class hash follows, so it cannot stand for {}", where, entry.ClassName));
            continue;
        }
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
        std::size_t const index = table->_types.size();
        uint16 const key = PairKey(entry.Block, entry.Type);
        if (auto const [existing, inserted] = table->_byPair.emplace(key, index); !inserted)
        {
            errors.push_back(fmt::format("{} is listed for both {} and {}", where, table->_types[existing->second].ClassName, entry.ClassName));
            continue;
        }
        if (auto const [existing, inserted] = table->_byClass.emplace(entry.ClassHash, index); !inserted)
        {
            table->_byPair.erase(key);
            CoreObjectType const& first = table->_types[existing->second];
            errors.push_back(fmt::format("{} is given both block {} type {} and {}", entry.ClassName, first.Block, first.Type, where));
            continue;
        }
        table->_types.push_back(std::move(entry));
    }
    if (errors.size() != before)
        return nullptr;
    return table;
}

CoreObjectType const* CoreObjectTypeTable::Find(uint8 block, uint8 type) const noexcept
{
    auto const found = _byPair.find(PairKey(block, type));
    return found == _byPair.end() ? nullptr : &_types[found->second];
}

CoreObjectType const* CoreObjectTypeTable::FindByClass(uint32 classHash) const noexcept
{
    auto const found = _byClass.find(classHash);
    return found == _byClass.end() ? nullptr : &_types[found->second];
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
