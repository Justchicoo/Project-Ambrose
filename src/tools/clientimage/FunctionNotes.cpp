/*
 * Project Ambrose by Imjustchico
 * Reads and writes the notes file, notes/<revision>.json, as a version and a list of function, text and milestone, refusing a file of another version or shape rather than reading part of it, and finds notes by function or by text their words or milestone hold, ignoring case.
 */

#include "FunctionNotes.h"
#include "StringUtil.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <system_error>

std::filesystem::path FunctionNotes::PathFor(std::filesystem::path const& dataFolder, std::string_view revision)
{
    return dataFolder / "notes" / (std::string(revision) + ".json");
}

bool FunctionNotes::Read(std::filesystem::path const& path, std::string& error)
{
    _notes.clear();
    std::error_code code;
    if (!std::filesystem::exists(path, code))
        return true;
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        error = "it cannot be opened";
        return false;
    }
    nlohmann::json document;
    try
    {
        stream >> document;
    }
    catch (std::exception const& exception)
    {
        error = exception.what();
        return false;
    }
    if (!document.is_object() || !document.contains("version") || document["version"] != Version || !document.contains("notes") || !document["notes"].is_array())
    {
        error = "it is not a notes file of this version";
        return false;
    }
    for (nlohmann::json const& entry : document["notes"])
    {
        if (!entry.is_object() || !entry.contains("function") || !entry["function"].is_number_unsigned() || !entry.contains("text") || !entry["text"].is_string()
            || (entry.contains("milestone") && !entry["milestone"].is_string()))
        {
            _notes.clear();
            error = "a note in it has no function or text";
            return false;
        }
        Set({ entry["function"].get<uint64>(), entry["text"].get<std::string>(), entry.value("milestone", std::string()) });
    }
    return true;
}

bool FunctionNotes::Write(std::filesystem::path const& path, std::string& error) const
{
    std::error_code code;
    std::filesystem::create_directories(path.parent_path(), code);
    if (code)
    {
        error = code.message();
        return false;
    }
    nlohmann::json list = nlohmann::json::array();
    for (auto const& [function, note] : _notes)
    {
        nlohmann::json entry = { { "function", function }, { "text", note.Text } };
        if (!note.Milestone.empty())
            entry["milestone"] = note.Milestone;
        list.push_back(std::move(entry));
    }
    nlohmann::json const document = { { "version", Version }, { "notes", std::move(list) } };
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        error = "it cannot be written";
        return false;
    }
    stream << document.dump(1, '\t');
    if (!stream.good())
    {
        error = "writing it failed";
        return false;
    }
    return true;
}

void FunctionNotes::Set(FunctionNote note)
{
    uint64 const function = note.Function;
    _notes.insert_or_assign(function, std::move(note));
}

bool FunctionNotes::Remove(uint64 function)
{
    return _notes.erase(function) != 0;
}

FunctionNote const* FunctionNotes::Find(uint64 function) const
{
    auto const found = _notes.find(function);
    return found == _notes.end() ? nullptr : &found->second;
}

std::vector<FunctionNote const*> FunctionNotes::Matching(std::string_view text) const
{
    std::string const wanted = Ambrose::ToLower(text);
    std::vector<FunctionNote const*> matches;
    for (auto const& [function, note] : _notes)
        if (wanted.empty() || Ambrose::ToLower(note.Text).find(wanted) != std::string::npos || Ambrose::ToLower(note.Milestone).find(wanted) != std::string::npos)
            matches.push_back(&note);
    return matches;
}
