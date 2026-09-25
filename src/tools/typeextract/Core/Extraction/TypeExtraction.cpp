/*
 * Project Ambrose by Imjustchico
 * Runs a whole extraction: refuses an install whose revision cannot name a dump file, loads the install's client program and runtime, runs its C and C++ initializers and lazy getters, counting the getters that fault, adds its races, walks and validates the type map, checks that every enum eRace property received every race and that the server's type loader accepts the dump, and gathers the revision, executable hash, timings and discoveries; also names the default output file.
 */

#include "TypeExtraction.h"
#include "ClientDiscovery.h"
#include "ClientLocator.h"
#include "ClientSystem.h"
#include "CodeIndex.h"
#include "GuestProcess.h"
#include "KiwadArchive.h"
#include "SHA256.h"
#include "StringHash.h"
#include "TypeWalker.h"
#include "WindowsApi.h"

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <pugixml.hpp>

#include <algorithm>
#include <array>
#include <map>
#include <chrono>
#include <cstddef>
#include <exception>
#include <limits>
#include <set>
#include <string_view>
#include <type_traits>
#include <unordered_set>

namespace
{
    constexpr std::string_view ExecutableName = "WizardGraphicalClient.exe";
    constexpr std::string_view RaceFile = "Races.xml";

    using Clock = std::chrono::steady_clock;

