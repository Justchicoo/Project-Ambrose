/*
 * Project Ambrose by Imjustchico
 * Names a property hash from what the loaded type dump already knows: every type string and every property name it lists, and any more a caller supplies, since a property's hash is the string hash of its type plus the djb2 hash of its name, so each type narrows a hash to the one djb2 value its name must have. A guess the dump lists as a real property of that type and name is marked known.
 */

#ifndef AMBROSE_PROPERTYORACLE_H
#define AMBROSE_PROPERTYORACLE_H

#include "TypeRegistry.h"
#include "Types.h"

#include <cstddef>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct PropertyGuess
{
    std::string Type;
    std::string Name;
    bool Known = false;

    bool operator==(PropertyGuess const&) const = default;
};

class PropertyOracle
{
public:
    explicit PropertyOracle(TypeCatalog const& catalog, std::vector<std::string> const& extraNames = {}, std::vector<std::string> const& extraTypes = {});

    std::vector<PropertyGuess> Guess(uint32 hash) const;
    std::size_t GetNameCount() const noexcept { return _nameCount; }
    std::size_t GetTypeCount() const noexcept { return _types.size(); }

private:
    void AddName(std::string const& name);
    void AddType(std::string const& type);

    std::vector<std::pair<std::string, uint32>> _types;
    std::set<std::string> _typeNames;
    std::unordered_map<uint32, std::vector<std::string>> _namesByDjb2;
    std::set<std::string> _names;
    std::set<std::pair<std::string, std::string>> _known;
    std::size_t _nameCount = 0;
};

#endif
