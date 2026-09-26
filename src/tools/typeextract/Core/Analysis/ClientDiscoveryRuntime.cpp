/*
 * Project Ambrose by Imjustchico
 * The discovery steps that read the emulated process: the type map head found from heap nodes whose type name hashes to their key, an in-order walk of that map, Type and std::string fields matched against values supplied to the Type constructor, and votes for the Type constructor and PropertyList initializer.
 */

#include "ClientDiscovery.h"
#include "CodeIndex.h"
#include "GuestHeap.h"
#include "Machine.h"
#include "PeImage.h"
#include "StringHash.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace
{
    constexpr uint64 MaxTypeNameLength = 512;
    constexpr uint64 VtableCallWindow = 0x10;
    constexpr std::size_t ListReferencesPerList = 4;
    constexpr std::size_t ListCallWindowBytes = 0x40;
    constexpr std::size_t ListCallWindowInstructions = 16;

    uint64 Get64(std::vector<uint8> const& bytes, uint64 offset)
    {
        uint64 value = 0;
        for (int i = 7; i >= 0; --i)
            value = (value << 8) | bytes[offset + static_cast<uint64>(i)];
        return value;
    }

    uint32 Get32(std::vector<uint8> const& bytes, uint64 offset)
    {
        uint32 value = 0;
        for (int i = 3; i >= 0; --i)
            value = (value << 8) | bytes[offset + static_cast<uint64>(i)];
        return value;
    }

    std::optional<uint64> TryReadU64(Machine const& machine, uint64 address)
    {
        std::array<uint8, sizeof(uint64)> bytes{};
        if (!machine.TryRead(address, bytes))
            return std::nullopt;
        uint64 value = 0;
        for (std::size_t index = bytes.size(); index-- > 0;)
            value = (value << 8) | bytes[index];
        return value;
    }

    enum class StringRepresentation
    {
        Inline,
        Allocated
    };

    std::optional<StringRepresentation> MatchConstructedString(Machine const& machine, GuestHeap const& heap, uint64 address, std::string_view expected)
    {
        std::vector<uint8> const value(expected.begin(), expected.end());
        std::vector<uint8> terminated = value;
        terminated.push_back(0);
        std::vector<uint8> observed(terminated.size());
        if (machine.TryRead(address, observed) && observed == terminated)
            return StringRepresentation::Inline;

        std::optional<uint64> const pointer = TryReadU64(machine, address);
        std::optional<uint64> const allocationSize = pointer ? heap.SizeOf(*pointer) : std::nullopt;
        if (!pointer || !allocationSize || *allocationSize < terminated.size())
            return std::nullopt;
        if (!machine.TryRead(*pointer, observed) || observed != terminated)
            return std::nullopt;
        return StringRepresentation::Allocated;
    }

    std::optional<DiscoveryVote> Elect(std::map<uint64, uint64> const& votes, std::string_view what, std::string& error)
    {
        std::vector<std::pair<uint64, uint64>> ranked(votes.begin(), votes.end());
        std::sort(ranked.begin(), ranked.end(), [](auto const& a, auto const& b) { return a.second != b.second ? a.second > b.second : a.first < b.first; });
        if (ranked.empty())
        {
            error = fmt::format("no candidate for the {} was found", what);
            return std::nullopt;
        }
        DiscoveryVote vote;
        vote.Winner = ranked[0].first;
        vote.WinnerVotes = ranked[0].second;
        if (ranked.size() > 1)
        {
            vote.RunnerUp = ranked[1].first;
            vote.RunnerUpVotes = ranked[1].second;
        }
        if (vote.WinnerVotes < ClientDiscovery::MinimumWinnerVotes || vote.WinnerVotes < ClientDiscovery::MinimumWinnerRatio * vote.RunnerUpVotes)
        {
            error = fmt::format("the {} vote has no clear winner: {:#x} with {} votes, then {:#x} with {}", what, vote.Winner, vote.WinnerVotes, vote.RunnerUp, vote.RunnerUpVotes);
            return std::nullopt;
        }
        return vote;
    }
}

