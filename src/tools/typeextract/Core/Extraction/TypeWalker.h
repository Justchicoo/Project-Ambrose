/*
 * Project Ambrose by Imjustchico
 * Reads every type the client registered into a format v2 dump model: names, hashes, bases, properties with their type, id, offset, flags, container, dynamic, singleton and pointer flags and enum options, where container names and dynamic flags come from calling the container's own methods; validates each value against the client's hashes and ordering rules, refuses any name or option text that is not valid UTF-8, and counts every problem by kind, keeping a few samples of each.
 */

#ifndef AMBROSE_TYPEWALKER_H
#define AMBROSE_TYPEWALKER_H

#include "ClientLayout.h"
#include "TypeDumpLoader.h"

#include <map>
#include <span>
#include <string>
#include <vector>

class GuestProcess;

struct TypeWalkResult
{
    static constexpr std::size_t SamplesPerProblem = 5;

    TypeDumpLoader::RawDump Dump;
    std::map<std::string, uint64> ProblemCounts;
    std::map<std::string, std::vector<std::string>> ProblemSamples;
    uint64 PropertyCount = 0;
    uint64 DuplicateTypes = 0;
    uint64 DuplicateOptions = 0;

    bool IsValid() const noexcept { return ProblemCounts.empty(); }
};

class TypeWalker
{
public:
    static constexpr uint64 ContainerCallBudget = 100000;
    static constexpr std::size_t MaxStringLength = 1u << 20;
    static constexpr std::size_t MaxBaseDepth = 64;
    static constexpr std::size_t MaxProperties = 1u << 16;
    static constexpr std::size_t MaxOptions = 1u << 16;

    TypeWalker(GuestProcess& process, ClientLayout layout);

    TypeWalkResult Walk(std::span<uint64 const> types);

    static std::string ListNameOf(std::string_view typeName);

private:
    GuestProcess& _process;
    ClientLayout _layout;
};

#endif
