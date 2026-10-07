/*
 * Project Ambrose by Imjustchico
 * The discovery steps that read the emulated process: the std::map node layout taken from the largest tree on the heap whose links, nil flag, red-black colors, rising keys and values holding and naming their keys all agree, allowing one alias node in 64 that shares another's Type, the type map head found from heap nodes whose type name hashes to their key, an in-order walk of that map, Type and std::string fields matched against values supplied to the Type constructor, the property list link voted on by lists that name their class, the pointer flag taken from the byte set for pointer and shared pointer types, the property vector, its stride and Property name, type and hash voted on by properties whose hash is that of their type's name and their own, Property id from each property's position in its list, the container slot and its name and dynamic vtable entries from what each kind of container returns when called on a copy, the PropertyList constructor found among what the lazy getters' class constructors call with their own object and the base, singleton and name its chosen arguments place, enum option vectors whose entries are a name and a mostly numeric value, Property offset and flags placed by values chosen for a property adder, and votes for the Type constructor and PropertyList initializer.
 */

#include "ClientDiscovery.h"
#include "CodeIndex.h"
#include "GuestHeap.h"
#include "Machine.h"
#include "PeImage.h"
#include "StringHash.h"
#include "TypeWalker.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

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

namespace
{
    constexpr uint64 TypeFieldWindow = 0x100;
    constexpr uint64 ListNameWindow = 0x100;
    constexpr std::size_t ListSampleTypes = 256;
    constexpr uint64 ListObjectWindow = 0x100;
    constexpr std::size_t ListConstructorInstructions = 4000;
    constexpr std::size_t ListConstructorCandidates = 8;

    struct TypeSnapshot
    {
        uint64 Address = 0;
        std::string Name;
        std::vector<uint8> Bytes;
    };

    std::optional<std::string> ReadLayoutString(Machine const& machine, uint64 address, ClientLayout const& layout)
    {
        std::optional<uint64> const length = TryReadU64(machine, address + layout.StringSize);
        std::optional<uint64> const capacity = TryReadU64(machine, address + layout.StringCapacity);
        if (!length || !capacity || *length > MaxTypeNameLength || *capacity < *length)
            return std::nullopt;
        uint64 text = address;
        if (*capacity > layout.StringInlineCapacity)
        {
            std::optional<uint64> const pointer = TryReadU64(machine, address);
            if (!pointer)
                return std::nullopt;
            text = *pointer;
        }
        std::vector<uint8> bytes(static_cast<std::size_t>(*length));
        if (!machine.TryRead(text, bytes))
            return std::nullopt;
        return std::string(bytes.begin(), bytes.end());
    }

    std::vector<TypeSnapshot> SnapshotTypes(Machine const& machine, GuestHeap const& heap, std::span<uint64 const> types, ClientLayout const& layout)
    {
        uint64 size = TypeFieldWindow;
        for (uint64 const type : types)
            if (std::optional<uint64> const allocated = heap.SizeOf(type))
                size = std::min(size, *allocated);
        std::vector<TypeSnapshot> snapshots;
        std::unordered_set<uint64> seen;
        for (uint64 const type : types)
        {
            if (!seen.insert(type).second)
                continue;
            std::optional<std::string> name = ReadLayoutString(machine, type + layout.TypeName, layout);
            std::vector<uint8> bytes(static_cast<std::size_t>(size));
            if (name && !name->empty() && machine.TryRead(type, bytes))
                snapshots.push_back({ type, std::move(*name), std::move(bytes) });
        }
        return snapshots;
    }

    bool NamesAPointer(std::string_view name)
    {
        return name.ends_with('*') || (name.starts_with("class SharedPointer<") && name.ends_with('>'));
    }

    bool TypeFieldTaken(ClientLayout const& layout, uint64 offset, uint64 length)
    {
        std::array const taken = { Span{ layout.TypeName, layout.StringObjectSize }, Span{ layout.TypeHash, 4 } };
        return Overlaps(offset, length, taken);
    }
}

bool ClientDiscovery::DerivePropertyListLink(Machine const& machine, GuestHeap const& heap, std::span<uint64 const> types, ClientLayout& layout, std::string& error)
{
    std::vector<TypeSnapshot> const snapshots = SnapshotTypes(machine, heap, types, layout);
    uint64 const size = snapshots.empty() ? 0 : snapshots.front().Bytes.size();
    std::map<uint64, uint64> votes;
    for (uint64 slot = 0; slot + 8 <= size; slot += 8)
    {
        if (TypeFieldTaken(layout, slot, 8))
            continue;
        std::size_t sampled = 0;
        for (TypeSnapshot const& type : snapshots)
        {
            uint64 const list = Get64(type.Bytes, slot);
            if (!list || list == type.Address)
                continue;
            if (++sampled > ListSampleTypes)
                break;
            std::string const expected = TypeWalker::ListNameOf(type.Name);
            for (uint64 name = 0; name < ListNameWindow; name += 8)
                if (ReadLayoutString(machine, list + name, layout) == expected)
                {
                    ++votes[(slot << 16) | name];
                    break;
                }
        }
    }
    std::optional<DiscoveryVote> const vote = Elect(votes, "Type.property_list and PropertyList.name", error);
    if (!vote)
    {
        error = fmt::format("Type.property_list could not be placed: {}", error);
        return false;
    }
    uint64 const slot = vote->Winner >> 16;
    uint64 const name = vote->Winner & 0xFFFF;
    std::size_t lists = 0;
    std::size_t named = 0;
    for (TypeSnapshot const& type : snapshots)
        if (uint64 const list = Get64(type.Bytes, slot); list)
        {
            ++lists;
            if (ReadLayoutString(machine, list + name, layout) == TypeWalker::ListNameOf(type.Name))
                ++named;
        }
    if (named != lists)
    {
        error = fmt::format("Type.property_list could not be placed: {} of the {} lists at {:#x} in a Type do not hold their class's name at {:#x}", lists - named, lists, slot, name);
        return false;
    }
    layout.TypePropertyList = slot;
    layout.ListName = name;
    std::string const evidence = fmt::format("each of {} types with a list points at {:#x} to one whose string at {:#x} names the type's class ({} sampled votes, runner-up {})", lists, slot, name, vote->WinnerVotes, vote->RunnerUpVotes);
    layout.ConfirmDerived("Type.property_list", evidence);
    layout.ConfirmDerived("PropertyList.name", evidence);
    return true;
}

