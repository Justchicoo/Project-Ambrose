/*
 * Project Ambrose by Imjustchico
 * Builds each proposal from the dump alone: the class the list it sits in holds is found by walking the path the sweep first saw it at, from its root class through each property's declared type, and where an object on the way is of a class that declares the property only in a class derived from the declared one, every such class of the dump must give the property the same class, which is the base, or PropertyClass where the object sits in no list; the base's properties come first as the dump gives them, then the class's own by hash, each copied from a property of the same type and name the dump lists or else read with the first container a trial has not refused, flagged Save because the files that hold it are written under the Save mask, and Enum or Bits when the dump's other properties of its type read their values that way. A class from the plain-XML files is refused when a BINd file holds it, since its property hashes prove its types there, when its name is not a class name or hashes to a class the dump has, when the dump properties holding it declare different classes, when an element holds text beside an object, more than one object or anything but an object, or objects in some places and text in others, or holds more distinct values than the sweep keeps; a value type is tried by reading every value the files give it as the XML reader would, through a property of the dump of that type with no value flags, or with the flags every property of that name shares when the type comes from the name, a name no property of the dump has, such as m_id.m_full, being read as its last field, .m_full, which every wrapped value of the dump gives one type.
 */

#include "ServerClassProposer.h"
#include "PropertyFlags.h"
#include "StringHash.h"
#include "StringUtil.h"
#include "XmlObjectReader.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <set>

namespace
{
    std::string Join(std::vector<std::string> const& names)
    {
        std::string joined;
        for (std::string const& name : names)
            joined += (joined.empty() ? "" : ", ") + name;
        return joined;
    }

    TypeDumpLoader::RawProperty FromDump(PropertyInfo const& property)
    {
        TypeDumpLoader::RawProperty raw;
        raw.Name = property.Name;
        raw.Type = property.TypeName;
        raw.Container = std::string(TypeKinds::GetName(property.Container));
        raw.Offset = property.Offset;
        raw.Flags = property.Flags;
        raw.Hash = property.Hash;
        raw.Dynamic = property.Dynamic;
        raw.Singleton = property.Singleton;
        raw.Pointer = property.Pointer;
        for (EnumOption const& option : property.Options)
            raw.Options.emplace_back(option.Name, option.Value);
        return raw;
    }

    PropertyInfo const* SameProperty(TypeCatalog const& catalog, std::string_view name, std::string_view type)
    {
        for (ClassInfo const* owner : catalog.GetClasses())
            if (PropertyInfo const* const property = owner->FindProperty(name); property && property->TypeName == type)
                return property;
        return nullptr;
    }

    PropertyInfo const* SameType(TypeCatalog const& catalog, std::string_view type)
    {
        for (ClassInfo const* owner : catalog.GetClasses())
            for (PropertyInfo const& property : owner->Properties)
                if (property.TypeName == type)
                    return &property;
        return nullptr;
    }

    bool IsPointer(std::string_view type)
    {
        return type.ends_with('*') || type.starts_with("class SharedPointer<");
    }

    constexpr std::array<std::string_view, 7> ValueTypes{ "bool", "int", "unsigned int", "unsigned __int64", "float", "class Color", "std::string" };

    uint32 ValueFlags() noexcept
    {
        return PropertyFlags::Bit(PropertyFlag::Enum) | PropertyFlags::Bit(PropertyFlag::Bits);
    }

    PropertyInfo const* PlainType(TypeCatalog const& catalog, std::string_view type)
    {
        for (ClassInfo const* owner : catalog.GetClasses())
            for (PropertyInfo const& property : owner->Properties)
                if (property.TypeName == type && (property.Flags & ValueFlags()) == 0 && property.Options.empty())
                    return &property;
        return nullptr;
    }

    bool ReadsEvery(PropertyInfo const& property, std::vector<std::string> const& values)
    {
        return std::all_of(values.begin(), values.end(), [&property](std::string const& value) { return XmlObjectReader::ReadsAs(property, value); });
    }

    bool IsTrueOrFalse(std::string const& value)
    {
        std::string_view const text = Ambrose::Trim(value);
        return Ambrose::EqualsIgnoreCase(text, "true") || Ambrose::EqualsIgnoreCase(text, "false");
    }

