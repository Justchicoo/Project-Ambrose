/*
 * Project Ambrose by Imjustchico
 * Names a property hash no source spells out by building names from the words of the names the sources do spell out: m_ and one or two words, or three with an anchor word in any place, each tried against a short list of types, and says how often a random hash would be named by the same search, which is what a match proves.
 */

#ifndef AMBROSE_PROPERTYWORDSEARCH_H
#define AMBROSE_PROPERTYWORDSEARCH_H

#include "Types.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct PropertyWordMatch
{
    uint32 Hash = 0;
    std::string Type;
    std::string Name;

    bool operator==(PropertyWordMatch const&) const = default;
};

struct PropertyWordSearchResult
{
    std::vector<PropertyWordMatch> Matches;
    uint64 Names = 0;
    double Chance = 0.0;
};

namespace PropertyWordSearch
{
    std::vector<std::string_view> DefaultTypes();
    std::vector<std::string> Split(std::string_view name);
    std::vector<std::string> Words(std::vector<std::string> const& names);
    PropertyWordSearchResult Run(std::vector<uint32> const& hashes, std::vector<std::string> const& types, std::vector<std::string> const& words,
        std::vector<std::string> const& anchors = {});
}

#endif