std::optional<uint64> ClientDiscovery::FindTypeMapHead(Machine const& machine, GuestHeap const& heap, ClientLayout const& layout, std::string& error)
{
    uint64 const base = heap.GetBase();
    uint64 const top = heap.GetTop();
    if (top <= base)
    {
        error = "the guest heap is empty, so no type map exists yet";
        return std::nullopt;
    }
    std::vector<uint8> const bytes = machine.ReadBytes(base, static_cast<std::size_t>(top - base));
    uint64 const size = bytes.size();
    auto inHeap = [&](uint64 address, uint64 length) { return address >= base && address - base <= size && length <= size - (address - base); };

    std::unordered_set<uint64> nodes;
    uint64 const nodeSize = layout.MapNodeValue + 8;
    for (uint64 offset = 0; offset + nodeSize <= size; offset += 8)
    {
        if (bytes[offset + layout.MapNodeIsNil] != 0 || bytes[offset + layout.MapNodeColor] > 1)
            continue;
        uint64 const value = Get64(bytes, offset + layout.MapNodeValue);
        uint64 const typeSize = std::max(layout.TypeHash + 4, layout.TypeName + layout.StringObjectSize);
        if (!inHeap(value, typeSize))
            continue;
        uint64 const key = Get32(bytes, offset + layout.MapNodeKey);
        uint64 const object = value - base;
        if (Get32(bytes, object + layout.TypeHash) != key)
            continue;
        uint64 const length = Get64(bytes, object + layout.TypeName + layout.StringSize);
        uint64 const capacity = Get64(bytes, object + layout.TypeName + layout.StringCapacity);
        if (length == 0 || length >= MaxTypeNameLength || capacity < length)
            continue;
        uint64 textOffset = object + layout.TypeName;
        if (capacity > layout.StringInlineCapacity)
        {
            uint64 const pointer = Get64(bytes, object + layout.TypeName);
            if (!inHeap(pointer, length))
                continue;
            textOffset = pointer - base;
        }
        std::string_view const text(reinterpret_cast<char const*>(bytes.data() + textOffset), static_cast<std::size_t>(length));
        if (StringHash::KiStringHash(text) != key)
            continue;
        nodes.insert(base + offset);
    }
    if (nodes.empty())
    {
        error = "no type map node was found on the guest heap";
        return std::nullopt;
    }
    std::map<uint64, uint64> heads;
    for (uint64 const node : nodes)
    {
        uint64 const parent = Get64(bytes, node - base + layout.MapNodeParent);
        if (!nodes.contains(parent) && inHeap(parent, nodeSize) && bytes[parent - base + layout.MapNodeIsNil] == 1)
            ++heads[parent];
    }
    if (heads.size() != 1)
    {
        error = fmt::format("expected the type map nodes to share one head, found {} heads among {} nodes", heads.size(), nodes.size());
        return std::nullopt;
    }
    return heads.begin()->first;
}

std::optional<std::vector<uint64>> ClientDiscovery::WalkTypeMap(Machine const& machine, uint64 head, ClientLayout const& layout, std::string& error)
{
    std::vector<uint64> types;
    std::unordered_set<uint64> visited;
    std::vector<uint64> stack;
    uint64 node = machine.ReadU64(head + layout.MapNodeParent);
    auto isNil = [&](uint64 address) { return address == 0 || address == head || machine.ReadU8(address + layout.MapNodeIsNil) != 0; };
    while (!isNil(node) || !stack.empty())
    {
        while (!isNil(node))
        {
            if (!visited.insert(node).second)
            {
                error = fmt::format("the type map has a cycle at {:#x}", node);
                return std::nullopt;
            }
            if (visited.size() > MaxTypeMapNodes)
            {
                error = fmt::format("the type map has more than {} nodes", MaxTypeMapNodes);
                return std::nullopt;
            }
            stack.push_back(node);
            node = machine.ReadU64(node + layout.MapNodeLeft);
        }
        node = stack.back();
        stack.pop_back();
        types.push_back(machine.ReadU64(node + layout.MapNodeValue));
        node = machine.ReadU64(node + layout.MapNodeRight);
    }
    if (types.empty())
    {
        error = "the type map is empty";
        return std::nullopt;
    }
    return types;
}