bool ClientDiscovery::DeriveTypePointerFlag(Machine const& machine, GuestHeap const& heap, std::span<uint64 const> types, ClientLayout& layout, std::string& error)
{
    std::vector<TypeSnapshot> const snapshots = SnapshotTypes(machine, heap, types, layout);
    std::size_t const pointers = static_cast<std::size_t>(std::count_if(snapshots.begin(), snapshots.end(), [](TypeSnapshot const& type) { return NamesAPointer(type.Name); }));
    if (pointers == 0 || pointers == snapshots.size())
    {
        error = fmt::format("Type.pointer could not be placed: {} of {} registered types are pointers, and it takes both kinds", pointers, snapshots.size());
        return false;
    }
    uint64 const size = snapshots.front().Bytes.size();
    std::size_t const tolerated = snapshots.size() / MapAliasShare;
    std::vector<uint64> flags;
    uint64 closest = 0;
    std::size_t fewestMisses = std::numeric_limits<std::size_t>::max();
    for (uint64 offset = 0; offset < size; ++offset)
    {
        if (TypeFieldTaken(layout, offset, 1) || Overlaps(offset, 1, std::array{ Span{ layout.TypePropertyList, 8 } }))
            continue;
        std::size_t const misses = static_cast<std::size_t>(std::count_if(snapshots.begin(), snapshots.end(), [&](TypeSnapshot const& type) { return type.Bytes[offset] != (NamesAPointer(type.Name) ? 1 : 0); }));
        if (misses <= tolerated)
            flags.push_back(offset);
        if (misses < fewestMisses)
        {
            fewestMisses = misses;
            closest = offset;
        }
    }
    std::vector<std::string> exceptions;
    for (TypeSnapshot const& type : snapshots)
        if (exceptions.size() < 4 && type.Bytes[closest] != (NamesAPointer(type.Name) ? 1 : 0))
            exceptions.push_back(fmt::format("{} ({})", type.Name, type.Bytes[closest]));
    if (flags.size() != 1)
    {
        error = fmt::format("Type.pointer could not be placed: {} bytes of a Type are 1 for the {} pointer types among {} with at most {} exceptions; the closest, at {:#x}, differs on {}, such as {}",
            flags.size(), pointers, snapshots.size(), tolerated, closest, fewestMisses, fmt::join(exceptions, ", "));
        return false;
    }
    layout.TypePointer = flags[0];
    layout.ConfirmDerived("Type.pointer", fmt::format("the one byte that is 1 for the {} types named as pointers or shared pointers and 0 for the other {}, but for {} exceptions{}{}",
        pointers, snapshots.size() - pointers, fewestMisses, exceptions.empty() ? "" : ": ", fmt::join(exceptions, ", ")));
    return true;
}

