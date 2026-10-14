/*
 * Project Ambrose by Imjustchico
 * What people have learned about the client program's functions, one note per function keyed by the address it starts at, with the milestone that needed it, kept per client revision as JSON in the Ambrose data folder so the next session reads what an earlier one found rather than working it out again.
 */

#ifndef AMBROSE_FUNCTIONNOTES_H
#define AMBROSE_FUNCTIONNOTES_H

#include "Types.h"

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

struct FunctionNote
{
    uint64 Function = 0;
    std::string Text;
    std::string Milestone;
};

class FunctionNotes
{
public:
    static constexpr uint32 Version = 1;

    static std::filesystem::path PathFor(std::filesystem::path const& dataFolder, std::string_view revision);

    bool Read(std::filesystem::path const& path, std::string& error);
    bool Write(std::filesystem::path const& path, std::string& error) const;

    void Set(FunctionNote note);
    bool Remove(uint64 function);
    FunctionNote const* Find(uint64 function) const;
    std::vector<FunctionNote const*> Matching(std::string_view text) const;
    std::size_t Size() const { return _notes.size(); }

private:
    std::map<uint64, FunctionNote> _notes;
};

#endif
