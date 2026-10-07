/*
 * Project Ambrose by Imjustchico
 * Finds the client's C and C++ initializer tables, derives the type map's std::map node layout from the shape of the trees on its emulated heap, scans that heap for the type map, votes on the Type constructor and PropertyList initializer, derives Type and std::string layout from chosen constructor values, and locates RaceManager's race adder; a vote without a clear winner is an error.
 */

#ifndef AMBROSE_CLIENTDISCOVERY_H
#define AMBROSE_CLIENTDISCOVERY_H

#include "ClientLayout.h"
#include "Types.h"

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

class CodeIndex;
class GuestHeap;
class Machine;
class PeImage;

struct InitializerTable
{
    uint64 Begin = 0;
    uint64 End = 0;

    uint64 Count() const noexcept { return (End - Begin) / 8; }
};

struct DiscoveryVote
{
    uint64 Winner = 0;
    uint64 WinnerVotes = 0;
    uint64 RunnerUp = 0;
    uint64 RunnerUpVotes = 0;
};

struct ConstructedTypeSample
{
    uint64 Address = 0;
    std::string Name;
    uint32 Hash = 0;
};

struct ConstructedListSample
{
    uint64 Address = 0;
    uint64 Base = 0;
    bool Singleton = false;
    std::string Name;
};

struct ListedProperty
{
    uint64 Address = 0;
    uint64 Index = 0;
};

using GuestCall = std::function<std::optional<uint64>(uint64 function, uint64 object)>;

namespace ClientDiscovery
{
    inline constexpr uint64 MinimumWinnerVotes = 20;
    inline constexpr uint64 MinimumWinnerRatio = 4;
    inline constexpr std::size_t MaxTypeMapNodes = 1u << 20;

    std::optional<InitializerTable> FindInitializerTable(PeImage const& image, CodeIndex const& code, std::string& error);
    std::optional<InitializerTable> FindCInitializerTable(PeImage const& image, CodeIndex const& code, std::string& error);
    std::optional<uint64> DeriveTypeMapLayout(Machine const& machine, GuestHeap const& heap, ClientLayout& layout, std::string& error);
    std::optional<uint64> FindTypeMapHead(Machine const& machine, GuestHeap const& heap, ClientLayout const& layout, std::string& error);
    std::optional<std::vector<uint64>> WalkTypeMap(Machine const& machine, uint64 head, ClientLayout const& layout, std::string& error);
    bool DeriveConstructedTypeLayout(Machine const& machine, GuestHeap const& heap, std::span<ConstructedTypeSample const> samples, ClientLayout& layout, std::string& error);
    bool DerivePropertyListLink(Machine const& machine, GuestHeap const& heap, std::span<uint64 const> types, ClientLayout& layout, std::string& error);
    bool DeriveTypePointerFlag(Machine const& machine, GuestHeap const& heap, std::span<uint64 const> types, ClientLayout& layout, std::string& error);
    std::optional<DiscoveryVote> FindTypeConstructor(Machine const& machine, CodeIndex const& code, std::span<uint64 const> types, std::string& error);
    std::optional<DiscoveryVote> FindPropertyListInitializer(Machine const& machine, CodeIndex const& code, std::span<uint64 const> types, ClientLayout const& layout, std::string& error);
    bool DerivePropertyLayout(Machine const& machine, GuestHeap const& heap, std::span<uint64 const> types, ClientLayout& layout, std::string& error);
    std::vector<ListedProperty> ListedProperties(Machine const& machine, GuestHeap const& heap, std::span<uint64 const> types, ClientLayout const& layout);
    bool DerivePropertyId(Machine const& machine, std::span<ListedProperty const> properties, ClientLayout& layout, std::string& error);
    bool DeriveContainerLayout(Machine const& machine, std::span<ListedProperty const> properties, ClientLayout& layout, GuestCall const& call, std::string& error);
    std::vector<uint64> FindPropertyListConstructorCandidates(CodeIndex const& code, std::span<uint64 const> lists, uint64 finalizer, std::span<uint64 const> known);
    bool DeriveConstructedListLayout(Machine const& machine, std::span<ConstructedListSample const> samples, ClientLayout& layout, std::string& error);
    std::optional<uint64> FindRaceAdder(CodeIndex const& code, std::string& error);
    std::vector<uint64> FunctionsCalling(CodeIndex const& code, uint64 target);
}

#endif