bool ClientDiscovery::DeriveConstructedTypeLayout(Machine const& machine, GuestHeap const& heap, std::span<ConstructedTypeSample const> samples, ClientLayout& layout, std::string& error)
{
    if (samples.size() < 2)
    {
        error = "Type.name needs short and allocated constructor samples";
        return false;
    }

    uint64 maxOffset = std::numeric_limits<uint64>::max();
    for (ConstructedTypeSample const& sample : samples)
    {
        std::optional<uint64> const size = heap.SizeOf(sample.Address);
        if (!size || *size < sizeof(uint32))
        {
            error = "Type.hash could not be placed in a constructor sample";
            return false;
        }
        maxOffset = std::min(maxOffset, *size - sizeof(uint32));
    }

    std::set<uint64> hashOffsets;
    for (uint64 offset = 0; offset <= maxOffset; offset += sizeof(uint32))
    {
        bool matches = true;
        for (ConstructedTypeSample const& sample : samples)
        {
            std::array<uint8, sizeof(uint32)> bytes{};
            if (!machine.TryRead(sample.Address + offset, bytes) || Get32(std::vector<uint8>(bytes.begin(), bytes.end()), 0) != sample.Hash)
            {
                matches = false;
                break;
            }
        }
        if (matches)
            hashOffsets.insert(offset);
    }

    std::set<uint64> nameOffsets;
    for (uint64 offset = 0; offset <= maxOffset; offset += sizeof(uint64))
    {
        if (offset + sizeof(uint64) * 2 > maxOffset + sizeof(uint32))
            continue;
        bool matches = true;
        bool sawInline = false;
        bool sawAllocated = false;
        for (ConstructedTypeSample const& sample : samples)
        {
            std::optional<StringRepresentation> const representation =
                MatchConstructedString(machine, heap, sample.Address + offset, sample.Name);
            if (!representation)
            {
                matches = false;
                break;
            }
            sawInline = sawInline || *representation == StringRepresentation::Inline;
            sawAllocated = sawAllocated || *representation == StringRepresentation::Allocated;
        }
        if (matches && sawInline && sawAllocated)
            nameOffsets.insert(offset);
    }
    if (nameOffsets.empty())
    {
        error = "Type.name has no matching offsets for the chosen inline and allocated names";
        return false;
    }
    std::set<std::array<uint64, 5>> candidates;
    uint64 const smallestObjectSize = maxOffset + sizeof(uint32);
    for (uint64 const nameOffset : nameOffsets)
    {
        if (nameOffset > smallestObjectSize || smallestObjectSize - nameOffset < sizeof(uint64))
            continue;
        uint64 const maxStringOffset = smallestObjectSize - nameOffset - sizeof(uint64);
        std::set<uint64> sizeOffsets;
        for (uint64 offset = 0; offset <= maxStringOffset; offset += sizeof(uint64))
        {
            bool lengthsMatch = true;
            for (ConstructedTypeSample const& sample : samples)
            {
                std::optional<uint64> const observed = TryReadU64(machine, sample.Address + nameOffset + offset);
                if (!observed || *observed != sample.Name.size())
                {
                    lengthsMatch = false;
                    break;
                }
            }
            if (lengthsMatch)
                sizeOffsets.insert(offset);
        }

        for (uint64 const sizeOffset : sizeOffsets)
        {
            for (uint64 capacityOffset = 0; capacityOffset <= maxStringOffset; capacityOffset += sizeof(uint64))
            {
                if (capacityOffset <= sizeOffset || capacityOffset - sizeOffset < sizeof(uint64))
                    continue;
                bool capacitiesMatch = true;
                bool sawInline = false;
                bool sawAllocated = false;
                std::optional<uint64> candidateInlineCapacity;
                for (ConstructedTypeSample const& sample : samples)
                {
                    std::optional<uint64> const observed = TryReadU64(machine, sample.Address + nameOffset + capacityOffset);
                    std::optional<StringRepresentation> const representation =
                        MatchConstructedString(machine, heap, sample.Address + nameOffset, sample.Name);
                    if (!observed || !representation)
                    {
                        capacitiesMatch = false;
                        break;
                    }
                    if (*representation == StringRepresentation::Inline)
                    {
                        sawInline = true;
                        if (*observed <= sample.Name.size() || *observed > MaxTypeNameLength
                            || (candidateInlineCapacity && *candidateInlineCapacity != *observed))
                            capacitiesMatch = false;
                        candidateInlineCapacity = *observed;
                    }
                    else
                    {
                        sawAllocated = true;
                        if (*observed < sample.Name.size() || *observed > MaxTypeNameLength)
                            capacitiesMatch = false;
                    }
                }
                if (!capacitiesMatch || !sawInline || !sawAllocated || !candidateInlineCapacity)
                    continue;
                bool allocatedLonger = false;
                for (ConstructedTypeSample const& sample : samples)
                {
                    std::optional<StringRepresentation> const representation =
                        MatchConstructedString(machine, heap, sample.Address + nameOffset, sample.Name);
                    if (representation == StringRepresentation::Allocated && sample.Name.size() > *candidateInlineCapacity)
                        allocatedLonger = true;
                }
                uint64 const hashOffset = nameOffset + capacityOffset + sizeof(uint64);
                if (allocatedLonger && hashOffsets.contains(hashOffset))
                    candidates.insert({ nameOffset, sizeOffset, capacityOffset, *candidateInlineCapacity, hashOffset });
            }
        }
    }
    if (candidates.size() != 1)
    {
        std::string details;
        for (auto const& [nameOffset, sizeOffset, capacityOffset, inlineCapacity, hashOffset] : candidates)
        {
            if (!details.empty())
                details += "; ";
            details += fmt::format("Type.name {:#x}, std::string.size {:#x}, std::string.capacity {:#x}, inline capacity {}, Type.hash {:#x}",
                nameOffset, sizeOffset, capacityOffset, inlineCapacity, hashOffset);
        }
        error = fmt::format("Type.name and its std::string fields have {} complete matches for the chosen constructor values: {}", candidates.size(), details);
        return false;
    }
    auto const [nameOffset, sizeOffset, capacityOffset, inlineCapacity, hashOffset] = *candidates.begin();

    layout.TypeName = nameOffset;
    layout.TypeHash = hashOffset;
    layout.StringSize = sizeOffset;
    layout.StringCapacity = capacityOffset;
    layout.StringInlineCapacity = inlineCapacity;
    layout.StringObjectSize = capacityOffset + sizeof(uint64);
    layout.ConfirmDerived("Type.name", fmt::format("matched {} names supplied to the Type constructor in inline and allocated form", samples.size()));
    layout.ConfirmDerived("Type.hash", fmt::format("matched {} chosen hashes immediately after the constructed std::string object", samples.size()));
    layout.ConfirmDerived("std::string.size", fmt::format("matched the lengths of {} names supplied to the Type constructor", samples.size()));
    layout.ConfirmDerived("std::string.capacity", fmt::format("matched inline and allocated capacities for {} constructor samples", samples.size()));
    layout.ConfirmDerived("std::string.inline_capacity", "matched the constructor's inline short name and separately allocated long name");
    layout.ConfirmDerived("std::string.object_size", "the constructor placed Type.hash immediately after the independently matched string object");
    return true;
}