    uint64 Milliseconds(Clock::time_point since)
    {
        return static_cast<uint64>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - since).count());
    }

    std::string Hex(std::span<uint8 const> bytes)
    {
        std::string out;
        out.reserve(bytes.size() * 2);
        for (uint8 const b : bytes)
            out += fmt::format("{:02x}", b);
        return out;
    }

    template <typename T>
    std::optional<T> TryRead(Machine const& machine, uint64 address)
    {
        std::array<uint8, sizeof(T)> bytes{};
        if (!machine.TryRead(address, bytes))
            return std::nullopt;
        uint64 value = 0;
        for (std::size_t index = sizeof(T); index-- > 0;)
            value = (value << 8) | bytes[index];
        return static_cast<T>(value);
    }

    bool TryReadString(Machine const& machine, uint64 address, ClientLayout const& layout, std::string& text)
    {
        std::optional<uint64> const length = TryRead<uint64>(machine, address + layout.StringSize);
        std::optional<uint64> const capacity = TryRead<uint64>(machine, address + layout.StringCapacity);
        if (!length || !capacity || *length > TypeWalker::MaxStringLength || *capacity < *length)
            return false;
        uint64 content = address;
        if (*capacity > layout.StringInlineCapacity)
        {
            std::optional<uint64> const pointer = TryRead<uint64>(machine, address);
            if (!pointer)
                return false;
            content = *pointer;
        }
        if (*length > std::numeric_limits<std::size_t>::max())
            return false;
        std::vector<uint8> bytes(static_cast<std::size_t>(*length));
        if (!machine.TryRead(content, bytes))
            return false;
        text.assign(bytes.begin(), bytes.end());
        return true;
    }

    template <typename T, typename Reader>
    std::optional<uint64> FindUniqueOffset(std::span<uint64 const> objects, std::span<T const> expected, uint64 limit, uint64 step, Reader read, std::size_t* matchingOffsets = nullptr)
    {
        if (objects.empty() || objects.size() != expected.size())
            return std::nullopt;
        std::set<uint64> candidates;
        for (uint64 offset = 0; offset <= limit; offset += step)
        {
            bool matches = true;
            for (std::size_t index = 0; index < objects.size(); ++index)
            {
                std::optional<T> const value = read(objects[index] + offset);
                if (!value || *value != expected[index])
                {
                    matches = false;
                    break;
                }
            }
            if (matches)
                candidates.insert(offset);
        }
        if (matchingOffsets)
            *matchingOffsets = candidates.size();
        if (candidates.size() != 1)
            return std::nullopt;
        return *candidates.begin();
    }

    struct OffsetVote
    {
        uint64 Offset = 0;
        std::size_t Matches = 0;
        std::size_t RunnerUpMatches = 0;
    };

    template <typename T, typename Reader>
    std::optional<OffsetVote> FindBestOffset(std::span<uint64 const> objects, std::span<T const> expected, uint64 limit, uint64 step, Reader read)
    {
        if (objects.empty() || objects.size() != expected.size())
            return std::nullopt;
        std::vector<OffsetVote> votes;
        for (uint64 offset = 0; offset <= limit; offset += step)
        {
            OffsetVote vote{ offset };
            for (std::size_t index = 0; index < objects.size(); ++index)
            {
                std::optional<T> const value = read(objects[index] + offset);
                if (value && *value == expected[index])
                    ++vote.Matches;
            }
            votes.push_back(vote);
        }
        std::sort(votes.begin(), votes.end(), [](OffsetVote const& left, OffsetVote const& right)
        {
            return left.Matches != right.Matches ? left.Matches > right.Matches : left.Offset < right.Offset;
        });
        if (votes.empty())
            return std::nullopt;
        if (votes.size() > 1)
            votes[0].RunnerUpMatches = votes[1].Matches;
        return votes[0];
    }

    std::vector<std::string> DeriveTypeFields(Machine const& machine, GuestHeap const& heap, std::span<uint64 const> types, ClientLayout& layout)
    {
        struct Sample
        {
            uint64 Address = 0;
            std::string Name;
            uint32 Hash = 0;
            uint8 Pointer = 0;
            uint64 PropertyList = 0;
        };
        std::vector<Sample> samples;
        samples.reserve(types.size());
        uint64 maxTypeOffset = std::numeric_limits<uint64>::max();
        for (uint64 const type : types)
        {
            std::optional<uint64> const blockSize = heap.SizeOf(type);
            if (!blockSize || *blockSize < sizeof(uint64))
                return {};
            maxTypeOffset = std::min(maxTypeOffset, *blockSize - sizeof(uint64));
            std::string name;
            if (!TryReadString(machine, type + layout.TypeName, layout, name))
                return {};
            std::optional<uint32> const hash = TryRead<uint32>(machine, type + layout.TypeHash);
            std::optional<uint8> const pointer = TryRead<uint8>(machine, type + layout.TypePointer);
            std::optional<uint64> const propertyList = TryRead<uint64>(machine, type + layout.TypePropertyList);
            if (!hash || !pointer || !propertyList || StringHash::KiStringHash(name) != *hash)
                return {};
            samples.push_back({ type, std::move(name), *hash, *pointer, *propertyList });
        }
        if (samples.size() < 20)
            return {};

        std::vector<uint64> objects;
        std::vector<std::string> names;
        std::vector<uint32> hashes;
        std::vector<uint8> pointerFlags;
        std::vector<uint64> lists;
        for (Sample const& sample : samples)
        {
            objects.push_back(sample.Address);
            names.push_back(sample.Name);
            hashes.push_back(sample.Hash);
            pointerFlags.push_back(sample.Pointer);
            lists.push_back(sample.PropertyList);
        }

        std::vector<std::string> confirmations;
        auto confirm = [&](std::string field, uint64& value, std::optional<uint64> discovered, std::string description)
        {
            if (!discovered)
                return;
            value = *discovered;
            confirmations.push_back(fmt::format("{} at {:#x}", field, value));
            layout.ConfirmDerived(std::move(field), std::move(description));
        };

        std::optional<uint64> const hashOffset = FindUniqueOffset<uint32>(objects, hashes, maxTypeOffset, 4, [&](uint64 address) { return TryRead<uint32>(machine, address); });
        confirm("Type.hash", layout.TypeHash, hashOffset, fmt::format("matched the KiStringHash of {} registered type names", samples.size()));
        std::set<uint64> nameOffsets;
        if (hashOffset)
        {
            uint64 const maxNameOffset = std::min(maxTypeOffset, *hashOffset > layout.StringObjectSize ? *hashOffset - layout.StringObjectSize : 0);
            for (uint64 offset = 0; offset <= maxNameOffset; offset += 8)
            {
                bool matches = true;
                for (std::size_t index = 0; index < objects.size(); ++index)
                {
                    std::string name;
                    if (!TryReadString(machine, objects[index] + offset, layout, name) || name != names[index])
                    {
                        matches = false;
                        break;
                    }
                }
                if (matches)
                    nameOffsets.insert(offset);
            }
        }
        std::optional<uint64> const nameOffset = nameOffsets.size() == 1 ? std::optional<uint64>(*nameOffsets.begin()) : std::nullopt;
        if (!nameOffset)
            confirmations.push_back(fmt::format("Type.name has {} matching offsets before the matched hash field; it remains assumed", nameOffsets.size()));
        confirm("Type.name", layout.TypeName, nameOffset, fmt::format("matched {} registered type names preceding the independently matched Type.hash field", samples.size()));
        std::optional<uint64> const listOffset = FindUniqueOffset<uint64>(objects, lists, maxTypeOffset, 8, [&](uint64 address) { return TryRead<uint64>(machine, address); });
        confirm("Type.property_list", layout.TypePropertyList, listOffset, fmt::format("matched {} registered type-to-property-list pointers", samples.size()));

        bool const hasPointer = std::find(pointerFlags.begin(), pointerFlags.end(), uint8{ 0 }) != pointerFlags.end();
        bool const hasNonPointer = std::find(pointerFlags.begin(), pointerFlags.end(), uint8{ 1 }) != pointerFlags.end();
        if (hasPointer && hasNonPointer)
        {
            std::optional<uint64> const pointerOffset = FindUniqueOffset<uint8>(objects, pointerFlags, maxTypeOffset, 1, [&](uint64 address) { return TryRead<uint8>(machine, address); });
            confirm("Type.pointer", layout.TypePointer, pointerOffset, fmt::format("matched pointer flags across {} registered types, including pointer and non-pointer types", samples.size()));
        }

        std::vector<uint64> stringObjects;
        std::vector<uint64> lengths;
        std::vector<uint64> capacities;
        for (Sample const& sample : samples)
        {
            stringObjects.push_back(sample.Address + layout.TypeName);
            lengths.push_back(sample.Name.size());
            std::optional<uint64> const capacity = TryRead<uint64>(machine, stringObjects.back() + layout.StringCapacity);
            if (!capacity)
                return confirmations;
            capacities.push_back(*capacity);
        }
        uint64 const stringSize = hashOffset && nameOffset && *hashOffset > *nameOffset ? *hashOffset - *nameOffset : layout.StringObjectSize;
        uint64 const maxStringOffset = stringSize >= sizeof(uint64) ? stringSize - sizeof(uint64) : 0;
        std::optional<uint64> const sizeOffset = FindUniqueOffset<uint64>(stringObjects, lengths, maxStringOffset, 8, [&](uint64 address) { return TryRead<uint64>(machine, address); });
        confirm("std::string.size", layout.StringSize, sizeOffset, fmt::format("matched the lengths of {} registered type names", samples.size()));
        std::optional<uint64> const capacityOffset = FindUniqueOffset<uint64>(stringObjects, capacities, maxStringOffset, 8, [&](uint64 address) { return TryRead<uint64>(machine, address); });
        confirm("std::string.capacity", layout.StringCapacity, capacityOffset, fmt::format("matched the capacities of {} registered type-name strings", samples.size()));
        if (nameOffset && hashOffset && *hashOffset > *nameOffset)
        {
            uint64 const objectSize = *hashOffset - *nameOffset;
            if (objectSize >= layout.StringCapacity + sizeof(uint64))
                confirm("std::string.object_size", layout.StringObjectSize, objectSize, "the distance from Type.name to the independently matched Type.hash field");
        }

        std::set<uint64> inlineCapacities;
        bool sawInline = false;
        bool sawAllocated = false;
        uint64 longestInline = 0;
        uint64 shortestAllocated = std::numeric_limits<uint64>::max();
        if (sizeOffset && capacityOffset)
        {
            for (std::size_t index = 0; index < stringObjects.size(); ++index)
            {
                uint64 const object = samples[index].Address + layout.TypeName;
                uint64 const length = lengths[index];
                std::vector<uint8> expectedBytes(names[index].begin(), names[index].end());
                std::vector<uint8> inlineBytes(expectedBytes.size());
                bool const inlineMatch = machine.TryRead(object, inlineBytes) && inlineBytes == expectedBytes;
                if (inlineMatch)
                {
                    sawInline = true;
                    longestInline = std::max(longestInline, length);
                    if (std::optional<uint64> const capacity = TryRead<uint64>(machine, object + *capacityOffset))
                        inlineCapacities.insert(*capacity);
                }
                else
                {
                    std::optional<uint64> const pointer = TryRead<uint64>(machine, object);
                    std::vector<uint8> pointedBytes(expectedBytes.size());
                    if (!pointer || !machine.TryRead(*pointer, pointedBytes) || pointedBytes != expectedBytes)
                        continue;
                    sawAllocated = true;
                    shortestAllocated = std::min(shortestAllocated, length);
                    if (std::optional<uint64> const capacity = TryRead<uint64>(machine, object + *capacityOffset); capacity && *capacity < length)
                    {
                        sawAllocated = false;
                        break;
                    }
                }
            }
        }
        if (sawInline && sawAllocated && inlineCapacities.size() == 1)
        {
            uint64 const inlineCapacity = *inlineCapacities.begin();
            if (inlineCapacity >= longestInline && inlineCapacity < shortestAllocated)
                confirm("std::string.inline_capacity", layout.StringInlineCapacity, inlineCapacity, "inline and allocated representations observed across registered type names");
        }
        return confirmations;
    }

    std::vector<std::string> DeriveMapFields(Machine const& machine, GuestHeap const& heap, uint64 head, std::span<uint64 const> types, ClientLayout& layout)
    {
        std::vector<uint64> nodes;
        std::vector<uint64> stack;
        std::unordered_set<uint64> visited;
        uint64 node = machine.ReadU64(head + layout.MapNodeParent);
        auto isNil = [&](uint64 address)
        {
            return address == 0 || address == head || machine.ReadU8(address + layout.MapNodeIsNil) != 0;
        };
        while (!isNil(node) || !stack.empty())
        {
            while (!isNil(node))
            {
                if (!visited.insert(node).second || visited.size() > ClientDiscovery::MaxTypeMapNodes)
                    return {};
                stack.push_back(node);
                node = machine.ReadU64(node + layout.MapNodeLeft);
            }
            node = stack.back();
            stack.pop_back();
            nodes.push_back(node);
            node = machine.ReadU64(node + layout.MapNodeRight);
        }
        if (nodes.size() != types.size() || nodes.empty())
            return {};

        std::vector<uint64> nilSamples = nodes;
        nilSamples.push_back(head);
        std::vector<uint64> left;
        std::vector<uint64> parent;
        std::vector<uint64> right;
        std::vector<uint8> colors;
        std::vector<uint8> nilFlags;
        for (std::size_t index = 0; index < nodes.size(); ++index)
        {
            left.push_back(machine.ReadU64(nodes[index] + layout.MapNodeLeft));
            parent.push_back(machine.ReadU64(nodes[index] + layout.MapNodeParent));
            right.push_back(machine.ReadU64(nodes[index] + layout.MapNodeRight));
            colors.push_back(machine.ReadU8(nodes[index] + layout.MapNodeColor));
            nilFlags.push_back(machine.ReadU8(nodes[index] + layout.MapNodeIsNil));
            if (machine.ReadU64(nodes[index] + layout.MapNodeValue) != types[index])
                return {};
        }
        nilFlags.push_back(machine.ReadU8(head + layout.MapNodeIsNil));

        std::unordered_map<uint64, uint32> hashesByType;
        hashesByType.reserve(types.size());
        for (uint64 const type : types)
            hashesByType.emplace(type, machine.ReadU32(type + layout.TypeHash));
        for (uint64 const mapNode : nodes)
        {
            uint64 const value = machine.ReadU64(mapNode + layout.MapNodeValue);
            auto const found = hashesByType.find(value);
            if (found == hashesByType.end())
                return {};
        }
        std::vector<uint64> expectedValues(types.begin(), types.end());

        uint64 maxOffset = std::numeric_limits<uint64>::max();
        for (uint64 const sample : nodes)
        {
            std::optional<uint64> const size = heap.SizeOf(sample);
            if (!size || *size < sizeof(uint64))
                return {};
            maxOffset = std::min(maxOffset, *size - sizeof(uint64));
        }

        std::vector<std::string> confirmations;
        auto confirm = [&](auto const& expected, std::string field, uint64& value, uint64 step, auto reader)
        {
            using Value = typename std::decay_t<decltype(expected)>::value_type;
            std::optional<uint64> const offset = FindUniqueOffset<Value>(nodes, expected, maxOffset, step, reader);
            if (!offset)
                return;
            value = *offset;
            layout.ConfirmDerived(field, fmt::format("matched {} live type-map nodes", nodes.size()));
            confirmations.push_back(fmt::format("{} at {:#x}", field, value));
        };
        confirm(left, "std::map.node.left", layout.MapNodeLeft, 8, [&](uint64 address) { return TryRead<uint64>(machine, address); });
        confirm(parent, "std::map.node.parent", layout.MapNodeParent, 8, [&](uint64 address) { return TryRead<uint64>(machine, address); });
        confirm(right, "std::map.node.right", layout.MapNodeRight, 8, [&](uint64 address) { return TryRead<uint64>(machine, address); });
        confirm(expectedValues, "std::map.node.value", layout.MapNodeValue, 8, [&](uint64 address) { return TryRead<uint64>(machine, address); });
        std::vector<std::pair<uint64, std::size_t>> keyVotes;
        for (uint64 offset = 0; offset <= maxOffset; offset += 4)
        {
                std::size_t matches = 0;
                for (uint64 const mapNode : nodes)
                {
                    uint64 const type = machine.ReadU64(mapNode + layout.MapNodeValue);
                    auto const found = hashesByType.find(type);
                    std::optional<uint32> const key = TryRead<uint32>(machine, mapNode + offset);
                    if (found != hashesByType.end() && key && *key == found->second)
                        ++matches;
                }
                keyVotes.emplace_back(offset, matches);
        }
        std::sort(keyVotes.begin(), keyVotes.end(), [](auto const& left, auto const& right)
        {
                return left.second != right.second ? left.second > right.second : left.first < right.first;
        });
        if (!keyVotes.empty() && keyVotes.front().second >= nodes.size() * 95 / 100
                && (keyVotes.size() == 1 || keyVotes.front().second >= keyVotes[1].second * ClientDiscovery::MinimumWinnerRatio))
        {
                layout.MapNodeKey = keyVotes.front().first;
                std::size_t const mismatches = nodes.size() - keyVotes.front().second;
                std::string const reason = fmt::format("matched {} of {} type hashes; {} map entries use a distinct key", keyVotes.front().second, nodes.size(), mismatches);
                layout.ConfirmDerived("std::map.node.key", reason);
                confirmations.push_back(fmt::format("std::map.node.key at {:#x} ({})", keyVotes.front().first, reason));
        }
        confirm(colors, "std::map.node.color", layout.MapNodeColor, 1, [&](uint64 address) { return TryRead<uint8>(machine, address); });
            std::optional<uint64> const headSize = heap.SizeOf(head);
            uint64 const maxNilOffset = headSize && *headSize > 0 ? std::min(maxOffset, *headSize - 1) : maxOffset;
            auto const nilOffset = FindUniqueOffset<uint8>(nilSamples, nilFlags, maxNilOffset, 1, [&](uint64 address) { return TryRead<uint8>(machine, address); });
            if (nilOffset)
            {
                layout.MapNodeIsNil = *nilOffset;
                layout.ConfirmDerived("std::map.node.is_nil", fmt::format("matched the live type-map nodes and its nil head"));
                confirmations.push_back(fmt::format("std::map.node.is_nil at {:#x}", *nilOffset));
            }
        return confirmations;
    }

    std::vector<std::string> DeriveListFields(Machine const& machine, std::span<uint64 const> types, ClientLayout& layout)
    {
        std::map<uint64, std::string> uniqueLists;
        for (uint64 const type : types)
        {
            std::string name;
            if (!TryReadString(machine, type + layout.TypeName, layout, name))
                return {};
            uint64 const list = machine.ReadU64(type + layout.TypePropertyList);
            if (list != 0)
                uniqueLists.try_emplace(list, TypeWalker::ListNameOf(name));
        }
        if (uniqueLists.size() < 20)
            return {};

        std::vector<uint64> lists;
        std::vector<std::string> names;
        std::vector<uint8> singletonValues;
        std::vector<uint64> bases;
        std::vector<uint64> propertyBegins;
        std::vector<uint64> propertyEnds;
        for (auto const& [list, name] : uniqueLists)
        {
            if (list == 0)
                return {};
            lists.push_back(list);
            names.push_back(name);
            singletonValues.push_back(machine.ReadU8(list + layout.ListSingleton));
            bases.push_back(machine.ReadU64(list + layout.ListBase));
            propertyBegins.push_back(machine.ReadU64(list + layout.ListProperties));
            propertyEnds.push_back(machine.ReadU64(list + layout.ListProperties + sizeof(uint64)));
        }

        std::vector<std::string> confirmations;
        auto confirm = [&](std::string field, uint64& value, std::optional<uint64> discovered, std::string description)
        {
            if (!discovered)
                return;
            value = *discovered;
            confirmations.push_back(fmt::format("{} at {:#x}", field, value));
            layout.ConfirmDerived(std::move(field), std::move(description));
        };

        std::optional<uint64> const nameOffset = FindUniqueOffset<std::string>(lists, names, 0x300, 8, [&](uint64 address) -> std::optional<std::string>
        {
            std::string name;
            if (!TryReadString(machine, address, layout, name))
                return std::nullopt;
            return name;
        });
        confirm("PropertyList.name", layout.ListName, nameOffset, fmt::format("matched {} registered property-list names against their type names", lists.size()));

        if (std::find(singletonValues.begin(), singletonValues.end(), uint8{ 0 }) != singletonValues.end()
            && std::find(singletonValues.begin(), singletonValues.end(), uint8{ 1 }) != singletonValues.end())
        {
            std::optional<uint64> const singletonOffset = FindUniqueOffset<uint8>(lists, singletonValues, 0x300, 1, [&](uint64 address) { return TryRead<uint8>(machine, address); });
            confirm("PropertyList.singleton", layout.ListSingleton, singletonOffset, fmt::format("matched singleton flags across {} registered property lists", lists.size()));
        }
        std::optional<uint64> const baseOffset = FindUniqueOffset<uint64>(lists, bases, 0x300, 8, [&](uint64 address) { return TryRead<uint64>(machine, address); });
        confirm("PropertyList.base", layout.ListBase, baseOffset, fmt::format("matched base-list pointers across {} registered property lists", lists.size()));

        std::optional<uint64> const propertiesOffset = FindUniqueOffset<uint64>(lists, propertyBegins, 0x300, 8, [&](uint64 address) { return TryRead<uint64>(machine, address); });
        if (propertiesOffset)
        {
            bool endsMatch = true;
            for (std::size_t index = 0; index < lists.size(); ++index)
            {
                std::optional<uint64> const end = TryRead<uint64>(machine, lists[index] + *propertiesOffset + sizeof(uint64));
                if (!end || *end != propertyEnds[index])
                {
                    endsMatch = false;
                    break;
                }
            }
            if (endsMatch)
                confirm("PropertyList.properties", layout.ListProperties, propertiesOffset, fmt::format("matched the begin and end of {} registered property vectors", lists.size()));
        }

        std::set<uint64> entrySizes;
        for (uint64 stride = 8; stride <= 0x80; stride += 8)
        {
            std::size_t checkedLists = 0;
            bool matches = true;
            for (std::size_t listIndex = 0; listIndex < lists.size() && matches; ++listIndex)
            {
                uint64 const begin = propertyBegins[listIndex];
                uint64 const end = propertyEnds[listIndex];
                if (end < begin || (end - begin) % stride != 0)
                {
                    matches = false;
                    break;
                }
                uint64 const count = (end - begin) / stride;
                if (count == 0)
                    continue;
                if (count > TypeWalker::MaxProperties)
                {
                    matches = false;
                    break;
                }
                ++checkedLists;
                for (uint64 index = 0; index < count; ++index)
                {
                    std::optional<uint64> const property = TryRead<uint64>(machine, begin + index * stride);
                    std::optional<uint32> const id = property ? TryRead<uint32>(machine, *property + layout.PropertyId) : std::nullopt;
                    if (!property || !id || *id != index)
                    {
                        matches = false;
                        break;
                    }
                }
            }
            if (matches && checkedLists >= 20)
                entrySizes.insert(stride);
        }
        if (entrySizes.size() == 1)
        {
            uint64 const stride = *entrySizes.begin();
            confirm("PropertyList.entry_size", layout.ListEntrySize, stride, fmt::format("property ids confirmed vector stride {} across registered property lists", stride));
        }
        return confirmations;
    }

    std::vector<std::string> DerivePropertyFields(Machine const& machine, std::span<uint64 const> types, ClientLayout& layout)
    {
        struct PropertySample
        {
            uint64 Address = 0;
            uint32 Id = 0;
            uint32 Hash = 0;
            uint32 Offset = 0;
            uint32 Flags = 0;
            uint64 Type = 0;
            uint64 Container = 0;
            uint64 Options = 0;
        };
        std::vector<PropertySample> samples;
        std::unordered_set<uint64> seen;
        for (uint64 const type : types)
        {
            std::string typeName;
            if (!TryReadString(machine, type + layout.TypeName, layout, typeName))
                return {};
            uint64 const list = machine.ReadU64(type + layout.TypePropertyList);
            if (!list)
                continue;
            uint64 const begin = machine.ReadU64(list + layout.ListProperties);
            uint64 const end = machine.ReadU64(list + layout.ListProperties + sizeof(uint64));
            if (end < begin || (end - begin) % layout.ListEntrySize != 0 || (end - begin) / layout.ListEntrySize > TypeWalker::MaxProperties)
                continue;
            uint64 id = 0;
            for (uint64 slot = begin; slot < end; slot += layout.ListEntrySize, ++id)
            {
                uint64 const property = machine.ReadU64(slot);
                if (!seen.insert(property).second)
                    continue;
                std::optional<uint64> const nameAddress = TryRead<uint64>(machine, property + layout.PropertyName);
                std::optional<std::string> const name = nameAddress ? machine.ReadCString(*nameAddress, TypeWalker::MaxStringLength) : std::nullopt;
                std::optional<uint64> const propertyType = TryRead<uint64>(machine, property + layout.PropertyType);
                std::optional<uint64> const container = TryRead<uint64>(machine, property + layout.PropertyContainer);
                std::optional<uint64> const options = TryRead<uint64>(machine, property + layout.PropertyOptions);
                std::optional<uint32> const storedId = TryRead<uint32>(machine, property + layout.PropertyId);
                std::optional<uint32> const hash = TryRead<uint32>(machine, property + layout.PropertyHash);
                std::optional<uint32> const offset = TryRead<uint32>(machine, property + layout.PropertyOffset);
                std::optional<uint32> const flags = TryRead<uint32>(machine, property + layout.PropertyFlags);
                std::string propertyTypeName;
                if (!name || !propertyType || !container || !options || !storedId || !hash || !offset || !flags || *storedId != id
                    || !TryReadString(machine, *propertyType + layout.TypeName, layout, propertyTypeName)
                    || StringHash::PropertyHash(propertyTypeName, *name) != *hash)
                    continue;
                samples.push_back({ property, *storedId, *hash, *offset, *flags, *propertyType, *container, *options });
            }
        }
        if (samples.size() < 20)
            return {};

        std::vector<uint64> objects;
        std::vector<uint64> names;
        std::vector<uint64> propertyTypes;
        std::vector<uint64> containers;
        std::vector<uint64> optionVectors;
        std::vector<uint32> ids;
        std::vector<uint32> hashes;
        std::vector<uint32> offsets;
        std::vector<uint32> flags;
        objects.reserve(samples.size());
        for (PropertySample const& sample : samples)
        {
            objects.push_back(sample.Address);
            names.push_back(machine.ReadU64(sample.Address + layout.PropertyName));
            propertyTypes.push_back(sample.Type);
            containers.push_back(sample.Container);
            optionVectors.push_back(sample.Options);
            ids.push_back(sample.Id);
            hashes.push_back(sample.Hash);
            offsets.push_back(sample.Offset);
            flags.push_back(sample.Flags);
        }

        std::vector<std::string> confirmations;
        auto confirm = [&](auto const& expected, std::string field, uint64& value, uint64 limit, uint64 step, auto reader, std::string description)
        {
            using Value = typename std::decay_t<decltype(expected)>::value_type;
            std::optional<uint64> const discovered = FindUniqueOffset<Value>(objects, expected, limit, step, reader);
            if (!discovered)
                return;
            value = *discovered;
            confirmations.push_back(fmt::format("{} at {:#x}", field, value));
            layout.ConfirmDerived(std::move(field), std::move(description));
        };
        confirm(names, "Property.name", layout.PropertyName, 0x200, 8, [&](uint64 address) { return TryRead<uint64>(machine, address); },
            fmt::format("matched names for {} properties validated against their owner type and property hash", samples.size()));
        confirm(ids, "Property.id", layout.PropertyId, 0x200, 4, [&](uint64 address) { return TryRead<uint32>(machine, address); },
            fmt::format("matched list positions for {} validated properties", samples.size()));
        confirm(hashes, "Property.hash", layout.PropertyHash, 0x200, 4, [&](uint64 address) { return TryRead<uint32>(machine, address); },
            fmt::format("matched {} independently recomputed property hashes", samples.size()));
        confirm(offsets, "Property.offset", layout.PropertyOffset, 0x200, 4, [&](uint64 address) { return TryRead<uint32>(machine, address); },
            fmt::format("matched serialized offsets for {} validated properties", samples.size()));
        confirm(flags, "Property.flags", layout.PropertyFlags, 0x200, 4, [&](uint64 address) { return TryRead<uint32>(machine, address); },
            fmt::format("matched flags for {} validated properties", samples.size()));
        confirm(propertyTypes, "Property.type", layout.PropertyType, 0x200, 8, [&](uint64 address) { return TryRead<uint64>(machine, address); },
            fmt::format("matched type pointers for {} validated properties", samples.size()));
        std::optional<OffsetVote> const containerVote = FindBestOffset<uint64>(objects, containers, 0x200, 8, [&](uint64 address) { return TryRead<uint64>(machine, address); });
        std::optional<uint64> containerOffset;
        if (layout.PropertyId >= 0x10)
        {
            uint64 const beforeId = layout.PropertyId - 0x10;
            bool matches = true;
            for (std::size_t index = 0; index < objects.size(); ++index)
            {
                std::optional<uint64> const container = TryRead<uint64>(machine, objects[index] + beforeId);
                if (!container || *container != containers[index])
                {
                    matches = false;
                    break;
                }
            }
            if (matches)
                containerOffset = beforeId;
        }
        if (!containerOffset && containerVote && containerVote->Matches >= objects.size() * 95 / 100
            && containerVote->Matches >= containerVote->RunnerUpMatches * ClientDiscovery::MinimumWinnerRatio)
            containerOffset = containerVote->Offset;
        if (containerOffset)
        {
            layout.PropertyContainer = *containerOffset;
            std::string const reason = fmt::format("matched {} validated property container pointers", samples.size());
            layout.ConfirmDerived("Property.container", reason);
            confirmations.push_back(fmt::format("Property.container at {:#x} ({})", *containerOffset, reason));
        }
        std::optional<uint64> const optionsOffset = FindUniqueOffset<uint64>(objects, optionVectors, 0x200, 8, [&](uint64 address) { return TryRead<uint64>(machine, address); });
        if (optionsOffset)
        {
            bool endsMatch = true;
            for (std::size_t index = 0; index < objects.size(); ++index)
            {
                std::optional<uint64> const end = TryRead<uint64>(machine, objects[index] + *optionsOffset + sizeof(uint64));
                if (!end || *end != machine.ReadU64(objects[index] + layout.PropertyOptions + sizeof(uint64)))
                {
                    endsMatch = false;
                    break;
                }
            }
            if (endsMatch)
            {
                layout.PropertyOptions = *optionsOffset;
                layout.ConfirmDerived("Property.options", fmt::format("matched the begin and end of option vectors across {} validated properties", samples.size()));
                confirmations.push_back(fmt::format("Property.options at {:#x}", *optionsOffset));
            }
        }
        return confirmations;
    }

    std::vector<std::string> DeriveOptionFields(Machine const& machine, std::span<uint64 const> types, ClientLayout& layout)
    {
        struct OptionGroup
        {
            uint64 Begin = 0;
            std::vector<std::string> Values;
            std::vector<std::string> Names;
        };
        std::vector<OptionGroup> groups;
        std::vector<uint64> options;
        std::vector<std::string> values;
        std::vector<std::string> names;
        for (uint64 const type : types)
        {
            uint64 const list = machine.ReadU64(type + layout.TypePropertyList);
            if (!list)
                continue;
            uint64 const propertyBegin = machine.ReadU64(list + layout.ListProperties);
            uint64 const propertyEnd = machine.ReadU64(list + layout.ListProperties + sizeof(uint64));
            if (propertyEnd < propertyBegin || (propertyEnd - propertyBegin) % layout.ListEntrySize != 0)
                continue;
            for (uint64 slot = propertyBegin; slot < propertyEnd; slot += layout.ListEntrySize)
            {
                uint64 const property = machine.ReadU64(slot);
                uint64 const begin = machine.ReadU64(property + layout.PropertyOptions);
                uint64 const end = machine.ReadU64(property + layout.PropertyOptions + sizeof(uint64));
                if (end < begin || (end - begin) % layout.OptionSize != 0 || (end - begin) / layout.OptionSize > TypeWalker::MaxOptions)
                    continue;
                OptionGroup group;
                group.Begin = begin;
                for (uint64 option = begin; option < end; option += layout.OptionSize)
                {
                    std::string value;
                    std::string name;
                    if (!TryReadString(machine, option + layout.OptionValue, layout, value)
                        || !TryReadString(machine, option + layout.OptionName, layout, name))
                        return {};
                    options.push_back(option);
                    values.push_back(value);
                    names.push_back(name);
                    group.Values.push_back(std::move(value));
                    group.Names.push_back(std::move(name));
                }
                if (!group.Names.empty())
                    groups.push_back(std::move(group));
            }
        }
        if (options.size() < 20)
            return {};

        std::vector<std::string> confirmations;
        auto confirm = [&](std::string field, uint64& value, std::optional<uint64> discovered, std::string description)
        {
            if (!discovered)
                return;
            value = *discovered;
            confirmations.push_back(fmt::format("{} at {:#x}", field, value));
            layout.ConfirmDerived(std::move(field), std::move(description));
        };
        std::optional<uint64> const valueOffset = FindUniqueOffset<std::string>(options, values, 0x100, 8, [&](uint64 address) -> std::optional<std::string>
        {
            std::string value;
            if (!TryReadString(machine, address, layout, value))
                return std::nullopt;
            return value;
        });
        confirm("EnumOption.value", layout.OptionValue, valueOffset, fmt::format("matched values in {} initialized enum options", options.size()));
        std::optional<uint64> const nameOffset = FindUniqueOffset<std::string>(options, names, 0x100, 8, [&](uint64 address) -> std::optional<std::string>
        {
            std::string name;
            if (!TryReadString(machine, address, layout, name))
                return std::nullopt;
            return name;
        });
        confirm("EnumOption.name", layout.OptionName, nameOffset, fmt::format("matched names in {} initialized enum options", options.size()));

        if (valueOffset && nameOffset)
        {
            std::set<uint64> strides;
            for (uint64 stride = 8; stride <= 0x100; stride += 8)
            {
                std::size_t checked = 0;
                bool matches = true;
                for (OptionGroup const& group : groups)
                {
                    for (std::size_t index = 0; index < group.Names.size(); ++index)
                    {
                        std::string value;
                        std::string name;
                        uint64 const option = group.Begin + index * stride;
                        if (!TryReadString(machine, option + *valueOffset, layout, value) || value != group.Values[index]
                            || !TryReadString(machine, option + *nameOffset, layout, name) || name != group.Names[index])
                        {
                            matches = false;
                            break;
                        }
                        ++checked;
                    }
                    if (!matches)
                        break;
                }
                if (matches && checked >= 20)
                    strides.insert(stride);
            }
            if (strides.size() == 1)
            {
                uint64 const stride = *strides.begin();
                layout.OptionSize = stride;
                layout.ConfirmDerived("EnumOption.size", fmt::format("matched both strings in {} enum options across their owning vectors", options.size()));
                confirmations.push_back(fmt::format("EnumOption.size at {:#x}", stride));
            }
        }
        return confirmations;
    }

    bool IsReadOnlyLeaf(CodeIndex const& code, uint64 address, uint8 depth = 0)
    {
        if (depth > 4)
            return false;
        std::optional<uint64> const functionStart = code.FunctionStart(address);
        std::vector<DecodedInstruction> const instructions = code.DecodeFunctionFrom(address, 64);
        if (instructions.empty())
            return false;
        bool hasReturn = false;
        std::vector<uint64> externalBranches;
        uint64 const begin = functionStart.value_or(address);
        uint64 const end = instructions.back().Address + instructions.back().Length;
        for (DecodedInstruction const& instruction : instructions)
        {
            if (instruction.Kind == InstructionKind::Return)
                hasReturn = true;
            if (instruction.Kind == InstructionKind::Call || (instruction.FirstOperandIsMemory && instruction.WritesFirstOperand))
                return false;
            if (instruction.Kind == InstructionKind::Jump && instruction.BranchTarget
                && (*instruction.BranchTarget < begin || *instruction.BranchTarget >= end))
                externalBranches.push_back(*instruction.BranchTarget);
        }
        for (uint64 const target : externalBranches)
            if (!IsReadOnlyLeaf(code, target, static_cast<uint8>(depth + 1)))
                return false;
        return hasReturn || !externalBranches.empty();
    }

    std::vector<std::string> DeriveContainerSlots(GuestProcess& process, CodeIndex const& code, std::span<uint64 const> types, ClientLayout& layout)
    {
        struct ContainerSample
        {
            uint64 Address = 0;
            std::string Name;
            bool Dynamic = false;
        };
        Machine& machine = process.GetMachine();
        std::map<uint64, ContainerSample> uniqueContainers;
        for (uint64 const type : types)
        {
            uint64 const list = machine.ReadU64(type + layout.TypePropertyList);
            if (!list)
                continue;
            uint64 const begin = machine.ReadU64(list + layout.ListProperties);
            uint64 const end = machine.ReadU64(list + layout.ListProperties + sizeof(uint64));
            if (end < begin || (end - begin) % layout.ListEntrySize != 0)
                continue;
            for (uint64 slot = begin; slot < end; slot += layout.ListEntrySize)
            {
                uint64 const property = machine.ReadU64(slot);
                uint64 const container = machine.ReadU64(property + layout.PropertyContainer);
                if (uniqueContainers.contains(container))
                    continue;
                uint64 const vtable = machine.ReadU64(container);
                uint64 const nameFunction = machine.ReadU64(vtable + layout.ContainerNameSlot * sizeof(uint64));
                uint64 const dynamicFunction = machine.ReadU64(vtable + layout.ContainerDynamicSlot * sizeof(uint64));
                std::array<uint64, 1> const arguments{ container };
                try
                {
                    uint64 const nameAddress = process.Call(nameFunction, arguments, TypeWalker::ContainerCallBudget);
                    std::optional<std::string> const name = machine.ReadCString(nameAddress, 256);
                    if (!name || (*name != "Static" && *name != "Vector" && *name != "List"))
                        continue;
                    bool const dynamic = (process.Call(dynamicFunction, arguments, TypeWalker::ContainerCallBudget) & 0xFF) != 0;
                    uniqueContainers.emplace(container, ContainerSample{ container, *name, dynamic });
                }
                catch (EmulationError const&)
                {
                }
            }
        }
        if (uniqueContainers.size() < 2)
            return {};
        std::vector<ContainerSample> containers;
        containers.reserve(uniqueContainers.size());
        for (auto const& [address, sample] : uniqueContainers)
            containers.push_back(sample);

        std::vector<std::string> confirmations;
        std::set<uint64> nameSlots;
        std::set<uint64> dynamicSlots;
        std::vector<std::string> slotDiagnostics;
        constexpr uint64 SlotsToCheck = 16;
        for (uint64 slot = 0; slot < SlotsToCheck; ++slot)
        {
            bool namesMatch = true;
            bool dynamicMatches = true;
            bool allReadOnlyLeaves = true;
            for (ContainerSample const& container : containers)
            {
                uint64 const vtable = machine.ReadU64(container.Address);
                std::optional<uint64> const function = TryRead<uint64>(machine, vtable + slot * sizeof(uint64));
                if (!function || !IsReadOnlyLeaf(code, *function))
                {
                    namesMatch = false;
                    dynamicMatches = false;
                    allReadOnlyLeaves = false;
                    continue;
                }
                std::array<uint64, 1> const arguments{ container.Address };
                try
                {
                    uint64 const result = process.Call(*function, arguments, TypeWalker::ContainerCallBudget);
                    std::optional<std::string> const name = machine.ReadCString(result, 256);
                    if (!name || *name != container.Name)
                        namesMatch = false;
                    if ((result & 0xFF) != static_cast<uint64>(container.Dynamic))
                        dynamicMatches = false;
                }
                catch (EmulationError const&)
                {
                    namesMatch = false;
                    dynamicMatches = false;
                }
            }
            if (allReadOnlyLeaves)
                slotDiagnostics.push_back(fmt::format("container vtable slot {}: names {}, dynamic flags {}", slot, namesMatch ? "matched" : "did not match", dynamicMatches ? "matched" : "did not match"));
            if (namesMatch)
                nameSlots.insert(slot);
            if (dynamicMatches)
                dynamicSlots.insert(slot);
        }
        if (nameSlots.size() == 1)
        {
            layout.ContainerNameSlot = *nameSlots.begin();
            layout.ConfirmDerived("Container.name_slot", fmt::format("one read-only vtable slot matched the names of {} live container objects", containers.size()));
            confirmations.push_back(fmt::format("Container.name_slot at {:#x}", layout.ContainerNameSlot));
        }
        bool const variedDynamic = std::any_of(containers.begin(), containers.end(), [](ContainerSample const& sample) { return sample.Dynamic; })
            && std::any_of(containers.begin(), containers.end(), [](ContainerSample const& sample) { return !sample.Dynamic; });
        if (variedDynamic && dynamicSlots.size() == 1)
        {
            layout.ContainerDynamicSlot = *dynamicSlots.begin();
            layout.ConfirmDerived("Container.dynamic_slot", fmt::format("one read-only vtable slot matched dynamic flags across {} live container objects", containers.size()));
            confirmations.push_back(fmt::format("Container.dynamic_slot at {:#x}", layout.ContainerDynamicSlot));
        }
        if (nameSlots.size() != 1 || !variedDynamic || dynamicSlots.size() != 1)
        {
            confirmations.push_back(fmt::format("container getter discovery checked {} slots across {} objects; {} safe slot candidates", SlotsToCheck, containers.size(), slotDiagnostics.size()));
            confirmations.insert(confirmations.end(), slotDiagnostics.begin(), slotDiagnostics.end());
        }
        return confirmations;
    }

    uint64 StoreStdString(GuestProcess& process, ClientLayout const& layout, std::string_view text)
    {
        Machine& machine = process.GetMachine();
        uint64 const object = process.GetHeap().Allocate(layout.StringObjectSize, true);
        if (text.size() <= layout.StringInlineCapacity)
        {
            std::vector<uint8> inlineBytes(text.begin(), text.end());
            inlineBytes.resize(16, 0);
            machine.Write(object, inlineBytes);
        }
        else
            machine.WriteU64(object, process.StoreCString(text));
        machine.WriteU64(object + layout.StringSize, text.size());
        machine.WriteU64(object + layout.StringCapacity, std::max<uint64>(text.size(), layout.StringInlineCapacity));
        return object;
    }

    std::vector<std::string> ReadRaces(std::filesystem::path const& client, std::string& error)
    {
        std::filesystem::path const wadPath = client / "Data" / "GameData" / "Root.wad";
        std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(wadPath, error);
        if (!archive)
        {
            error = fmt::format("cannot open {}: {}", ClientLocator::PathText(wadPath), error);
            return {};
        }
        KiwadReadResult const read = archive->Read(RaceFile);
        if (!read.Succeeded())
        {
            error = fmt::format("cannot read {} from Root.wad: {}", RaceFile, read.Error);
            return {};
        }
        pugi::xml_document document;
        pugi::xml_parse_result const parsed = document.load_buffer(read.Data.data(), read.Data.size());
        if (!parsed)
        {
            error = fmt::format("{} is not valid XML: {}", RaceFile, parsed.description());
            return {};
        }
        pugi::xml_node const root = document.document_element();
        std::vector<std::string> races;
        for (pugi::xml_node const race : root.children("Race"))
            races.emplace_back(race.child_value());
        if (races.empty())
            error = fmt::format("{} lists no races", RaceFile);
        return races;
    }
}