std::vector<uint64> ClientDiscovery::FindPropertyListConstructorCandidates(CodeIndex const& code, std::span<uint64 const> lists, uint64 finalizer, std::span<uint64 const> known)
{
    std::unordered_set<uint64> const listSet(lists.begin(), lists.end());
    std::unordered_set<uint64> classConstructors;
    for (uint64 const getter : FunctionsCalling(code, finalizer))
    {
        std::optional<uint64> listInRcx;
        std::vector<std::pair<uint64, uint64>> callsOnLists;
        for (DecodedInstruction const& instruction : code.DecodeFunctionFrom(getter, ListConstructorInstructions))
        {
            if (instruction.Kind == InstructionKind::Call)
            {
                if (instruction.BranchTarget && listInRcx)
                    callsOnLists.emplace_back(*listInRcx, *instruction.BranchTarget);
                listInRcx.reset();
                continue;
            }
            if (instruction.FirstRegisterFamily != "rcx" || !instruction.WritesFirstOperand || instruction.FirstOperandIsMemory)
                continue;
            if (instruction.Kind == InstructionKind::Lea && instruction.RipRelativeTarget && listSet.contains(*instruction.RipRelativeTarget))
                listInRcx = instruction.RipRelativeTarget;
            else
                listInRcx.reset();
        }
        for (auto const& [list, target] : callsOnLists)
            if (target != finalizer && std::find(known.begin(), known.end(), target) == known.end()
                && std::any_of(callsOnLists.begin(), callsOnLists.end(), [&](auto const& call) { return call.first == list && call.second == finalizer; }))
                classConstructors.insert(target);
    }
    std::unordered_map<uint64, std::vector<uint64>> thisCallees;
    auto calledWithThis = [&](uint64 function) -> std::vector<uint64> const&
    {
        if (auto const found = thisCallees.find(function); found != thisCallees.end())
            return found->second;
        std::vector<uint64> called;
        std::unordered_set<std::string> holdingThis = { "rcx" };
        for (DecodedInstruction const& instruction : code.DecodeFunctionFrom(function, ListConstructorInstructions))
        {
            if (instruction.Kind == InstructionKind::Call)
            {
                if (instruction.BranchTarget && holdingThis.contains("rcx") && std::find(known.begin(), known.end(), *instruction.BranchTarget) == known.end()
                    && std::find(called.begin(), called.end(), *instruction.BranchTarget) == called.end())
                    called.push_back(*instruction.BranchTarget);
                for (std::string_view const volatileRegister : { "rax", "rcx", "rdx", "r8", "r9", "r10", "r11" })
                    holdingThis.erase(std::string(volatileRegister));
                continue;
            }
            if (!instruction.WritesFirstOperand || instruction.FirstOperandIsMemory || instruction.FirstRegisterFamily.empty())
                continue;
            if (instruction.Kind == InstructionKind::Mov && holdingThis.contains(instruction.SecondRegisterFamily) && instruction.SecondRegisterFamily == instruction.SecondRegister)
                holdingThis.insert(instruction.FirstRegisterFamily);
            else
                holdingThis.erase(instruction.FirstRegisterFamily);
        }
        return thisCallees.emplace(function, std::move(called)).first->second;
    };
    std::map<uint64, uint64> callers;
    for (uint64 const constructor : classConstructors)
    {
        std::unordered_set<uint64> reached;
        for (uint64 const callee : std::vector<uint64>(calledWithThis(constructor)))
        {
            reached.insert(callee);
            for (uint64 const inner : std::vector<uint64>(calledWithThis(callee)))
                reached.insert(inner);
        }
        for (uint64 const target : reached)
            ++callers[target];
    }
    std::vector<std::pair<uint64, uint64>> ranked(callers.begin(), callers.end());
    std::sort(ranked.begin(), ranked.end(), [](auto const& a, auto const& b) { return a.second != b.second ? a.second > b.second : a.first < b.first; });
    std::vector<uint64> candidates;
    for (auto const& [target, count] : ranked)
        if (candidates.size() < ListConstructorCandidates && count >= MinimumWinnerVotes)
            candidates.push_back(target);
    return candidates;
}

bool ClientDiscovery::DeriveConstructedListLayout(Machine const& machine, std::span<ConstructedListSample const> samples, ClientLayout& layout, std::string& error)
{
    bool const bothSingletons = std::any_of(samples.begin(), samples.end(), [](ConstructedListSample const& s) { return s.Singleton; })
        && std::any_of(samples.begin(), samples.end(), [](ConstructedListSample const& s) { return !s.Singleton; });
    if (samples.size() < 2 || !bothSingletons)
    {
        error = "PropertyList.singleton needs constructor samples with and without the singleton flag";
        return false;
    }
    std::vector<std::vector<uint8>> objects;
    for (ConstructedListSample const& sample : samples)
    {
        std::vector<uint8>& bytes = objects.emplace_back(static_cast<std::size_t>(ListObjectWindow));
        if (!machine.TryRead(sample.Address, bytes))
        {
            error = fmt::format("the constructed list at {:#x} cannot be read", sample.Address);
            return false;
        }
    }
    std::vector<uint64> bases;
    for (uint64 offset = 0; offset + 8 <= ListObjectWindow; offset += 8)
        if (std::all_of(samples.begin(), samples.end(), [&, index = std::size_t{ 0 }](ConstructedListSample const& sample) mutable { return Get64(objects[index++], offset) == sample.Base; }))
            bases.push_back(offset);
    std::vector<uint64> names;
    for (uint64 offset = 0; offset + layout.StringObjectSize <= ListObjectWindow; offset += 8)
        if (std::all_of(samples.begin(), samples.end(), [&](ConstructedListSample const& sample) { return ReadLayoutString(machine, sample.Address + offset, layout) == sample.Name; }))
            names.push_back(offset);
    std::vector<uint64> singletons;
    for (uint64 offset = 0; offset < ListObjectWindow; ++offset)
    {
        bool const outside = std::none_of(bases.begin(), bases.end(), [&](uint64 base) { return offset >= base && offset < base + 8; })
            && std::none_of(names.begin(), names.end(), [&](uint64 name) { return offset >= name && offset < name + layout.StringObjectSize; });
        if (outside && std::all_of(samples.begin(), samples.end(), [&, index = std::size_t{ 0 }](ConstructedListSample const& sample) mutable { return objects[index++][offset] == (sample.Singleton ? 1 : 0); }))
            singletons.push_back(offset);
    }
    if (bases.size() != 1 || names.size() != 1 || singletons.size() != 1)
    {
        error = fmt::format("the PropertyList constructor's samples place the base at {} offsets, the name at {} and the singleton flag at {}", bases.size(), names.size(), singletons.size());
        return false;
    }
    if (layout.DerivedFields.contains("PropertyList.name") && layout.ListName != names[0])
    {
        error = fmt::format("the PropertyList constructor wrote the name at {:#x}, but the registered lists hold it at {:#x}", names[0], layout.ListName);
        return false;
    }
    layout.ListBase = bases[0];
    layout.ListSingleton = singletons[0];
    layout.ListName = names[0];
    std::string const evidence = fmt::format("{} lists built by the client's PropertyList constructor with chosen bases and singleton flags", samples.size());
    layout.ConfirmDerived("PropertyList.base", fmt::format("{}: the chosen base landed at {:#x}", evidence, bases[0]));
    layout.ConfirmDerived("PropertyList.singleton", fmt::format("{}: only the byte at {:#x} followed the chosen flag", evidence, singletons[0]));
    if (!layout.DerivedFields.contains("PropertyList.name"))
        layout.ConfirmDerived("PropertyList.name", fmt::format("{}: the class name without its class keyword landed at {:#x}", evidence, names[0]));
    return true;
}

