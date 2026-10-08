/*
 * Project Ambrose by Imjustchico
 * Splits names into lower-case words at every break between letters, digits and case, drops the words every door and target shares, scores each candidate by the destination zone the door names twice, the door's own zone the target names twice and any other word they share once, and keeps the best candidate per door at or above the minimum score, ties going to the first in data order.
 */

#include "TeleportProposer.h"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <fstream>

namespace
{
    std::set<std::string> const Common = { "teleport", "teleporter", "to", "trigger", "location", "target", "exit", "entrance", "volume", "activator", "the", "wc", "vol",
        "teleportvol", "interiors", "wizard", "city", "from" };

    int Shared(std::set<std::string> const& a, std::set<std::string> const& b)
    {
        int count = 0;
        for (std::string const& word : a)
            count += b.contains(word) ? 1 : 0;
        return count;
    }

    bool HoldsDoor(ExtractedTrigger const& trigger)
    {
        return std::any_of(trigger.Results.begin(), trigger.Results.end(),
            [](ExtractedTriggerResult const& result) { return result.ClassName && *result.ClassName == TeleportProposer::TeleportClass; });
    }

    std::string Quoted(std::string_view text)
    {
        std::string out = "\"";
        for (char c : text)
        {
            if (c == '"')
                out += '"';
            out += c;
        }
        return out + "\"";
    }
}

std::set<std::string> TeleportProposer::Words(std::string_view text)
{
    std::set<std::string> words;
    std::string word;
    auto const flush = [&]
    {
        if (!word.empty() && !Common.contains(word))
            words.insert(word);
        word.clear();
    };
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        unsigned char const c = static_cast<unsigned char>(text[index]);
        if (!std::isalnum(c))
        {
            flush();
            continue;
        }
        if (!word.empty())
        {
            unsigned char const previous = static_cast<unsigned char>(text[index - 1]);
            bool const caseBreak = std::isupper(c) && std::islower(previous);
            bool const kindBreak = std::isdigit(c) != std::isdigit(previous);
            if (caseBreak || kindBreak)
                flush();
        }
        word += static_cast<char>(std::tolower(c));
    }
    flush();
    return words;
}

std::vector<TeleportProposal> TeleportProposer::Propose(std::vector<ExtractedZone> const& zones)
{
    std::vector<std::set<std::string>> zoneWords;
    zoneWords.reserve(zones.size());
    for (ExtractedZone const& zone : zones)
        zoneWords.push_back(Words(zone.Path));
    std::vector<TeleportProposal> proposals;
    for (std::size_t from = 0; from < zones.size(); ++from)
    {
        for (ExtractedTrigger const& trigger : zones[from].Triggers)
        {
            if (!HoldsDoor(trigger))
                continue;
            std::set<std::string> const doorWords = Words(trigger.Name);
            TeleportProposal best;
            for (std::size_t to = 0; to < zones.size(); ++to)
            {
                if (to == from)
                    continue;
                int const named = 2 * Shared(doorWords, zoneWords[to]);
                for (ExtractedLocation const& location : zones[to].Locations)
                {
                    if (!location.Name.starts_with("Target"))
                        continue;
                    std::set<std::string> const targetWords = Words(location.Name);
                    int const score = named + 2 * Shared(targetWords, zoneWords[from]) + Shared(targetWords, doorWords);
                    if (score > best.Score)
                        best = TeleportProposal{ zones[from].Path, trigger.Name, zones[to].Path, location.Name, score };
                }
            }
            if (best.Score >= MinimumScore)
                proposals.push_back(std::move(best));
        }
    }
    return proposals;
}

std::string TeleportProposer::Csv(std::vector<TeleportProposal> const& proposals)
{
    std::string out = "zone,trigger_name,dest_zone,dest_location,score\n";
    for (TeleportProposal const& proposal : proposals)
        out += fmt::format("{},{},{},{},{}\n", Quoted(proposal.Zone), Quoted(proposal.TriggerName), Quoted(proposal.DestZone), Quoted(proposal.DestLocation), proposal.Score);
    return out;
}

bool TeleportProposer::WriteCsv(std::filesystem::path const& file, std::vector<TeleportProposal> const& proposals, std::string& error)
{
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        error = "the file cannot be opened for writing";
        return false;
    }
    std::string const text = Csv(proposals);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out)
    {
        error = "the file could not be written";
        return false;
    }
    return true;
}
