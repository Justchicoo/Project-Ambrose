/*
 * Project Ambrose by Imjustchico
 * Proposes a server class for each class the client's data holds and its type dump does not describe, from what a sweep saw of its objects: its name is the one name the client program's strings or the install's text files give that hashes to it, and its evidence says which, each property is the one type and name the property oracle gives its hash, its base is the one class the data proves, the class the list it sits in holds, read by walking the path it was first seen at from its root class through each property's declared type, or PropertyClass for an object that sits in no list, and it must hold every property of that base, since a class that shares a deeper class's properties may declare them itself, and a property keeps the container, flags and options the dump gives the same type and name elsewhere, and one the dump never lists is read with a trial container, which the proposal names, and with the value flags the dump gives its type; a class that cannot be named, based or typed that way is refused with the reason. ProposeXml does the same for a class only the install's plain-XML object files hold, which no BINd file and no type the client program registers describes, so nothing in the install gives the types it was built with and the server gives it its own: it is named as the files write it, derives from the class the dump property holding it declares, or PropertyClass, and each element its objects hold that its base does not declare is a property, a pointer to the class of the objects it holds, the nearest class they all derive from where they differ, and otherwise the one type the dump gives every property of that name with the same value flags, or of that field of a wrapped value where the dump has no such name, when every value reads as it, or else the first of bool, for values that are all true or false, int, unsigned int, unsigned __int64, float, Color and std::string that reads every value, an empty element being an empty value, and it is a list when an object repeats it or writes it with a key.
 */

#ifndef AMBROSE_SERVERCLASSPROPOSER_H
#define AMBROSE_SERVERCLASSPROPOSER_H

#include "PropertyOracle.h"
#include "TypeDumpLoader.h"
#include "TypeRegistry.h"
#include "Types.h"
#include "XmlSweep.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

struct ServerClassObservation
{
    uint32 Hash = 0;
    uint64 Count = 0;
    uint64 Files = 0;
    std::string FirstFile;
    std::string FirstPath;
    std::vector<uint32> Properties;
};

struct ServerClassProposal
{
    TypeDumpLoader::RawClass Class;
    std::string Evidence;
    std::set<uint32> Trials;
};

struct ServerClassRefusal
{
    uint32 Hash = 0;
    std::string Reason;
};

struct ServerClassProposals
{
    std::vector<ServerClassProposal> Classes;
    std::vector<ServerClassRefusal> Refused;
};

struct ServerClassName
{
    std::string Name;
    bool Written = false;
};

using ServerClassNames = std::unordered_map<uint32, std::vector<ServerClassName>>;
using ServerClassContainers = std::map<std::pair<uint32, uint32>, std::string>;

namespace ServerClassProposer
{
    inline constexpr std::string_view FirstContainer = "Static";
    inline constexpr std::string_view ListContainer = "List";

    std::optional<std::string> ElementClass(std::string_view typeName);
    std::string_view LastProperty(std::string_view path);
    ClassInfo const* HeldClass(TypeCatalog const& catalog, std::string_view path, std::string& problem);
    ServerClassProposals Propose(TypeCatalog const& catalog, PropertyOracle const& oracle, ServerClassNames const& names, std::vector<ServerClassObservation> const& observed,
        ServerClassContainers const& containers = {});
    ServerClassProposals ProposeXml(TypeCatalog const& catalog, std::vector<XmlSweepClass> const& observed, std::set<uint32> const& inBinary = {});
}

#endif