namespace
{
    constexpr std::array<uint64, 4> ListEntryStrides = { 8, 0x10, 0x18, 0x20 };
    constexpr std::size_t PropertySampleLists = 256;
    constexpr std::size_t PropertySampleEntries = 4;
    constexpr uint64 PropertyObjectWindow = 0x100;
    constexpr uint64 MaxListEntries = 4096;
    constexpr std::size_t PropertyLayoutCandidates = 8;

    using PropertyFields = std::array<uint64, 3>;

    bool IsPropertyName(std::string_view text)
    {
        return !text.empty() && std::all_of(text.begin(), text.end(), [](char c) { return c > ' ' && c < 0x7F; });
    }

    std::string const* TypeNameAt(Machine const& machine, ClientLayout const& layout, uint64 address, std::unordered_map<uint64, std::optional<std::string>>& typeNames)
    {
        if (auto const known = typeNames.find(address); known != typeNames.end())
            return known->second ? &*known->second : nullptr;
        std::optional<std::string> name;
        if (address >= Machine::PageSize)
        {
            name = ReadLayoutString(machine, address + layout.TypeName, layout);
            std::optional<uint64> const hash = TryReadU64(machine, address + layout.TypeHash);
            if (name && (name->empty() || !hash || static_cast<uint32>(*hash) != StringHash::KiStringHash(*name)))
                name.reset();
        }
        std::optional<std::string> const& stored = typeNames.emplace(address, std::move(name)).first->second;
        return stored ? &*stored : nullptr;
    }

    std::vector<PropertyFields> PlaceNameTypeHash(Machine const& machine, GuestHeap const& heap, ClientLayout const& layout, uint64 property, std::unordered_map<uint64, std::optional<std::string>>& typeNames)
    {
        uint64 const size = std::min(heap.SizeOf(property).value_or(PropertyObjectWindow), PropertyObjectWindow);
        std::vector<uint8> bytes(static_cast<std::size_t>(size));
        if (size < 8 || !machine.TryRead(property, bytes))
            return {};
        std::vector<std::pair<uint64, std::string>> names;
        std::vector<std::pair<uint64, std::string const*>> types;
        for (uint64 offset = 0; offset + 8 <= size; offset += 8)
        {
            uint64 const pointer = Get64(bytes, offset);
            if (std::string const* const type = TypeNameAt(machine, layout, pointer, typeNames))
                types.emplace_back(offset, type);
            else if (pointer >= Machine::PageSize)
            {
                std::optional<std::string> name = machine.ReadCString(pointer, MaxTypeNameLength);
                if (name && IsPropertyName(*name))
                    names.emplace_back(offset, std::move(*name));
            }
        }
        std::vector<PropertyFields> placed;
        for (auto const& [nameOffset, name] : names)
            for (auto const& [typeOffset, typeName] : types)
            {
                uint32 const hash = StringHash::PropertyHash(*typeName, name);
                for (uint64 offset = 0; offset + 4 <= size; offset += 4)
                    if (Get32(bytes, offset) == hash)
                        placed.push_back({ nameOffset, typeOffset, offset });
            }
        return placed;
    }
}