TypeExtractionResult TypeExtraction::Extract(TypeExtractionOptions const& options)
{
    TypeExtractionResult result;
    Clock::time_point const started = Clock::now();
    auto progress = [&](std::string const& text)
    {
        if (options.Progress)
            options.Progress(text);
    };
    auto fail = [&](std::string message) -> TypeExtractionResult&
    {
        result.Error = std::move(message);
        result.Stats.TotalMilliseconds = Milliseconds(started);
        return result;
    };

    LocalClientSystem const system;
    std::optional<ClientInstall> const install = ClientInstall::Inspect(system, options.ClientDir);
    if (!install)
        return fail(fmt::format("{} holds no Wizard101 install", ClientLocator::PathText(options.ClientDir)));
    if (install->Revision.empty())
        return fail(fmt::format("{} has no readable Bin/revision.dat, so the dump cannot be named", ClientLocator::PathText(options.ClientDir)));
    if (!IsPlainRevision(install->Revision))
        return fail(fmt::format("{} names the revision {} in Bin/revision.dat, which cannot name a dump file: only letters, digits, '.', '_' and '-' are allowed", ClientLocator::PathText(options.ClientDir), install->Revision));
    result.Metadata.Revision = install->Revision;
    result.Metadata.Extractor = std::string(ExtractorName);
    ClientLayout layout = options.Layout;
    result.LayoutEvidence = layout.Evidence();
    auto reportLayout = [&]()
    {
        result.LayoutEvidence = layout.Evidence();
        for (ClientLayoutEvidence const& evidence : result.LayoutEvidence)
            result.Discovered.push_back(fmt::format("layout {} = {:#x} ({}, confirmed by {})", evidence.Field, evidence.Value, evidence.Status, evidence.ConfirmedBy));
    };
    try
    {
        Clock::time_point phase = Clock::now();
        GuestProcess::Options processOptions;
        processOptions.Folder = options.ClientDir / "Bin";
        GuestProcess process(processOptions);
        WindowsApi::Register(process);
        progress(fmt::format("loading {} and its runtime from {}", ExecutableName, ClientLocator::PathText(processOptions.Folder)));
        GuestModule& program = process.LoadMain(ExecutableName);
        result.Metadata.ExecutableSha256 = Hex(SHA256::GetDigestOf(program.Image->GetBytes()));
        process.AttachRuntime();
        CodeIndex const code(*program.Image);
        result.Stats.LoadMilliseconds = Milliseconds(phase);

        std::string error;
        std::optional<InitializerTable> const table = ClientDiscovery::FindInitializerTable(*program.Image, code, error);
        if (!table)
            return fail(fmt::format("the C++ initializer table was not found: {}", error));
        result.Discovered.push_back(fmt::format("C++ initializers at {:#x}-{:#x} ({} slots)", table->Begin, table->End, table->Count()));

        std::optional<InitializerTable> const cTable = ClientDiscovery::FindCInitializerTable(*program.Image, code, error);
        if (!cTable)
            return fail(fmt::format("the C initializer table was not found: {}", error));
        result.Discovered.push_back(fmt::format("C initializers at {:#x}-{:#x} ({} slots)", cTable->Begin, cTable->End, cTable->Count()));

        phase = Clock::now();
        Machine& machine = process.GetMachine();
        for (uint64 slot = cTable->Begin; slot < cTable->End; slot += 8)
        {
            uint64 const function = machine.ReadU64(slot);
            if (!function)
                continue;
            uint64 status = 0;
            try
            {
                status = process.Call(function, std::span<uint64 const>{}, options.InitializerBudget);
            }
            catch (std::exception const& failure)
            {
                return fail(fmt::format("C initializer {} of {} at {} failed: {}", (slot - cTable->Begin) / 8, cTable->Count(), process.DescribeAddress(function), failure.what()));
            }
            if (static_cast<uint32>(status) != 0)
                return fail(fmt::format("C initializer {} of {} at {} returned {}", (slot - cTable->Begin) / 8, cTable->Count(), process.DescribeAddress(function), static_cast<int32>(status)));
            ++result.Stats.Initializers;
        }
        progress(fmt::format("running {} C++ initializers", table->Count()));
        for (uint64 slot = table->Begin; slot < table->End; slot += 8)
        {
            uint64 const function = machine.ReadU64(slot);
            if (!function)
                continue;
            try
            {
                process.Call(function, std::span<uint64 const>{}, options.InitializerBudget);
            }
            catch (std::exception const& failure)
            {
                return fail(fmt::format("C++ initializer {} of {} at {} failed: {}", (slot - table->Begin) / 8, table->Count(), process.DescribeAddress(function), failure.what()));
            }
            ++result.Stats.Initializers;
        }
        result.Stats.InitializeMilliseconds = Milliseconds(phase);

        phase = Clock::now();
        std::optional<uint64> const head = ClientDiscovery::FindTypeMapHead(machine, process.GetHeap(), layout, error);
        if (!head)
            return fail(fmt::format("the type map was not found: {}", error));
        std::optional<std::vector<uint64>> types = ClientDiscovery::WalkTypeMap(machine, *head, layout, error);
        if (!types)
            return fail(fmt::format("the type map could not be walked: {}", error));
        result.Discovered.push_back(fmt::format("type map at {:#x} with {} types after the initializers", *head, types->size()));
        std::vector<std::string> const derived = DeriveTypeFields(machine, process.GetHeap(), *types, layout);
        for (std::string const& field : derived)
            result.Discovered.push_back(fmt::format("layout derivation: {}", field));
        std::vector<std::string> const mapDerived = DeriveMapFields(machine, process.GetHeap(), *head, *types, layout);
        for (std::string const& field : mapDerived)
            result.Discovered.push_back(fmt::format("layout derivation: {}", field));
        types = ClientDiscovery::WalkTypeMap(machine, *head, layout, error);
        if (!types)
            return fail(fmt::format("the type map could not be walked with its derived layout: {}", error));

        std::optional<DiscoveryVote> const constructor = ClientDiscovery::FindTypeConstructor(machine, code, *types, error);
        if (!constructor)
            return fail(fmt::format("the Type constructor was not found: {}", error));
        result.Discovered.push_back(fmt::format("Type constructor at {} ({} votes, runner-up {} votes)", process.DescribeAddress(constructor->Winner), constructor->WinnerVotes, constructor->RunnerUpVotes));
        std::optional<DiscoveryVote> const listInitializer = ClientDiscovery::FindPropertyListInitializer(machine, code, *types, layout, error);
        if (!listInitializer)
            return fail(fmt::format("the PropertyList initializer was not found: {}", error));
        result.Discovered.push_back(fmt::format("PropertyList initializer at {} ({} votes, runner-up {} votes)", process.DescribeAddress(listInitializer->Winner), listInitializer->WinnerVotes, listInitializer->RunnerUpVotes));
        std::optional<uint64> const raceAdder = ClientDiscovery::FindRaceAdder(code, error);
        if (!raceAdder)
            return fail(fmt::format("the race adder was not found: {}", error));
        result.Discovered.push_back(fmt::format("race adder at {}", process.DescribeAddress(*raceAdder)));
        result.Stats.DiscoverMilliseconds = Milliseconds(phase);

        phase = Clock::now();
        std::vector<uint64> getters;
        std::unordered_set<uint64> seen;
        for (uint64 const target : { constructor->Winner, listInitializer->Winner })
            for (uint64 const function : ClientDiscovery::FunctionsCalling(code, target))
                if (seen.insert(function).second)
                    getters.push_back(function);
        result.Stats.LazyGetters = getters.size();
        progress(fmt::format("running {} lazy type and property list getters", getters.size()));
        std::array<uint64, 4> const noArguments{ 0, 0, 0, 0 };
        std::map<std::string, uint64> faultKinds;
        uint64 nullThisFaults = 0;
        for (uint64 const getter : getters)
        {
            try
            {
                process.Call(getter, noArguments, options.GetterBudget);
                ++result.Stats.LazyGettersRun;
            }
            catch (EmulationError const& failure)
            {
                std::string_view const message = failure.what();
                if (message.find("instruction budget") != std::string_view::npos || message.find("exhausted") != std::string_view::npos)
                    return fail(fmt::format("the lazy getter {} did not finish: {}", process.DescribeAddress(getter), message));
                ++result.Stats.LazyGettersFaulted;
                if (std::optional<GuestFault> const& fault = process.GetMachine().GetLastFault())
                {
                    ++faultKinds[fault->Kind];
                    if (fault->Address < Machine::PageSize)
                        ++nullThisFaults;
                }
                else
                    ++faultKinds["no fault recorded"];
            }
            catch (std::exception const& failure)
            {
                return fail(fmt::format("the lazy getter {} failed: {}", process.DescribeAddress(getter), failure.what()));
            }
        }
        result.Discovered.push_back(fmt::format("{} of {} lazy getters ran, {} faulted", result.Stats.LazyGettersRun, result.Stats.LazyGetters, result.Stats.LazyGettersFaulted));
        for (auto const& [kind, count] : faultKinds)
            result.Discovered.push_back(fmt::format("lazy getter fault: {} x{}", kind, count));
        if (nullThisFaults != 0)
            result.Discovered.push_back(fmt::format("lazy getter fault: {} touched the first page, which is what a getter called with no object does", nullThisFaults));

        std::vector<std::string> const races = ReadRaces(options.ClientDir, error);
        if (races.empty())
            return fail(error);
        progress(fmt::format("adding {} races from {}", races.size(), RaceFile));
        for (std::string const& race : races)
        {
            try
            {
                std::array<uint64, 1> const arguments{ StoreStdString(process, layout, race) };
                process.Call(*raceAdder, arguments, options.RaceBudget);
            }
            catch (std::exception const& failure)
            {
                return fail(fmt::format("adding the race {} failed: {}", race, failure.what()));
            }
        }
        result.Stats.Races = races.size();

        types = ClientDiscovery::WalkTypeMap(machine, *head, layout, error);
        if (!types)
            return fail(fmt::format("the type map could not be walked: {}", error));
        std::vector<std::string> const listDerived = DeriveListFields(machine, *types, layout);
        for (std::string const& field : listDerived)
            result.Discovered.push_back(fmt::format("layout derivation: {}", field));
        std::vector<std::string> const propertyDerived = DerivePropertyFields(machine, *types, layout);
        for (std::string const& field : propertyDerived)
            result.Discovered.push_back(fmt::format("layout derivation: {}", field));
        std::vector<std::string> const optionDerived = DeriveOptionFields(machine, *types, layout);
        for (std::string const& field : optionDerived)
            result.Discovered.push_back(fmt::format("layout derivation: {}", field));
        std::vector<std::string> const containerDerived = DeriveContainerSlots(process, code, *types, layout);
        for (std::string const& field : containerDerived)
            result.Discovered.push_back(fmt::format("layout derivation: {}", field));
        reportLayout();
        if (options.RequireDerivedLayout)
        {
            auto const unresolved = std::find_if(result.LayoutEvidence.begin(), result.LayoutEvidence.end(), [](ClientLayoutEvidence const& evidence)
            {
                return evidence.Status != "derived";
            });
            if (unresolved != result.LayoutEvidence.end())
                return fail(fmt::format("the client layout could not be derived: {}", unresolved->Field));
        }
        progress(fmt::format("reading and checking {} types", types->size()));
        TypeWalker walker(process, layout);
        TypeWalkResult walk = walker.Walk(*types);
        result.Stats.WalkMilliseconds = Milliseconds(phase);
        result.Stats.Classes = walk.Dump.Classes.size();
        result.Stats.Properties = walk.PropertyCount;
        result.Stats.DuplicateTypes = walk.DuplicateTypes;
        result.Stats.DuplicateOptions = walk.DuplicateOptions;
        result.Discovered.push_back(fmt::format("{} types after the lazy getters and races, {} registered twice, {} enum options listed twice", types->size(), walk.DuplicateTypes, walk.DuplicateOptions));
        result.Stats.HeapBytes = process.GetHeap().GetTop() - process.GetHeap().GetBase();
        result.ProblemCounts = std::move(walk.ProblemCounts);
        result.ProblemSamples = std::move(walk.ProblemSamples);
        result.Dump = std::move(walk.Dump);
        result.UnhandledApiCalls = process.GetUnhandledApiCalls();

        std::vector<std::string> kernel;
        for (auto const& [call, count] : result.UnhandledApiCalls)
            if (call.starts_with("kernel32.dll!"))
                kernel.push_back(fmt::format("{} ({} calls)", call.substr(13), count));
        if (!kernel.empty())
            return fail(fmt::format("the client called Windows functions the emulator does not provide: {}", fmt::join(kernel, ", ")));
        if (!result.ProblemCounts.empty())
        {
            std::vector<std::string> kinds;
            for (auto const& [kind, count] : result.ProblemCounts)
                kinds.push_back(fmt::format("{} {}", count, kind));
            return fail(fmt::format("the extracted types failed validation: {}", fmt::join(kinds, ", ")));
        }
        if (!CheckRaces(result.Dump, races, error))
            return fail(error);
        progress("building the server's type catalog from the dump");
        if (!CheckCatalog(result.Dump, result.Metadata.ExecutableSha256, error))
            return fail(error);
    }
    catch (std::exception const& failure)
    {
        return fail(failure.what());
    }
    result.Stats.TotalMilliseconds = Milliseconds(started);
    return result;
}