    PropertyInfo const* NamedType(TypeCatalog const& catalog, std::string_view name)
    {
        std::size_t const dot = name.rfind('.');
        std::string_view const field = dot == std::string_view::npos ? std::string_view() : name.substr(dot);
        for (bool const byField : { false, true })
        {
            if (byField && field.empty())
                break;
            std::set<std::pair<std::string, uint32>> types;
            PropertyInfo const* sample = nullptr;
            for (ClassInfo const* owner : catalog.GetClasses())
                for (PropertyInfo const& property : owner->Properties)
                    if (byField ? std::string_view(property.Name).ends_with(field) : property.Name == name)
                    {
                        types.emplace(property.TypeName, property.Flags & ValueFlags());
                        sample = &property;
                    }
            if (sample)
                return types.size() == 1 && sample->Kind != ValueKind::Object ? sample : nullptr;
        }
        return nullptr;
    }

    ClassInfo const* NearestCommon(TypeCatalog const& catalog, std::set<std::string> const& held)
    {
        std::vector<ClassInfo const*> classes;
        for (std::string const& name : held)
        {
            ClassInfo const* const found = catalog.FindClass(name);
            if (!found)
                return catalog.FindClass("class PropertyClass");
            classes.push_back(found);
        }
        std::vector<ClassInfo const*> chain{ classes.front() };
        chain.insert(chain.end(), classes.front()->Bases.begin(), classes.front()->Bases.end());
        for (ClassInfo const* candidate : chain)
            if (std::all_of(classes.begin(), classes.end(), [candidate](ClassInfo const* type) { return type->IsA(*candidate); }))
                return candidate;
        return nullptr;
    }
}

std::optional<std::string> ServerClassProposer::ElementClass(std::string_view typeName)
{
    std::string_view type = typeName;
    while (!type.empty() && (type.back() == '*' || type.back() == ' '))
        type.remove_suffix(1);
    constexpr std::string_view Shared = "class SharedPointer<";
    if (type.starts_with(Shared) && type.ends_with('>'))
        type = type.substr(Shared.size(), type.size() - Shared.size() - 1);
    if (!type.starts_with("class ") && !type.starts_with("struct "))
        return std::nullopt;
    return std::string(type);
}

std::string_view ServerClassProposer::LastProperty(std::string_view path)
{
    std::size_t const dot = path.rfind('.');
    if (dot == std::string_view::npos)
        return {};
    std::string_view last = path.substr(dot + 1);
    if (std::size_t const bracket = last.find('['); bracket != std::string_view::npos)
        last = last.substr(0, bracket);
    return last;
}

ClassInfo const* ServerClassProposer::HeldClass(TypeCatalog const& catalog, std::string_view path, std::string& problem)
{
    std::vector<std::string_view> segments;
    for (std::size_t start = 0; start <= path.size();)
    {
        std::size_t const dot = path.find('.', start);
        std::string_view segment = path.substr(start, dot == std::string_view::npos ? std::string_view::npos : dot - start);
        if (std::size_t const bracket = segment.find('['); bracket != std::string_view::npos)
            segment = segment.substr(0, bracket);
        segments.push_back(segment);
        if (dot == std::string_view::npos)
            break;
        start = dot + 1;
    }
    if (segments.size() < 2)
        return nullptr;
    ClassInfo const* current = catalog.FindClass(segments.front());
    if (!current)
    {
        problem = fmt::format("the root of the path it was first seen at, {}, is a class the dump does not describe", segments.front());
        return nullptr;
    }
    for (std::size_t index = 1; index < segments.size(); ++index)
    {
        std::string_view const name = segments[index];
        std::set<std::string> held;
        if (PropertyInfo const* const declared = current->FindProperty(name))
        {
            if (std::optional<std::string> element = ElementClass(declared->TypeName))
                held.insert(std::move(*element));
        }
        else
            for (ClassInfo const* derived : catalog.GetClasses())
                if (derived->IsA(*current))
                    if (PropertyInfo const* const property = derived->FindProperty(name))
                        if (std::optional<std::string> element = ElementClass(property->TypeName))
                            held.insert(std::move(*element));
        if (held.size() != 1)
        {
            problem = held.empty() ? fmt::format("no class of the dump derived from {} declares {}, on the path it was first seen at", current->Name, name)
                                   : fmt::format("the classes of the dump derived from {} give {}, on the path it was first seen at, {} different classes", current->Name, name, held.size());
            return nullptr;
        }
        current = catalog.FindClass(*held.begin());
        if (!current)
        {
            problem = fmt::format("{}, on the path it was first seen at, holds {}, which the dump does not describe", name, *held.begin());
            return nullptr;
        }
    }
    return current;
}