bool ClientDiscovery::DerivePropertyLayout(Machine const& machine, GuestHeap const& heap, std::span<uint64 const> types, ClientLayout& layout, std::string& error)
{
    std::unordered_map<uint64, std::optional<std::string>> typeNames;
    std::vector<uint64> lists;
    std::unordered_set<uint64> seenLists;
    for (TypeSnapshot& type : SnapshotTypes(machine, heap, types, layout))
    {
        if (std::optional<uint64> const list = TryReadU64(machine, type.Address + layout.TypePropertyList); list && *list && seenLists.insert(*list).second)
            lists.push_back(*list);
        typeNames.emplace(type.Address, std::move(type.Name));
    }
    std::sort(lists.begin(), lists.end());

    std::array const taken = { Span{ layout.ListBase, 8 }, Span{ layout.ListName, layout.StringObjectSize }, Span{ layout.ListSingleton, 1 } };
    std::unordered_map<uint64, std::vector<PropertyFields>> placedByProperty;
    auto placed = [&](uint64 property) -> std::vector<PropertyFields> const&
    {
        if (auto const found = placedByProperty.find(property); found != placedByProperty.end())
            return found->second;
        return placedByProperty.emplace(property, PlaceNameTypeHash(machine, heap, layout, property, typeNames)).first->second;
    };
    auto entries = [&](uint64 list, uint64 slot, uint64 stride) -> std::optional<std::pair<uint64, uint64>>
    {
        std::optional<uint64> const begin = TryReadU64(machine, list + slot);
        std::optional<uint64> const end = TryReadU64(machine, list + slot + 8);
        if (!begin || !end || *end < *begin || (*end - *begin) % stride != 0 || (*end - *begin) / stride > MaxListEntries || (*begin == 0) != (*end == 0))
            return std::nullopt;
        return std::pair{ *begin, (*end - *begin) / stride };
    };
    std::map<std::array<uint64, 5>, uint64> votes;
    for (uint64 slot = 0; slot + 16 <= ListObjectWindow; slot += 8)
    {
        if (Overlaps(slot, 16, taken))
            continue;
        for (uint64 const stride : ListEntryStrides)
        {
            std::size_t sampled = 0;
            for (uint64 const list : lists)
            {
                std::optional<std::pair<uint64, uint64>> const vector = entries(list, slot, stride);
                if (!vector || vector->second == 0)
                    continue;
                if (++sampled > PropertySampleLists)
                    break;
                for (uint64 index = 0; index < vector->second && index < PropertySampleEntries; ++index)
                    if (std::optional<uint64> const property = TryReadU64(machine, vector->first + index * stride))
                        for (PropertyFields const& fields : placed(*property))
                            ++votes[{ slot, stride, fields[0], fields[1], fields[2] }];
            }
        }
    }
    std::vector<std::pair<std::array<uint64, 5>, uint64>> ranked(votes.begin(), votes.end());
    std::sort(ranked.begin(), ranked.end(), [](auto const& a, auto const& b) { return a.second > b.second; });
    std::vector<std::string> rejected;
    std::vector<PropertyFields> const none;
    for (std::size_t rank = 0; rank < ranked.size() && rank < PropertyLayoutCandidates && ranked[rank].second >= MinimumWinnerVotes; ++rank)
    {
        auto const& [key, count] = ranked[rank];
        auto const [slot, stride, name, typeSlot, hash] = key;
        std::size_t properties = 0;
        std::size_t misses = 0;
        std::size_t badVectors = 0;
        for (uint64 const list : lists)
        {
            std::optional<std::pair<uint64, uint64>> const vector = entries(list, slot, stride);
            if (!vector)
            {
                ++badVectors;
                continue;
            }
            for (uint64 index = 0; index < vector->second; ++index)
            {
                ++properties;
                std::optional<uint64> const property = TryReadU64(machine, vector->first + index * stride);
                std::vector<PropertyFields> const& fields = property ? placed(*property) : none;
                if (std::find(fields.begin(), fields.end(), PropertyFields{ name, typeSlot, hash }) == fields.end())
                    ++misses;
            }
        }
        if (badVectors != 0 || misses > properties / MapAliasShare)
        {
            rejected.push_back(fmt::format("vector {:#x} stride {:#x}: {} lists without such a vector, {} of {} properties not named, typed and hashed there", slot, stride, badVectors, misses, properties));
            continue;
        }
        layout.ListProperties = slot;
        layout.ListEntrySize = stride;
        layout.PropertyName = name;
        layout.PropertyType = typeSlot;
        layout.PropertyHash = hash;
        std::string const evidence = fmt::format("{} of the {} properties in the vectors at {:#x} of {} lists, {:#x} bytes apart, point at a type at {:#x} and a C string at {:#x}, and hold the property hash of the two at {:#x} ({} sampled votes)",
            properties - misses, properties, slot, lists.size(), stride, typeSlot, name, hash, count);
        for (std::string_view const field : { "PropertyList.properties", "PropertyList.entry_size", "Property.name", "Property.type", "Property.hash" })
            layout.ConfirmDerived(std::string(field), evidence);
        return true;
    }
    error = fmt::format("PropertyList.properties could not be placed: {} candidates had {} votes or more{}{}", rejected.size(), MinimumWinnerVotes, rejected.empty() ? "" : ": ", fmt::join(rejected, "; "));
    return false;
}

namespace
{
    constexpr std::size_t MaxContainers = 4096;
    constexpr uint64 ContainerVtableSlots = 16;
    constexpr std::array<std::string_view, 3> ContainerNames = { "Static", "Vector", "List" };
}

std::vector<ListedProperty> ClientDiscovery::ListedProperties(Machine const& machine, GuestHeap const& heap, std::span<uint64 const> types, ClientLayout const& layout)
{
    std::vector<ListedProperty> properties;
    std::unordered_set<uint64> seenLists;
    for (TypeSnapshot const& type : SnapshotTypes(machine, heap, types, layout))
    {
        std::optional<uint64> const list = TryReadU64(machine, type.Address + layout.TypePropertyList);
        if (!list || !*list || !seenLists.insert(*list).second)
            continue;
        std::optional<uint64> const begin = TryReadU64(machine, *list + layout.ListProperties);
        std::optional<uint64> const end = TryReadU64(machine, *list + layout.ListProperties + 8);
        if (!begin || !end || *end < *begin)
            continue;
        uint64 index = 0;
        for (uint64 slot = *begin; slot < *end && index < MaxListEntries; slot += layout.ListEntrySize, ++index)
            if (std::optional<uint64> const property = TryReadU64(machine, slot))
                properties.push_back({ *property, index });
    }
    return properties;
}

