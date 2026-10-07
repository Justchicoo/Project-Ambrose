/*
 * Project Ambrose by Imjustchico
 * The discovery steps that read the emulated process: the std::map node layout taken from the largest tree on the heap whose links, nil flag, red-black colors, rising keys and values holding and naming their keys all agree, allowing one alias node in 64 that shares another's Type, the type map head found from heap nodes whose type name hashes to their key, an in-order walk of that map, Type and std::string fields matched against values supplied to the Type constructor, and votes for the Type constructor and PropertyList initializer.
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

    constexpr std::array<uint64, 5> MapLinkSlots = { 0x00, 0x08, 0x10, 0x18, 0x20 };
    constexpr uint64 MapNodeWindow = 0x48;
    constexpr uint64 MapLinkReach = MapLinkSlots.back() + 8;
    constexpr uint64 MapValueWindow = 0x200;
    constexpr std::size_t MinimumDerivedMapNodes = 3;
    constexpr std::size_t MapAliasShare = 64;

    struct HeapView
    {
        std::vector<uint8> const& Bytes;
        GuestHeap const& Heap;
        uint64 Base = 0;

        uint64 SizeOf(uint64 address) const
        {
            std::optional<uint64> const size = Heap.SizeOf(address);
            return size && Holds(address, *size) ? *size : 0;
        }
        bool Holds(uint64 address, uint64 length) const
        {
            return address >= Base && address - Base <= Bytes.size() && length <= Bytes.size() - (address - Base);
        }
        uint64 U64(uint64 address) const { return Get64(Bytes, address - Base); }
        uint32 U32(uint64 address) const { return Get32(Bytes, address - Base); }
        uint8 U8(uint64 address) const { return Bytes[address - Base]; }
    };

    struct MapLinks
    {
        uint64 Left = 0;
        uint64 Parent = 0;
        uint64 Right = 0;
    };

    struct DerivedMap
    {
        uint64 Head = 0;
        MapLinks Links;
        uint64 Color = 0;
        uint64 IsNil = 0;
        uint64 Key = 0;
        uint64 Value = 0;
        uint64 KeyInValue = 0;
        uint64 NameInValue = 0;
        std::size_t Aliases = 0;
        std::vector<uint64> Nodes;
    };

    std::optional<std::vector<uint64>> WalkCandidateTree(HeapView const& view, uint64 head, uint64 root, MapLinks const& links)
    {
        std::vector<uint64> order;
        std::unordered_set<uint64> seen;
        std::vector<uint64> stack;
        auto child = [&](uint64 node, uint64 slot) -> std::optional<uint64>
        {
            uint64 const next = view.U64(node + slot);
            if (next == head)
                return uint64{ 0 };
            if (!view.Holds(next, MapLinkReach) || view.U64(next + links.Parent) != node)
                return std::nullopt;
            return next;
        };
        uint64 node = root;
        while (node || !stack.empty())
        {
            while (node)
            {
                if (!seen.insert(node).second || seen.size() > ClientDiscovery::MaxTypeMapNodes)
                    return std::nullopt;
                stack.push_back(node);
                std::optional<uint64> const left = child(node, links.Left);
                if (!left)
                    return std::nullopt;
                node = *left;
            }
            node = stack.back();
            stack.pop_back();
            order.push_back(node);
            std::optional<uint64> const right = child(node, links.Right);
            if (!right)
                return std::nullopt;
            node = *right;
        }
        if (order.empty() || view.U64(head + links.Left) != order.front() || view.U64(head + links.Right) != order.back())
            return std::nullopt;
        return order;
    }

    bool IsRedBlack(HeapView const& view, uint64 head, uint64 root, MapLinks const& links, uint64 color)
    {
        if (view.U8(root + color) != 1)
            return false;
        std::optional<uint64> leafBlacks;
        std::vector<std::pair<uint64, uint64>> stack = { { root, 0 } };
        while (!stack.empty())
        {
            auto const [node, above] = stack.back();
            stack.pop_back();
            uint8 const shade = view.U8(node + color);
            if (shade > 1)
                return false;
            uint64 const blacks = above + shade;
            for (uint64 const slot : { links.Left, links.Right })
            {
                uint64 const next = view.U64(node + slot);
                if (next == head)
                {
                    if (leafBlacks && *leafBlacks != blacks)
                        return false;
                    leafBlacks = blacks;
                }
                else
                {
                    if (shade == 0 && view.U8(next + color) == 0)
                        return false;
                    stack.emplace_back(next, blacks);
                }
            }
        }
        return true;
    }

    using Span = std::pair<uint64, uint64>;

    bool Overlaps(uint64 offset, uint64 length, std::span<Span const> taken)
    {
        for (auto const& [start, size] : taken)
            if (offset < start + size && start < offset + length)
                return true;
        return false;
    }

    bool NamedBy(HeapView const& view, uint64 object, uint64 slot, uint32 key)
    {
        auto hashes = [&](uint64 text, uint64 limit)
        {
            std::string name;
            for (uint64 at = text; name.size() < limit && view.Holds(at, 1); ++at)
            {
                char const c = static_cast<char>(view.U8(at));
                if (c == 0)
                    return !name.empty() && StringHash::KiStringHash(name) == key;
                name += c;
            }
            return false;
        };
        return hashes(object + slot, 16) || hashes(view.U64(object + slot), MaxTypeNameLength);
    }

    bool PlaceMapFields(HeapView const& view, DerivedMap& map)
    {
        MapLinks const& links = map.Links;
        std::size_t const tolerated = map.Nodes.size() / MapAliasShare;
        auto fewMisses = [&](auto&& holds)
        {
            std::size_t misses = 0;
            for (uint64 const node : map.Nodes)
                if (!holds(node) && ++misses > tolerated)
                    return false;
            return true;
        };
        uint64 nodeSize = view.SizeOf(map.Head);
        for (uint64 const node : map.Nodes)
            nodeSize = std::min(nodeSize, view.SizeOf(node));
        nodeSize = std::min(nodeSize, MapNodeWindow);
        if (nodeSize <= std::max({ links.Left, links.Parent, links.Right }) + 8)
            return false;
        std::array<Span, 3> const linkSlots = { Span{ links.Left, 8 }, Span{ links.Parent, 8 }, Span{ links.Right, 8 } };
        std::vector<std::array<uint64, 4>> placements;
        for (uint64 value = 0; value + 8 <= nodeSize; value += 8)
        {
            if (Overlaps(value, 8, linkSlots))
                continue;
            std::unordered_set<uint64> objects;
            uint64 valueSize = MapValueWindow;
            for (uint64 const node : map.Nodes)
            {
                uint64 const object = view.U64(node + value);
                valueSize = std::min(valueSize, view.SizeOf(object));
                if (valueSize < 4)
                    break;
                objects.insert(object);
            }
            if (valueSize < 4 || map.Nodes.size() - objects.size() > tolerated)
                continue;
            for (uint64 key = 0; key + 4 <= nodeSize; key += 4)
            {
                if (Overlaps(key, 4, linkSlots) || Overlaps(key, 4, std::array{ Span{ value, 8 } }))
                    continue;
                bool rising = true;
                for (std::size_t index = 1; index < map.Nodes.size() && rising; ++index)
                    rising = view.U32(map.Nodes[index - 1] + key) < view.U32(map.Nodes[index] + key);
                if (!rising)
                    continue;
                for (uint64 inValue = 0; inValue + 4 <= valueSize; inValue += 4)
                {
                    if (!fewMisses([&](uint64 node) { return view.U32(view.U64(node + value) + inValue) == view.U32(node + key); }))
                        continue;
                    for (uint64 name = 0; name + 8 <= valueSize; name += 8)
                        if (fewMisses([&](uint64 node) { return NamedBy(view, view.U64(node + value), name, view.U32(node + key)); }))
                        {
                            placements.push_back({ key, value, inValue, name });
                            break;
                        }
                    break;
                }
            }
        }
        if (placements.size() != 1)
            return false;
        map.Key = placements[0][0];
        map.Value = placements[0][1];
        map.KeyInValue = placements[0][2];
        map.NameInValue = placements[0][3];
        map.Aliases = static_cast<std::size_t>(std::count_if(map.Nodes.begin(), map.Nodes.end(), [&](uint64 node) { return view.U32(view.U64(node + map.Value) + map.KeyInValue) != view.U32(node + map.Key); }));

        uint64 const root = view.U64(map.Head + links.Parent);
        std::vector<uint64> nilFlags;
        std::vector<uint64> colors;
        for (uint64 offset = 0; offset < nodeSize; ++offset)
        {
            if (Overlaps(offset, 1, linkSlots) || Overlaps(offset, 1, std::array{ Span{ map.Key, 4 }, Span{ map.Value, 8 } }))
                continue;
            if (view.U8(map.Head + offset) == 1 && std::all_of(map.Nodes.begin(), map.Nodes.end(), [&](uint64 node) { return view.U8(node + offset) == 0; }))
                nilFlags.push_back(offset);
            else if (IsRedBlack(view, map.Head, root, links, offset))
                colors.push_back(offset);
        }
        if (nilFlags.size() != 1 || colors.size() != 1)
            return false;
        map.IsNil = nilFlags[0];
        map.Color = colors[0];
        return true;
    }
}

std::optional<uint64> ClientDiscovery::DeriveTypeMapLayout(Machine const& machine, GuestHeap const& heap, ClientLayout& layout, std::string& error)
{
    uint64 const base = heap.GetBase();
    uint64 const top = heap.GetTop();
    if (top <= base)
    {
        error = "the guest heap is empty, so no type map exists yet";
        return std::nullopt;
    }
    std::vector<uint8> const bytes = machine.ReadBytes(base, static_cast<std::size_t>(top - base));
    HeapView const view{ bytes, heap, base };

    std::vector<DerivedMap> maps;
    for (uint64 const parent : MapLinkSlots)
        for (uint64 head = base; view.Holds(head, MapLinkReach); head += 8)
        {
            uint64 const root = view.U64(head + parent);
            if (root == head || !view.Holds(root, MapLinkReach) || view.U64(root + parent) != head)
                continue;
            for (uint64 const left : MapLinkSlots)
                for (uint64 const right : MapLinkSlots)
                {
                    if (left == parent || right == parent || left == right)
                        continue;
                    MapLinks const links{ left, parent, right };
                    std::optional<std::vector<uint64>> nodes = WalkCandidateTree(view, head, root, links);
                    if (!nodes || nodes->size() < MinimumDerivedMapNodes)
                        continue;
                    DerivedMap map;
                    map.Head = head;
                    map.Links = links;
                    map.Nodes = std::move(*nodes);
                    if (PlaceMapFields(view, map))
                        maps.push_back(std::move(map));
                }
        }
    if (maps.empty())
    {
        error = "no std::map on the guest heap keeps its keys rising with each value holding its key and a name that hashes to it, so std::map.node.left could not be placed";
        return std::nullopt;
    }
    std::sort(maps.begin(), maps.end(), [](DerivedMap const& a, DerivedMap const& b) { return a.Nodes.size() > b.Nodes.size(); });
    if (maps.size() > 1 && maps[1].Nodes.size() == maps[0].Nodes.size())
    {
        error = fmt::format("two std::maps of {} nodes, at {:#x} and {:#x}, both look like the type map, so std::map.node.left could not be placed", maps[0].Nodes.size(), maps[0].Head, maps[1].Head);
        return std::nullopt;
    }
    DerivedMap const& map = maps[0];
    std::size_t const count = map.Nodes.size();
    std::string const runnerUp = maps.size() > 1 ? fmt::format("; the next largest such map has {} nodes", maps[1].Nodes.size()) : std::string();
    layout.MapNodeLeft = map.Links.Left;
    layout.MapNodeParent = map.Links.Parent;
    layout.MapNodeRight = map.Links.Right;
    layout.MapNodeColor = map.Color;
    layout.MapNodeIsNil = map.IsNil;
    layout.MapNodeKey = map.Key;
    layout.MapNodeValue = map.Value;
    layout.ConfirmDerived("std::map.node.left", fmt::format("the head at {:#x} points at the first of {} nodes walked in rising key order{}", map.Head, count, runnerUp));
    layout.ConfirmDerived("std::map.node.parent", fmt::format("every child of {} nodes points back at its parent, and the root and head point at each other", count));
    layout.ConfirmDerived("std::map.node.right", fmt::format("the head points at the last of {} nodes walked in rising key order", count));
    layout.ConfirmDerived("std::map.node.color", fmt::format("{} nodes form a red-black tree: a black root, no red node with a red child and one black height", count));
    layout.ConfirmDerived("std::map.node.is_nil", fmt::format("set on the head and clear on all {} nodes", count));
    layout.ConfirmDerived("std::map.node.key", fmt::format("rises through the in-order walk of {} nodes and, but for {} aliases, equals the 32-bit value at {:#x} in each node's value and the hash of the name at {:#x}", count, map.Aliases, map.KeyInValue, map.NameInValue));
    layout.ConfirmDerived("std::map.node.value", fmt::format("points {} nodes at heap objects, {} of them aliases sharing another node's object", count, map.Aliases));
    return map.Head;
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
