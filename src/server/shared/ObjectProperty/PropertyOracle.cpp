/*
 * Project Ambrose by Imjustchico
 * Indexes every name by the 31 bits of its djb2 hash the property hash keeps, then answers a hash by subtracting each type's string hash and looking the rest up, a difference with its top bit set being no name's; guesses come back known ones first, then by type and name.
 */

#include "PropertyOracle.h"
#include "StringHash.h"

#include <algorithm>

namespace
{
    constexpr uint32 NameBits = 0x7FFFFFFFu;
}

PropertyOracle::PropertyOracle(TypeCatalog const& catalog, std::vector<std::string> const& extraNames, std::vector<std::string> const& extraTypes)
{
    for (ClassInfo const* type : catalog.GetClasses())
        for (PropertyInfo const& property : type->Properties)
        {
            AddType(property.TypeName);
            AddName(property.Name);
            _known.emplace(property.TypeName, property.Name);
        }
    for (std::string const& name : extraNames)
        AddName(name);
    for (std::string const& type : extraTypes)
        AddType(type);
}

void PropertyOracle::AddName(std::string const& name)
{
    if (name.empty() || !_names.insert(name).second)
        return;
    _namesByDjb2[StringHash::Djb2(name) & NameBits].push_back(name);
    ++_nameCount;
}

void PropertyOracle::AddType(std::string const& type)
{
    if (type.empty() || !_typeNames.insert(type).second)
        return;
    _types.emplace_back(type, StringHash::KiStringHash(type));
}

std::vector<PropertyGuess> PropertyOracle::Guess(uint32 hash) const
{
    std::vector<PropertyGuess> guesses;
    for (auto const& [type, typeHash] : _types)
    {
        uint32 const rest = hash - typeHash;
        if ((rest & ~NameBits) != 0)
            continue;
        auto const found = _namesByDjb2.find(rest);
        if (found == _namesByDjb2.end())
            continue;
        for (std::string const& name : found->second)
            guesses.push_back({ type, name, _known.contains({ type, name }) });
    }
    std::sort(guesses.begin(), guesses.end(), [](PropertyGuess const& left, PropertyGuess const& right)
    {
        if (left.Known != right.Known)
            return left.Known;
        return left.Type != right.Type ? left.Type < right.Type : left.Name < right.Name;
    });
    return guesses;
}