bool ClientDiscovery::DerivePropertyId(Machine const& machine, std::span<ListedProperty const> properties, ClientLayout& layout, std::string& error)
{
    std::array const taken = { Span{ layout.PropertyName, 8 }, Span{ layout.PropertyType, 8 }, Span{ layout.PropertyHash, 4 } };
    std::size_t const positions = static_cast<std::size_t>(std::count_if(properties.begin(), properties.end(), [](ListedProperty const& p) { return p.Index != 0; }));
    if (positions == 0)
    {
        error = "Property.id could not be placed: no list holds more than one property";
        return false;
    }
    std::vector<uint64> ids;
    for (uint64 offset = 0; offset + 4 <= PropertyObjectWindow; offset += 4)
    {
        if (Overlaps(offset, 4, taken))
            continue;
        if (std::all_of(properties.begin(), properties.end(), [&](ListedProperty const& p)
            {
                std::optional<uint64> const value = TryReadU64(machine, p.Address + offset);
                return value && static_cast<uint32>(*value) == p.Index;
            }))
            ids.push_back(offset);
    }
    if (ids.size() != 1)
    {
        error = fmt::format("Property.id could not be placed: {} offsets of a property hold its position in the list for all {}", ids.size(), properties.size());
        return false;
    }
    layout.PropertyId = ids[0];
    layout.ConfirmDerived("Property.id", fmt::format("the one 32-bit word that holds each of {} properties' position in its list", properties.size()));
    return true;
}

bool ClientDiscovery::DeriveContainerLayout(Machine const& machine, std::span<ListedProperty const> properties, ClientLayout& layout, GuestCall const& call, std::string& error)
{
    std::array const taken = { Span{ layout.PropertyName, 8 }, Span{ layout.PropertyType, 8 }, Span{ layout.PropertyHash, 4 }, Span{ layout.PropertyId, 4 } };
    struct Placement
    {
        uint64 Slot = 0;
        uint64 NameSlot = 0;
        uint64 DynamicSlot = 0;
        std::size_t Containers = 0;
    };
    std::vector<Placement> placements;
    std::vector<std::string> rejected;
    for (uint64 slot = 0; slot + 8 <= PropertyObjectWindow; slot += 8)
    {
        if (Overlaps(slot, 8, taken))
            continue;
        std::vector<uint64> containers;
        std::unordered_set<uint64> vtables;
        bool shared = true;
        for (ListedProperty const& property : properties)
        {
            std::optional<uint64> const container = TryReadU64(machine, property.Address + slot);
            std::optional<uint64> const vtable = container && *container ? TryReadU64(machine, *container) : std::nullopt;
            if (!vtable || *vtable < Machine::PageSize)
            {
                shared = false;
                break;
            }
            if (vtables.insert(*vtable).second)
            {
                containers.push_back(*container);
                if (containers.size() > MaxContainers)
                {
                    shared = false;
                    break;
                }
            }
        }
        if (!shared || containers.size() < 2)
            continue;
        std::vector<std::array<std::optional<std::optional<uint64>>, ContainerVtableSlots>> results(containers.size());
        auto result = [&](std::size_t index, uint64 k) -> std::optional<uint64>
        {
            std::optional<std::optional<uint64>>& cell = results[index][k];
            if (!cell)
            {
                std::optional<uint64> const function = TryReadU64(machine, machine.ReadU64(containers[index]) + 8 * k);
                cell = function && *function >= Machine::PageSize ? call(*function, containers[index]) : std::nullopt;
            }
            return *cell;
        };
        std::vector<uint64> nameSlots;
        std::vector<std::string> names(containers.size());
        for (uint64 k = 0; k < ContainerVtableSlots; ++k)
        {
            std::vector<std::string> found;
            for (std::size_t index = 0; index < containers.size(); ++index)
            {
                std::optional<uint64> const returned = result(index, k);
                std::optional<std::string> const name = returned ? machine.ReadCString(*returned, 256) : std::nullopt;
                if (!name || std::find(ContainerNames.begin(), ContainerNames.end(), *name) == ContainerNames.end())
                    break;
                found.push_back(*name);
            }
            if (found.size() == containers.size())
            {
                nameSlots.push_back(k);
                names = std::move(found);
            }
        }
        bool const bothKinds = std::count(names.begin(), names.end(), "Static") != 0 && std::count(names.begin(), names.end(), "Static") != static_cast<std::ptrdiff_t>(names.size());
        std::vector<uint64> dynamicSlots;
        for (uint64 k = 0; nameSlots.size() == 1 && bothKinds && k < ContainerVtableSlots; ++k)
        {
            if (k == nameSlots[0])
                continue;
            bool matches = true;
            for (std::size_t index = 0; index < containers.size() && matches; ++index)
            {
                std::optional<uint64> const returned = result(index, k);
                matches = returned && (*returned & 0xFF) == (names[index] == "Static" ? 0u : 1u);
            }
            if (matches)
                dynamicSlots.push_back(k);
        }
        if (nameSlots.size() == 1 && dynamicSlots.size() == 1)
            placements.push_back({ slot, nameSlots[0], dynamicSlots[0], containers.size() });
        else
            rejected.push_back(fmt::format("{:#x}: {} containers, {} name slots, {} dynamic slots", slot, containers.size(), nameSlots.size(), dynamicSlots.size()));
    }
    bool const mirrored = placements.size() > 1 && std::all_of(properties.begin(), properties.end(), [&](ListedProperty const& property)
    {
        uint64 const first = machine.ReadU64(property.Address + placements[0].Slot);
        return std::all_of(placements.begin() + 1, placements.end(), [&](Placement const& other) { return machine.ReadU64(property.Address + other.Slot) == first; });
    });
    std::vector<std::string> mirrors;
    if (mirrored)
    {
        for (auto other = placements.begin() + 1; other != placements.end(); ++other)
            mirrors.push_back(fmt::format("{:#x}", other->Slot));
        placements.resize(1);
    }
    if (placements.size() != 1)
    {
        std::vector<std::string> placed;
        for (Placement const& placement : placements)
            placed.push_back(fmt::format("{:#x} ({} kinds, name entry {}, dynamic entry {})", placement.Slot, placement.Containers, placement.NameSlot, placement.DynamicSlot));
        error = fmt::format("Property.container could not be placed: {} slots of a property point at containers that name themselves{}{}{}{}", placements.size(),
            placed.empty() ? "" : ": ", fmt::join(placed, ", "), rejected.empty() ? "" : "; ", fmt::join(rejected, "; "));
        return false;
    }
    Placement const& placement = placements[0];
    layout.PropertyContainer = placement.Slot;
    layout.ContainerNameSlot = placement.NameSlot;
    layout.ContainerDynamicSlot = placement.DynamicSlot;
    std::string const evidence = fmt::format("all {} properties point at {:#x} to a container of one of {} kinds, whose vtable entry {} returns Static, Vector or List and entry {} is clear for Static alone{}{}",
        properties.size(), placement.Slot, placement.Containers, placement.NameSlot, placement.DynamicSlot, mirrors.empty() ? "" : "; every property holds the same pointer again at ", fmt::join(mirrors, ", "));
    for (std::string_view const field : { "Property.container", "Container.name_slot", "Container.dynamic_slot" })
        layout.ConfirmDerived(std::string(field), evidence);
    return true;
}

