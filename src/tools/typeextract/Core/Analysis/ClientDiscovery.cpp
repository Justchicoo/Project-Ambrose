/*
 * Project Ambrose by Imjustchico
 * Static discovery over the client's code: the C and C++ initializer tables loaded into rcx and rdx before calls through the _initterm_e and _initterm imports, the race adder as the one function outside RaceManager::InitializeRaces that references the eRace enum name, and the functions calling a given function.
 */

#include "ClientDiscovery.h"
#include "CodeIndex.h"
#include "PeImage.h"

#include <fmt/format.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <set>
#include <string_view>
#include <utility>

namespace
{
    constexpr std::string_view InitializerImportName = "_initterm";
    constexpr std::string_view CInitializerImportName = "_initterm_e";
    constexpr std::string_view RaceEnumName = "enum eRace";
    constexpr std::string_view RaceInitializerName = "RaceManager::InitializeRaces";
    constexpr std::size_t InitializerLookback = 8;
    constexpr uint8 IndirectJumpModrm = 0x25;
    constexpr uint64 PointerSize = 8;

    bool InOneSection(PeImage const& image, uint64 begin, uint64 end)
    {
        uint64 const base = image.GetImageBase();
        if (begin < base || begin - base > std::numeric_limits<uint32>::max())
            return false;
        PeSection const* const section = image.SectionOfRva(static_cast<uint32>(begin - base));
        if (section == nullptr)
            return false;
        uint64 const sectionEnd = base + section->VirtualAddress + std::max(section->VirtualSize, section->RawSize);
        return end <= sectionEnd;
    }

    std::set<uint64> FunctionsReferencing(CodeIndex const& code, std::vector<uint64> const& strings)
    {
        std::set<uint64> functions;
        for (uint64 const text : strings)
            for (uint64 const site : code.LeaReferences(text))
                if (std::optional<uint64> const start = code.FunctionStart(site))
                    functions.insert(*start);
        return functions;
    }

    std::string ListAddresses(std::vector<uint64> const& addresses)
    {
        std::string text;
        for (uint64 const address : addresses)
        {
            if (!text.empty())
                text += ", ";
            text += fmt::format("{:#x}", address);
        }
        return text;
    }
}

namespace
{
    std::optional<InitializerTable> FindTable(PeImage const& image, CodeIndex const& code, std::string_view importName, std::string_view what, std::string& error)
    {
        uint64 const base = image.GetImageBase();
        std::set<std::pair<uint64, uint64>> tables;
        std::size_t imports = 0;
        std::size_t callSites = 0;
        for (PeImport const& import : image.GetImports())
        {
            if (import.Ordinal || import.Name != importName)
                continue;
            ++imports;
            for (uint64 const site : code.IndirectBranchSites(base + import.SlotRva))
            {
                std::span<uint8 const> const opcode = image.ReadRva(static_cast<uint32>(site - base), 2);
                if (opcode.size() != 2)
                    continue;
                std::vector<uint64> calls;
                if (opcode[1] == IndirectJumpModrm)
                {
                    std::span<uint64 const> const thunkCalls = code.CallSites(site);
                    calls.assign(thunkCalls.begin(), thunkCalls.end());
                }
                else
                {
                    calls.push_back(site);
                }
                for (uint64 const call : calls)
                {
                    ++callSites;
                    std::vector<DecodedInstruction> const before = code.DecodeFunctionUntil(call);
                    std::optional<uint64> first;
                    std::optional<uint64> last;
                    for (std::size_t index = before.size() > InitializerLookback ? before.size() - InitializerLookback : 0; index < before.size(); ++index)
                    {
                        DecodedInstruction const& instruction = before[index];
                        if (instruction.Kind != InstructionKind::Lea || !instruction.RipRelativeTarget)
                            continue;
                        if (instruction.FirstRegister == "rcx")
                            first = instruction.RipRelativeTarget;
                        else if (instruction.FirstRegister == "rdx")
                            last = instruction.RipRelativeTarget;
                    }
                    if (!first || !last || *last <= *first || (*last - *first) % PointerSize != 0 || !InOneSection(image, *first, *last))
                        continue;
                    tables.emplace(*first, *last);
                }
            }
        }
        if (tables.size() == 1)
            return InitializerTable{ tables.begin()->first, tables.begin()->second };
        if (imports == 0)
            error = fmt::format("the program does not import {}, so its {} table cannot be found", importName, what);
        else if (tables.empty())
            error = fmt::format("none of the {} calls through the program's {} {} imports loads a {} table into rcx and rdx", callSites, imports, importName, what);
        else
        {
            std::string found;
            for (auto const& [begin, end] : tables)
            {
                if (!found.empty())
                    found += ", ";
                found += fmt::format("{:#x}-{:#x} ({} entries)", begin, end, (end - begin) / PointerSize);
            }
            error = fmt::format("found {} {} tables where one was expected: {}", tables.size(), what, found);
        }
        return std::nullopt;
    }
}

std::optional<InitializerTable> ClientDiscovery::FindInitializerTable(PeImage const& image, CodeIndex const& code, std::string& error)
{
    return FindTable(image, code, InitializerImportName, "C++ initializer", error);
}

std::optional<InitializerTable> ClientDiscovery::FindCInitializerTable(PeImage const& image, CodeIndex const& code, std::string& error)
{
    return FindTable(image, code, CInitializerImportName, "C initializer", error);
}

std::optional<uint64> ClientDiscovery::FindRaceAdder(CodeIndex const& code, std::string& error)
{
    std::vector<uint64> const enumStrings = code.FindStrings(RaceEnumName);
    if (enumStrings.empty())
    {
        error = fmt::format("the program holds no \"{}\" string, so its race adder cannot be found", RaceEnumName);
        return std::nullopt;
    }
    std::set<uint64> const initializers = FunctionsReferencing(code, code.FindStrings(RaceInitializerName));
    std::set<uint64> const referencing = FunctionsReferencing(code, enumStrings);
    std::vector<uint64> candidates;
    std::set_difference(referencing.begin(), referencing.end(), initializers.begin(), initializers.end(), std::back_inserter(candidates));
    if (candidates.size() == 1)
        return candidates.front();
    if (referencing.empty())
        error = fmt::format("found no race adder: no function references \"{}\"", RaceEnumName);
    else if (candidates.empty())
        error = fmt::format("found no race adder: every function referencing \"{}\" ({}) also references \"{}\"", RaceEnumName, ListAddresses(std::vector<uint64>(referencing.begin(), referencing.end())), RaceInitializerName);
    else
        error = fmt::format("found {} race adders where one was expected: {}", candidates.size(), ListAddresses(candidates));
    return std::nullopt;
}

std::vector<uint64> ClientDiscovery::FunctionsCalling(CodeIndex const& code, uint64 target)
{
    std::vector<uint64> functions;
    for (uint64 const site : code.CallSites(target))
        if (std::optional<uint64> const start = code.FunctionStart(site))
            functions.push_back(*start);
    std::sort(functions.begin(), functions.end());
    functions.erase(std::unique(functions.begin(), functions.end()), functions.end());
    return functions;
}