bool TypeExtraction::IsPlainRevision(std::string_view revision)
{
    if (revision.empty() || revision == "." || revision == "..")
        return false;
    return std::all_of(revision.begin(), revision.end(), [](char c)
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
    });
}

std::optional<std::filesystem::path> TypeExtraction::DefaultOutputPath(std::filesystem::path const& dataFolder, std::string_view revision)
{
    if (!IsPlainRevision(revision) || dataFolder.empty() || !dataFolder.is_absolute())
        return std::nullopt;
    return dataFolder / "types" / (std::string(revision) + ".json");
}

bool TypeExtraction::CheckRaces(TypeDumpLoader::RawDump const& dump, std::span<std::string const> races, std::string& error)
{
    for (TypeDumpLoader::RawClass const& rawClass : dump.Classes)
    {
        for (TypeDumpLoader::RawProperty const& property : rawClass.Properties)
        {
            if (property.Type != RaceEnumType)
                continue;
            std::unordered_set<std::string_view> options;
            options.reserve(property.Options.size());
            for (auto const& option : property.Options)
                options.insert(option.first);
            std::string_view firstMissing;
            std::size_t missing = 0;
            for (std::string const& race : races)
            {
                if (options.contains(race))
                    continue;
                if (missing++ == 0)
                    firstMissing = race;
            }
            if (missing != 0)
            {
                error = fmt::format("the races did not land: {} property {} of type {} lacks {} of the {} races from Races.xml, among them {}", rawClass.Name.value_or(rawClass.Key), property.Name,
                    RaceEnumType, missing, races.size(), firstMissing);
                return false;
            }
        }
    }
    return true;
}

bool TypeExtraction::CheckCatalog(TypeDumpLoader::RawDump const& dump, std::string const& sha256, std::string& error)
{
    std::vector<std::string> errors;
    if (TypeCatalogBuilder::Build(dump, "extracted", sha256, 1, {}, errors))
        return true;
    if (errors.empty())
    {
        error = "the server's type loader refuses the extracted dump without naming a reason";
        return false;
    }
    std::size_t const listed = std::min(errors.size(), MaxListedLoaderErrors);
    error = fmt::format("the server's type loader refuses the extracted dump: {}", fmt::join(errors.begin(), errors.begin() + static_cast<std::ptrdiff_t>(listed), "; "));
    if (errors.size() > listed)
        error += fmt::format("; and {} more", errors.size() - listed);
    return false;
}