namespace
{
    constexpr std::size_t PropertyAdderCandidates = 12;
}

std::vector<uint64> ClientDiscovery::FindPropertyAdderCandidates(CodeIndex const& code, uint64 finalizer)
{
    std::map<uint64, uint64> callers;
    for (uint64 const getter : FunctionsCalling(code, finalizer))
    {
        bool finalized = false;
        bool textInRdx = false;
        std::unordered_set<uint64> adders;
        for (DecodedInstruction const& instruction : code.DecodeFunctionFrom(getter, ListConstructorInstructions))
        {
            if (instruction.Kind == InstructionKind::Call)
            {
                if (instruction.BranchTarget && *instruction.BranchTarget == finalizer)
                    finalized = true;
                else if (instruction.BranchTarget && finalized && textInRdx)
                    adders.insert(*instruction.BranchTarget);
                textInRdx = false;
                continue;
            }
            if (instruction.FirstRegisterFamily == "rdx" && instruction.WritesFirstOperand && !instruction.FirstOperandIsMemory)
                textInRdx = instruction.Kind == InstructionKind::Lea && instruction.RipRelativeTarget.has_value();
        }
        for (uint64 const adder : adders)
            ++callers[adder];
    }
    std::vector<std::pair<uint64, uint64>> ranked(callers.begin(), callers.end());
    std::sort(ranked.begin(), ranked.end(), [](auto const& a, auto const& b) { return a.second != b.second ? a.second > b.second : a.first < b.first; });
    std::vector<uint64> candidates;
    for (auto const& [adder, count] : ranked)
        if (candidates.size() < PropertyAdderCandidates)
            candidates.push_back(adder);
    return candidates;
}

bool ClientDiscovery::DeriveConstructedPropertyLayout(Machine const& machine, std::span<ConstructedPropertySample const> samples, ClientLayout& layout, std::string& error)
{
    if (samples.size() < 2)
    {
        error = "Property.offset needs at least two properties built with chosen offsets and flags";
        return false;
    }
    std::array const taken = { Span{ layout.PropertyName, 8 }, Span{ layout.PropertyType, 8 }, Span{ layout.PropertyHash, 4 }, Span{ layout.PropertyId, 4 }, Span{ layout.PropertyContainer, 8 } };
    auto place = [&](auto&& chosen) -> std::vector<uint64>
    {
        std::vector<uint64> found;
        for (uint64 offset = 0; offset + 4 <= PropertyObjectWindow; offset += 4)
            if (!Overlaps(offset, 4, taken) && std::all_of(samples.begin(), samples.end(), [&](ConstructedPropertySample const& sample)
                {
                    std::optional<uint64> const value = TryReadU64(machine, sample.Address + offset);
                    return value && static_cast<uint32>(*value) == chosen(sample);
                }))
                found.push_back(offset);
        return found;
    };
    std::vector<uint64> const offsets = place([](ConstructedPropertySample const& sample) { return sample.Offset; });
    std::vector<uint64> const flags = place([](ConstructedPropertySample const& sample) { return sample.Flags; });
    if (offsets.size() != 1 || flags.size() != 1)
    {
        error = fmt::format("the property adder's samples place the offset at {} offsets and the flags at {}", offsets.size(), flags.size());
        return false;
    }
    layout.PropertyOffset = offsets[0];
    layout.PropertyFlags = flags[0];
    std::string const evidence = fmt::format("{} properties built by the client's own property adder with chosen field offsets and flags", samples.size());
    layout.ConfirmDerived("Property.offset", fmt::format("{}: the chosen offset landed at {:#x}", evidence, offsets[0]));
    layout.ConfirmDerived("Property.flags", fmt::format("{}: the chosen flags landed at {:#x}", evidence, flags[0]));
    return true;
}