ServerClassProposals ServerClassProposer::Propose(TypeCatalog const& catalog, PropertyOracle const& oracle, ServerClassNames const& names, std::vector<ServerClassObservation> const& observed,
    ServerClassContainers const& containers)
{
    ServerClassProposals proposals;
    uint32 const saveFlag = PropertyFlags::Bit(PropertyFlag::Save);
    for (ServerClassObservation const& seen : observed)
    {
        auto const refuse = [&proposals, &seen](std::string reason) { proposals.Refused.push_back({ seen.Hash, std::move(reason) }); };
        if (catalog.FindClass(seen.Hash))
            continue;

        std::vector<std::string> named;
        bool written = true;
        if (auto const found = names.find(seen.Hash); found != names.end())
            for (ServerClassName const& name : found->second)
                if (StringHash::KiStringHash(name.Name) == seen.Hash)
                {
                    if (std::find(named.begin(), named.end(), name.Name) == named.end())
                        named.push_back(name.Name);
                    written = written && name.Written;
                }
        if (named.empty())
        {
            refuse("no name the client program's strings or the install's text files give hashes to it");
            continue;
        }
        if (named.size() > 1)
        {
            refuse(fmt::format("{} names the client program's strings and the install's text files give hash to it: {}", named.size(), Join(named)));
            continue;
        }

        std::map<uint32, PropertyGuess> guessed;
        std::string unnamed;
        for (uint32 const hash : seen.Properties)
        {
            std::vector<PropertyGuess> guesses = oracle.Guess(hash);
            if (guesses.size() > 1)
            {
                std::erase_if(guesses, [](PropertyGuess const& guess) { return !guess.Known; });
                if (guesses.size() != 1)
                    guesses.clear();
            }
            if (guesses.size() != 1)
            {
                unnamed = fmt::format("its property {} is named {} by the property oracle", hash, oracle.Guess(hash).empty() ? std::string("no way") : std::string("more than one way"));
                break;
            }
            guessed.emplace(hash, guesses.front());
        }
        if (!unnamed.empty())
        {
            refuse(std::move(unnamed));
            continue;
        }

        std::string_view const holder = LastProperty(seen.FirstPath);
        std::string walked;
        ClassInfo const* const bound = HeldClass(catalog, seen.FirstPath, walked);
        if (!walked.empty())
        {
            refuse(std::move(walked));
            continue;
        }

        ClassInfo const* const root = bound ? bound : catalog.FindClass("class PropertyClass");
        if (!root)
        {
            refuse("the dump describes no class PropertyClass for it to derive from");
            continue;
        }
        std::set<uint32> const holds(seen.Properties.begin(), seen.Properties.end());
        if (!std::all_of(root->Properties.begin(), root->Properties.end(), [&holds](PropertyInfo const& property) { return holds.contains(property.Hash); }))
        {
            refuse(fmt::format("it does not hold every property of {}, the class {} holds", root->Name, holder));
            continue;
        }
        ClassInfo const& base = *root;

        ServerClassProposal proposal;
        proposal.Class.Key = fmt::format("{}", seen.Hash);
        proposal.Class.Name = named.front();
        proposal.Class.Hash = seen.Hash;
        proposal.Class.Bases.push_back(base.Name);
        for (ClassInfo const* ancestor : base.Bases)
            proposal.Class.Bases.push_back(ancestor->Name);
        std::set<uint32> inherited;
        for (PropertyInfo const& property : base.Properties)
        {
            proposal.Class.Properties.push_back(FromDump(property));
            inherited.insert(property.Hash);
        }
        std::string refusal;
        std::size_t own = 0;
        for (auto const& [hash, guess] : guessed)
        {
            if (inherited.contains(hash))
                continue;
            ++own;
            TypeDumpLoader::RawProperty raw;
            if (PropertyInfo const* const same = SameProperty(catalog, guess.Name, guess.Type))
            {
                raw = FromDump(*same);
                raw.Offset = 0;
                raw.Flags = same->Flags | saveFlag;
            }
            else
            {
                proposal.Trials.insert(hash);
                auto const tried = containers.find({ seen.Hash, hash });
                std::string const container = tried == containers.end() ? std::string(FirstContainer) : tried->second;
                raw.Name = guess.Name;
                raw.Type = guess.Type;
                raw.Container = container;
                raw.Offset = 0;
                raw.Flags = saveFlag;
                if (PropertyInfo const* const sameType = SameType(catalog, guess.Type))
                    raw.Flags = *raw.Flags | (sameType->Flags & (PropertyFlags::Bit(PropertyFlag::Enum) | PropertyFlags::Bit(PropertyFlag::Bits)));
                raw.Hash = hash;
                raw.Dynamic = container != FirstContainer;
                raw.Singleton = false;
                raw.Pointer = IsPointer(guess.Type);
                if (guess.Type.starts_with("enum "))
                {
                    PropertyInfo const* const options = SameType(catalog, guess.Type);
                    if (!options || options->Options.empty())
                    {
                        refusal = fmt::format("its property {} is a {}, whose options the dump does not list", guess.Name, guess.Type);
                        break;
                    }
                    for (EnumOption const& option : options->Options)
                        raw.Options.emplace_back(option.Name, option.Value);
                }
            }
            proposal.Class.Properties.push_back(std::move(raw));
        }
        if (!refusal.empty())
        {
            refuse(std::move(refusal));
            continue;
        }
        for (std::size_t index = 0; index < proposal.Class.Properties.size(); ++index)
            proposal.Class.Properties[index].Id = index;
        proposal.Evidence = fmt::format("{} object(s) in {} file(s) of the install's archives hold it, the first in {} at {}. {} {}, which hashes to it. "
                                        "The property oracle names each of the {} properties they hold one way only. It derives from {}, {}, and holds every "
                                        "property {} declares, which come first, with its {} own after them.",
            seen.Count, seen.Files, seen.FirstFile, seen.FirstPath.empty() ? std::string("the root") : seen.FirstPath,
            written ? "A text file of the install's archives writes the class" : "The client program holds the string", named.front(), guessed.size(), base.Name,
            bound ? fmt::format("the class {}, the list it sits in, holds", holder) : std::string("since it sits in no list"), base.Name, own);
        proposals.Classes.push_back(std::move(proposal));
    }
    std::sort(proposals.Classes.begin(), proposals.Classes.end(), [](ServerClassProposal const& left, ServerClassProposal const& right) { return left.Class.Hash < right.Class.Hash; });
    return proposals;
}

