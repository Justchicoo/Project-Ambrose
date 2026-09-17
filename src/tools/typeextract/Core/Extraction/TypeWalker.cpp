/*
 * Project Ambrose by Imjustchico
 * Walks the client's registered types in guest memory into a format v2 dump model, calling each container's own name and dynamic methods, and checks every type hash, property hash, id order, flag byte, container and vector bound and that every class, base, property, type and option name and option text is valid UTF-8, counting problems by kind.
 */

#include "TypeWalker.h"
#include "GuestProcess.h"
#include "StringHash.h"
#include "Utf.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace
{
    constexpr std::string_view StaticContainer = "Static";
    constexpr std::array<std::string_view, 3> KnownContainers = { "Static", "Vector", "List" };
    constexpr std::string_view InvalidText = "invalid text";

    std::string Escaped(std::string_view text)
    {
        std::string escaped;
        escaped.reserve(text.size());
        for (char const c : text)
        {
            uint8 const byte = static_cast<uint8>(c);
            if (c == '\\')
                escaped += "\\\\";
            else if (byte < 0x20 || byte >= 0x7F)
                escaped += fmt::format("\\x{:02X}", byte);
            else
                escaped += c;
        }
        return escaped;
    }

    struct WalkState
    {
        GuestProcess& process;
        Machine& machine;
        ClientLayout const& layout;
        TypeWalkResult& result;
        std::unordered_map<uint64, std::pair<std::string, bool>> containers;

        void Problem(std::string const& kind, std::string detail)
        {
            ++result.ProblemCounts[kind];
            std::vector<std::string>& samples = result.ProblemSamples[kind];
            if (samples.size() < TypeWalkResult::SamplesPerProblem)
                samples.push_back(std::move(detail));
        }

        std::string ReadString(uint64 address)
        {
            uint64 const length = machine.ReadU64(address + layout.StringSize);
            uint64 const capacity = machine.ReadU64(address + layout.StringCapacity);
            if (length > TypeWalker::MaxStringLength || capacity < length)
                throw EmulationError(fmt::format("no string at {:#x} (length {}, capacity {})", address, length, capacity));
            uint64 const text = capacity > layout.StringInlineCapacity ? machine.ReadU64(address) : address;
            std::vector<uint8> const bytes = machine.ReadBytes(text, static_cast<std::size_t>(length));
            return std::string(bytes.begin(), bytes.end());
        }

        std::pair<std::string, bool> const& Container(uint64 container)
        {
            uint64 const vtable = machine.ReadU64(container);
            auto found = containers.find(vtable);
            if (found != containers.end())
                return found->second;
            uint64 const nameFunction = machine.ReadU64(vtable + 8 * layout.ContainerNameSlot);
            uint64 const dynamicFunction = machine.ReadU64(vtable + 8 * layout.ContainerDynamicSlot);
            std::array<uint64, 1> const arguments{ container };
            uint64 const namePointer = process.Call(nameFunction, arguments, TypeWalker::ContainerCallBudget);
            std::optional<std::string> name = machine.ReadCString(namePointer, 256);
            if (!name)
                throw EmulationError(fmt::format("the container at {:#x} returned no readable name", container));
            bool const dynamic = (process.Call(dynamicFunction, arguments, TypeWalker::ContainerCallBudget) & 0xFF) != 0;
            return containers.emplace(vtable, std::pair<std::string, bool>{ std::move(*name), dynamic }).first->second;
        }

        std::variant<int64, std::string> OptionValue(std::string const& text)
        {
            std::size_t start = 0;
            bool const negative = !text.empty() && text[0] == '-';
            if (negative)
                start = 1;
            if (start >= text.size())
                return text;
            uint64 modular = 0;
            for (std::size_t i = start; i < text.size(); ++i)
            {
                if (text[i] < '0' || text[i] > '9')
                    return text;
                modular = (modular * 10 + static_cast<uint64>(text[i] - '0')) & 0xFFFFFFFFu;
            }
            if (negative)
                modular = (0x100000000ull - modular) & 0xFFFFFFFFu;
            return static_cast<int64>(modular);
        }

        void Type(uint64 type)
        {
            std::string const name = ReadString(type + layout.TypeName);
            if (!Utf::IsValidUtf8(name))
                Problem(std::string(InvalidText), fmt::format("the class name {} is not valid UTF-8", Escaped(name)));
            uint32 const hash = machine.ReadU32(type + layout.TypeHash);
            if (StringHash::KiStringHash(name) != hash)
                Problem("type hash", fmt::format("{} hashes to {}, but the client stores {}", name, StringHash::KiStringHash(name), hash));
            uint8 const pointerFlag = machine.ReadU8(type + layout.TypePointer);
            if (pointerFlag > 1)
                Problem("type pointer flag", fmt::format("{} has pointer flag {}", name, pointerFlag));

            TypeDumpLoader::RawClass& entry = result.Dump.Classes.emplace_back();
            entry.Key = std::to_string(hash);
            entry.Name = name;
            entry.Hash = hash;
            uint64 const list = machine.ReadU64(type + layout.TypePropertyList);
            if (!list)
                return;

            std::string const listName = ReadString(list + layout.ListName);
            if (listName != TypeWalker::ListNameOf(name))
                Problem("property list name", fmt::format("{} has the property list {}", name, listName));
            uint8 const singleton = machine.ReadU8(list + layout.ListSingleton);
            if (singleton > 1)
                Problem("singleton flag", fmt::format("{} has singleton flag {}", name, singleton));

            uint64 base = machine.ReadU64(list + layout.ListBase);
            for (std::size_t depth = 0; base; ++depth)
            {
                if (depth >= TypeWalker::MaxBaseDepth)
                {
                    Problem("base chain", fmt::format("{} has more than {} bases", name, TypeWalker::MaxBaseDepth));
                    break;
                }
                std::string baseName = ReadString(base + layout.ListName);
                for (std::string_view const prefix : { std::string_view("class "), std::string_view("struct ") })
                    if (baseName.starts_with(prefix))
                        baseName.erase(0, prefix.size());
                if (!Utf::IsValidUtf8(baseName))
                    Problem(std::string(InvalidText), fmt::format("{} names the base {}, which is not valid UTF-8", Escaped(name), Escaped(baseName)));
                entry.Bases.push_back(std::move(baseName));
                base = machine.ReadU64(base + layout.ListBase);
            }

            uint64 const begin = machine.ReadU64(list + layout.ListProperties);
            uint64 const end = machine.ReadU64(list + layout.ListProperties + 8);
            if (end < begin || (end - begin) % layout.ListEntrySize != 0 || (end - begin) / layout.ListEntrySize > TypeWalker::MaxProperties)
            {
                Problem("property vector", fmt::format("{} has the property vector {:#x}-{:#x}", name, begin, end));
                return;
            }
            uint64 index = 0;
            for (uint64 slot = begin; slot < end; slot += layout.ListEntrySize, ++index)
            {
                uint64 const property = machine.ReadU64(slot);
                uint64 const propertyType = machine.ReadU64(property + layout.PropertyType);
                std::string const typeName = ReadString(propertyType + layout.TypeName);
                std::optional<std::string> const propertyName = machine.ReadCString(machine.ReadU64(property + layout.PropertyName), 4096);
                if (!propertyName)
                {
                    Problem("property name", fmt::format("{} property {} has no readable name", name, index));
                    continue;
                }
                if (!Utf::IsValidUtf8(*propertyName))
                    Problem(std::string(InvalidText), fmt::format("{} has the property name {}, which is not valid UTF-8", Escaped(name), Escaped(*propertyName)));
                if (!Utf::IsValidUtf8(typeName))
                    Problem(std::string(InvalidText), fmt::format("{}.{} has the type {}, which is not valid UTF-8", Escaped(name), Escaped(*propertyName), Escaped(typeName)));
                uint32 const propertyHash = machine.ReadU32(property + layout.PropertyHash);
                if (StringHash::PropertyHash(typeName, *propertyName) != propertyHash)
                    Problem("property hash", fmt::format("{}.{} of type {} hashes to {}, but the client stores {}", name, *propertyName, typeName, StringHash::PropertyHash(typeName, *propertyName), propertyHash));
                uint32 const id = machine.ReadU32(property + layout.PropertyId);
                if (id != index)
                    Problem("property id", fmt::format("{}.{} has id {} at position {}", name, *propertyName, id, index));
                auto const& [containerName, dynamic] = Container(machine.ReadU64(property + layout.PropertyContainer));
                bool const known = std::find(KnownContainers.begin(), KnownContainers.end(), containerName) != KnownContainers.end();
                if (!known || (containerName == StaticContainer) == dynamic)
                    Problem("container", fmt::format("{}.{} has the container {} with dynamic {}", name, *propertyName, containerName, dynamic));

                TypeDumpLoader::RawProperty& out = entry.Properties.emplace_back();
                out.Name = *propertyName;
                out.Type = typeName;
                out.Container = containerName;
                out.Id = id;
                out.Offset = machine.ReadU32(property + layout.PropertyOffset);
                out.Flags = machine.ReadU32(property + layout.PropertyFlags);
                out.Hash = propertyHash;
                out.Dynamic = dynamic;
                out.Singleton = singleton != 0;
                out.Pointer = machine.ReadU8(propertyType + layout.TypePointer) != 0;
                ++result.PropertyCount;

                uint64 const optionsBegin = machine.ReadU64(property + layout.PropertyOptions);
                uint64 const optionsEnd = machine.ReadU64(property + layout.PropertyOptions + 8);
                if (!optionsBegin && !optionsEnd)
                    continue;
                if (optionsEnd < optionsBegin || (optionsEnd - optionsBegin) % layout.OptionSize != 0 || (optionsEnd - optionsBegin) / layout.OptionSize > TypeWalker::MaxOptions)
                {
                    Problem("option vector", fmt::format("{}.{} has the option vector {:#x}-{:#x}", name, *propertyName, optionsBegin, optionsEnd));
                    continue;
                }
                for (uint64 option = optionsBegin; option < optionsEnd; option += layout.OptionSize)
                {
                    std::string optionName = ReadString(option + layout.OptionName);
                    std::string const optionText = ReadString(option + layout.OptionValue);
                    if (!Utf::IsValidUtf8(optionName))
                        Problem(std::string(InvalidText), fmt::format("{}.{} has the option name {}, which is not valid UTF-8", Escaped(name), Escaped(*propertyName), Escaped(optionName)));
                    if (!Utf::IsValidUtf8(optionText))
                        Problem(std::string(InvalidText), fmt::format("{}.{} option {} has the text {}, which is not valid UTF-8", Escaped(name), Escaped(*propertyName), Escaped(optionName), Escaped(optionText)));
                    out.Options.emplace_back(std::move(optionName), OptionValue(optionText));
                }
            }
        }
    };
}

