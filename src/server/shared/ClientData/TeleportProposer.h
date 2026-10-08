/*
 * Project Ambrose by Imjustchico
 * Suggests where each door, a zone trigger whose results hold a ResTeleport, leads, for a person to review before any row is authored: the client data names no destination, so each door is matched to the 'Target' location of another zone whose zone path the door's name mentions and whose own name mentions the door's zone, scored by the words they share, and written as a review CSV.
 */

#ifndef AMBROSE_TELEPORTPROPOSER_H
#define AMBROSE_TELEPORTPROPOSER_H

#include "ZoneExtractor.h"

#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

struct TeleportProposal
{
    std::string Zone;
    std::string TriggerName;
    std::string DestZone;
    std::string DestLocation;
    int Score = 0;
};

namespace TeleportProposer
{
    constexpr std::string_view TeleportClass = "class ResTeleport";
    constexpr int MinimumScore = 2;

    std::set<std::string> Words(std::string_view text);
    std::vector<TeleportProposal> Propose(std::vector<ExtractedZone> const& zones);
    std::string Csv(std::vector<TeleportProposal> const& proposals);
    bool WriteCsv(std::filesystem::path const& file, std::vector<TeleportProposal> const& proposals, std::string& error);
}

#endif