ServerClassProposals ServerClassProposer::ProposeXml(TypeCatalog const& catalog, std::vector<XmlSweepClass> const& observed, std::set<uint32> const& inBinary)
{
    ServerClassProposals proposals;
    uint32 const saveFlag = PropertyFlags::Bit(PropertyFlag::Save);
    for (XmlSweepClass const& seen : observed)
    {
        if (catalog.FindClass(seen.Name))
            continue;
        uint32 const hash = StringHash::KiStringHash(seen.Name);
        auto const refuse = [&proposals, hash](std::string reason) { proposals.Refused.push_back({ hash, std::move(reason) }); };
        if (!seen.Name.starts_with("class ") && !seen.Name.starts_with("struct "))
        {
            refuse(fmt::format("the files name it '{}', which is not a class name", seen.Name));
            continue;
        }
        if (ClassInfo const* const clash = catalog.FindClass(hash))
        {
            refuse(fmt::format("its name hashes to {}, the hash of {}", hash, clash->Name));
            continue;
        }
        if (inBinary.contains(hash))
        {
            refuse("BINd files hold it too, whose property hashes prove its types, so it is proposed from them");
            continue;
        }

        std::set<std::string> declared;
        for (auto const& [holderClass, holderProperty] : seen.Holders)
            if (ClassInfo const* const owner = catalog.FindClass(holderClass))
                if (PropertyInfo const* const property = owner->FindProperty(holderProperty))
                    if (std::optional<std::string> element = ElementClass(property->TypeName))
                        declared.insert(std::move(*element));
        if (declared.size() > 1)
        {
            refuse(fmt::format("the dump properties holding it declare {} different classes: {}", declared.size(), Join(std::vector<std::string>(declared.begin(), declared.end()))));
            continue;
        }
        ClassInfo const* const base = catalog.FindClass(declared.empty() ? std::string("class PropertyClass") : *declared.begin());
        if (!base)
        {
            refuse(declared.empty() ? std::string("the dump describes no class PropertyClass for it to derive from")
                                    : fmt::format("the property holding it declares {}, which the dump does not describe", *declared.begin()));
            continue;
        }

        ServerClassProposal proposal;
        proposal.Class.Key = fmt::format("{}", hash);
        proposal.Class.Name = seen.Name;
        proposal.Class.Hash = hash;
        proposal.Class.Bases.push_back(base->Name);
        for (ClassInfo const* ancestor : base->Bases)
            proposal.Class.Bases.push_back(ancestor->Name);
        for (PropertyInfo const& property : base->Properties)
            proposal.Class.Properties.push_back(FromDump(property));

        std::string refusal;
        std::size_t objects = 0;
        std::size_t lists = 0;
        std::size_t byName = 0;
        std::size_t byValue = 0;
        for (XmlSweepProperty const& element : seen.Properties)
        {
            if (base->FindProperty(element.Name))
                continue;
            TypeDumpLoader::RawProperty raw;
            raw.Name = element.Name;
            raw.Container = std::string(element.Repeats || element.Keyed ? ListContainer : FirstContainer);
            raw.Offset = 0;
            raw.Flags = saveFlag;
            raw.Singleton = false;
            if (element.Mixed || (!element.Held.empty() && !element.Values.empty()))
            {
                refusal = fmt::format("its element <{}> holds text beside an object, more than one object or other elements, or objects in some places and text in others", element.Name);
                break;
            }
            if (!element.Held.empty())
            {
                ClassInfo const* const common = element.Held.size() == 1 ? nullptr : NearestCommon(catalog, element.Held);
                if (element.Held.size() > 1 && !common)
                {
                    refusal = fmt::format("its element <{}> holds objects of {} classes that derive from no one class", element.Name, element.Held.size());
                    break;
                }
                raw.Type = (common ? common->Name : *element.Held.begin()) + "*";
                raw.Pointer = true;
                ++objects;
            }
            else
            {
                if (element.Overflow)
                {
                    refusal = fmt::format("its element <{}> holds more than {} distinct values, more than the sweep keeps to check a type against", element.Name, XmlSweep::MaxValues);
                    break;
                }
                std::vector<std::string> values(element.Values.begin(), element.Values.end());
                if (element.Empty > 0)
                    values.emplace_back();
                PropertyInfo const* chosen = NamedType(catalog, element.Name);
                if (chosen && ReadsEvery(*chosen, values))
                    ++byName;
                else
                {
                    chosen = nullptr;
                    for (std::string_view const type : ValueTypes)
                    {
                        if (type == "bool" && !std::all_of(values.begin(), values.end(), IsTrueOrFalse))
                            continue;
                        PropertyInfo const* const sample = PlainType(catalog, type);
                        if (sample && ReadsEvery(*sample, values))
                        {
                            chosen = sample;
                            break;
                        }
                    }
                    if (!chosen)
                    {
                        refusal = fmt::format("no type the dump has reads every value of its element <{}>", element.Name);
                        break;
                    }
                    ++byValue;
                }
                raw.Type = chosen->TypeName;
                raw.Flags = saveFlag | (chosen->Flags & ValueFlags());
                for (EnumOption const& option : chosen->Options)
                    raw.Options.emplace_back(option.Name, option.Value);
                raw.Pointer = false;
            }
            if (*raw.Container != FirstContainer)
                ++lists;
            raw.Dynamic = *raw.Container != FirstContainer;
            raw.Hash = StringHash::PropertyHash(*raw.Type, raw.Name);
            proposal.Class.Properties.push_back(std::move(raw));
        }
        if (!refusal.empty())
        {
            refuse(std::move(refusal));
            continue;
        }
        for (std::size_t index = 0; index < proposal.Class.Properties.size(); ++index)
            proposal.Class.Properties[index].Id = index;
        proposal.Evidence = fmt::format("{} object(s) in {} plain-XML file(s) of the install's archives hold it, the first in {} at {}. No BINd file holds it and the type dump the client "
                                        "program registers does not describe it, so it is a class only the server reads, named as those files write it, and nothing in the install gives "
                                        "the types it was built with. It derives from {}, {}. Its {} own properties are the elements its objects hold, with the server's own types: {} hold "
                                        "objects and point to the class they are, {} take the one type the dump gives every property of that name, or of that field of a wrapped value, and {} the first of bool, int, "
                                        "unsigned int, unsigned __int64, float, Color and std::string that reads every value the files give them; {} are lists, because an object "
                                        "repeats them or writes them with a key.",
            seen.Count, seen.Files, seen.FirstFile, seen.FirstPath, base->Name,
            declared.empty() ? std::string("since no property of the dump holds it") : std::string("the class the dump property holding it declares"),
            objects + byName + byValue, objects, byName, byValue, lists);
        proposals.Classes.push_back(std::move(proposal));
    }
    std::sort(proposals.Classes.begin(), proposals.Classes.end(), [](ServerClassProposal const& left, ServerClassProposal const& right) { return left.Class.Hash < right.Class.Hash; });
    return proposals;
}