namespace
{
    constexpr uint64 MinOptionSize = 0x20;
    constexpr uint64 MaxOptionSize = 0x100;
    constexpr uint64 MaxOptions = 4096;

    bool IsNumber(std::string_view text)
    {
        if (!text.empty() && text.front() == '-')
            text.remove_prefix(1);
        return !text.empty() && std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
    }
}

bool ClientDiscovery::DeriveOptionLayout(Machine const& machine, std::span<ListedProperty const> properties, ClientLayout& layout, std::string& error)
{
    std::array const taken = { Span{ layout.PropertyName, 8 }, Span{ layout.PropertyType, 8 }, Span{ layout.PropertyHash, 4 }, Span{ layout.PropertyId, 4 },
        Span{ layout.PropertyContainer, 8 }, Span{ layout.PropertyOffset, 4 }, Span{ layout.PropertyFlags, 4 } };
    struct Placement
    {
        uint64 Slot = 0;
        uint64 Size = 0;
        uint64 Value = 0;
        uint64 Name = 0;
        std::size_t Properties = 0;
        std::size_t Options = 0;
        std::size_t Numbers = 0;
    };
    std::vector<Placement> placements;
    for (uint64 slot = 0; slot + 16 <= PropertyObjectWindow; slot += 8)
    {
        if (Overlaps(slot, 16, taken))
            continue;
        std::vector<std::pair<uint64, uint64>> vectors;
        bool valid = true;
        for (ListedProperty const& property : properties)
        {
            std::optional<uint64> const begin = TryReadU64(machine, property.Address + slot);
            std::optional<uint64> const end = TryReadU64(machine, property.Address + slot + 8);
            if (!begin || !end || *end < *begin || (*begin == 0) != (*end == 0))
            {
                valid = false;
                break;
            }
            if (*end > *begin)
                vectors.emplace_back(*begin, *end);
        }
        if (!valid || vectors.size() < MinimumWinnerVotes)
            continue;
        for (uint64 size = MinOptionSize; size <= MaxOptionSize; size += 8)
        {
            if (!std::all_of(vectors.begin(), vectors.end(), [&](auto const& vector) { return (vector.second - vector.first) % size == 0 && (vector.second - vector.first) / size <= MaxOptions; }))
                continue;
            std::vector<uint64> strings;
            for (uint64 offset = 0; offset + layout.StringObjectSize <= size; offset += 8)
            {
                std::size_t entries = 0;
                std::size_t blanks = 0;
                bool const everyEntry = std::all_of(vectors.begin(), vectors.end(), [&](auto const& vector)
                {
                    for (uint64 entry = vector.first; entry < vector.second; entry += size, ++entries)
                    {
                        std::optional<std::string> const text = ReadLayoutString(machine, entry + offset, layout);
                        if (!text)
                            return false;
                        blanks += text->empty() ? 1 : 0;
                    }
                    return true;
                });
                if (everyEntry && blanks <= entries / MapAliasShare)
                    strings.push_back(offset);
            }
            if (strings.size() != 2 || strings[1] < strings[0] + layout.StringObjectSize)
                continue;
            std::array<std::size_t, 2> numbers{};
            std::array<std::size_t, 2> empty{};
            std::size_t options = 0;
            for (auto const& [begin, end] : vectors)
                for (uint64 entry = begin; entry < end; entry += size, ++options)
                    for (std::size_t which = 0; which < 2; ++which)
                    {
                        std::string const text = ReadLayoutString(machine, entry + strings[which], layout).value_or("");
                        numbers[which] += IsNumber(text) ? 1 : 0;
                        empty[which] += text.empty() ? 1 : 0;
                    }
            std::size_t const value = numbers[0] >= numbers[1] ? 0 : 1;
            if (numbers[value] <= numbers[1 - value] || empty[1 - value] > options / MapAliasShare)
                continue;
            placements.push_back({ slot, size, strings[value], strings[1 - value], vectors.size(), options, numbers[value] });
            break;
        }
    }
    if (placements.size() != 1)
    {
        error = fmt::format("Property.options could not be placed: {} slots of a property hold vectors whose entries are a name and a mostly numeric value", placements.size());
        return false;
    }
    Placement const& placement = placements[0];
    layout.PropertyOptions = placement.Slot;
    layout.OptionSize = placement.Size;
    layout.OptionValue = placement.Value;
    layout.OptionName = placement.Name;
    std::string const evidence = fmt::format("{} properties hold {} options in vectors at {:#x}, {:#x} bytes apart, each a name at {:#x} and a value at {:#x}, {} of the values numbers",
        placement.Properties, placement.Options, placement.Slot, placement.Size, placement.Name, placement.Value, placement.Numbers);
    for (std::string_view const field : { "Property.options", "EnumOption.size", "EnumOption.value", "EnumOption.name" })
        layout.ConfirmDerived(std::string(field), evidence);
    return true;
}
