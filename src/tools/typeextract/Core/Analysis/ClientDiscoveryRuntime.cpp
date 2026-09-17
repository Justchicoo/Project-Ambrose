/*
 * Project Ambrose by Imjustchico
 * The discovery steps that read the emulated process: the type map head found from heap nodes whose type name hashes to their key, an in-order walk of that map, and the votes for the Type constructor and PropertyList initializer.
 */

#include "ClientDiscovery.h"
#include "CodeIndex.h"
#include "GuestHeap.h"
#include "Machine.h"
#include "PeImage.h"
#include "StringHash.h"

#include <fmt/format.h>

#include <algorithm>
#include <map>
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