TypeWalker::TypeWalker(GuestProcess& process, ClientLayout layout) : _process(process), _layout(layout)
{
}

TypeWalkResult TypeWalker::Walk(std::span<uint64 const> types)
{
    TypeWalkResult result;
    result.Dump.Version = TypeDumpLoader::SupportedVersion;
    result.Dump.HasClasses = true;
    WalkState walk{ _process, _process.GetMachine(), _layout, result, {} };
    for (uint64 const type : types)
    {
        try
        {
            walk.Type(type);
        }
        catch (EmulationError const& error)
        {
            walk.Problem("unreadable type", fmt::format("the type at {:#x}: {}", type, error.what()));
        }
    }

    auto sameProperty = [](TypeDumpLoader::RawProperty const& a, TypeDumpLoader::RawProperty const& b)
    {
        return a.Name == b.Name && a.Type == b.Type && a.Container == b.Container && a.Id == b.Id && a.Offset == b.Offset && a.Flags == b.Flags && a.Hash == b.Hash
            && a.Dynamic == b.Dynamic && a.Singleton == b.Singleton && a.Pointer == b.Pointer && a.Options == b.Options;
    };
    std::vector<TypeDumpLoader::RawClass> unique;
    std::unordered_map<std::string, std::size_t> byKey;
    unique.reserve(result.Dump.Classes.size());
    result.PropertyCount = 0;
    for (TypeDumpLoader::RawClass& entry : result.Dump.Classes)
    {
        for (TypeDumpLoader::RawProperty& property : entry.Properties)
        {
            std::vector<std::pair<std::string, std::variant<int64, std::string>>> options;
            std::unordered_map<std::string, std::size_t> seen;
            for (auto& option : property.Options)
            {
                auto const found = seen.find(option.first);
                if (found == seen.end())
                {
                    seen.emplace(option.first, options.size());
                    options.push_back(std::move(option));
                    continue;
                }
                ++result.DuplicateOptions;
                if (options[found->second].second != option.second)
                    walk.Problem("conflicting enum option", fmt::format("{}.{} lists the option {} twice with different values", entry.Name.value_or(entry.Key), property.Name, option.first));
            }
            property.Options = std::move(options);
        }
        auto const found = byKey.find(entry.Key);
        if (found == byKey.end())
        {
            byKey.emplace(entry.Key, unique.size());
            result.PropertyCount += entry.Properties.size();
            unique.push_back(std::move(entry));
            continue;
        }
        ++result.DuplicateTypes;
        TypeDumpLoader::RawClass const& kept = unique[found->second];
        bool const same = kept.Name == entry.Name && kept.Hash == entry.Hash && kept.Bases == entry.Bases && kept.Properties.size() == entry.Properties.size()
            && std::equal(kept.Properties.begin(), kept.Properties.end(), entry.Properties.begin(), sameProperty);
        if (!same)
            walk.Problem("conflicting duplicate type", fmt::format("{} is registered twice with different contents", entry.Name.value_or(entry.Key)));
    }
    result.Dump.Classes = std::move(unique);
    return result;
}