std::optional<DiscoveryVote> ClientDiscovery::FindTypeConstructor(Machine const& machine, CodeIndex const& code, std::span<uint64 const> types, std::string& error)
{
    std::unordered_set<uint64> vtables;
    for (uint64 const type : types)
        vtables.insert(machine.ReadU64(type));
    std::map<uint64, uint64> votes;
    for (uint64 const vtable : vtables)
    {
        for (uint64 const site : code.LeaReferences(vtable))
        {
            std::vector<DecodedInstruction> const instructions = code.DecodeFunctionUntil(site);
            std::optional<uint64> target;
            for (DecodedInstruction const& instruction : instructions)
                if (instruction.Kind == InstructionKind::Call && instruction.BranchTarget && instruction.Address + VtableCallWindow >= site && instruction.Address + instruction.Length <= site)
                    target = instruction.BranchTarget;
            if (target)
                ++votes[*target];
        }
    }
    return Elect(votes, "Type constructor", error);
}

std::optional<DiscoveryVote> ClientDiscovery::FindPropertyListInitializer(Machine const& machine, CodeIndex const& code, std::span<uint64 const> types, ClientLayout const& layout, std::string& error)
{
    PeImage const& image = code.GetImage();
    uint64 const imageBase = code.GetImageBase();
    uint64 const imageEnd = imageBase + image.GetSizeOfImage();
    std::unordered_set<uint64> lists;
    for (uint64 const type : types)
    {
        uint64 const list = machine.ReadU64(type + layout.TypePropertyList);
        if (list >= imageBase && list < imageEnd)
            lists.insert(list);
    }
    std::map<uint64, uint64> votes;
    for (uint64 const list : lists)
    {
        std::span<uint64 const> const sites = code.LeaReferences(list);
        for (std::size_t i = 0; i < sites.size() && i < ListReferencesPerList; ++i)
        {
            std::vector<DecodedInstruction> const instructions = code.Decode(sites[i], ListCallWindowBytes, ListCallWindowInstructions);
            for (std::size_t k = 1; k < instructions.size(); ++k)
            {
                if (instructions[k].Kind == InstructionKind::Call && instructions[k].BranchTarget)
                {
                    ++votes[*instructions[k].BranchTarget];
                    break;
                }
            }
        }
    }
    return Elect(votes, "PropertyList initializer", error);
}