std::string TypeWalker::ListNameOf(std::string_view typeName)
{
    std::string text(typeName);
    constexpr std::string_view SharedPointer = "class SharedPointer<";
    if (text.starts_with(SharedPointer) && text.ends_with('>'))
        text = text.substr(SharedPointer.size(), text.size() - SharedPointer.size() - 1);
    if (text.ends_with('*'))
        text.pop_back();
    std::string stripped;
    stripped.reserve(text.size());
    for (std::size_t i = 0; i < text.size();)
    {
        bool const boundary = i == 0 || !(std::isalnum(static_cast<unsigned char>(text[i - 1])) || text[i - 1] == '_');
        if (boundary && text.compare(i, 6, "class ") == 0)
        {
            i += 6;
            continue;
        }
        if (boundary && text.compare(i, 7, "struct ") == 0)
        {
            i += 7;
            continue;
        }
        stripped.push_back(text[i++]);
    }
    auto replaceAll = [&](std::string_view from, std::string_view to)
    {
        for (std::size_t at = stripped.find(from); at != std::string::npos; at = stripped.find(from, at + to.size()))
            stripped.replace(at, from.size(), to);
    };
    replaceAll("std::basic_string<char,std::char_traits<char>,std::allocator<char> >", "std::string");
    replaceAll("std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t> >", "std::wstring");
    for (std::size_t at = stripped.find(" >"); at != std::string::npos; at = stripped.find(" >"))
        stripped.erase(at, 1);
    return stripped;
}
