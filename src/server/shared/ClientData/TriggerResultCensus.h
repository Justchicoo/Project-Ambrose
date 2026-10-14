/*
 * Project Ambrose by Imjustchico
 * Counts the result classes the zone triggers of extracted zones run, each trigger's results and cooldown results decoded through the type dump and counted once per result and once per zone that holds it, and for the classes asked about, how often each property holds each value, so what the server must honour in a result is read from the data rather than guessed; results that do not decode are counted under their class hash.
 */

#ifndef AMBROSE_TRIGGERRESULTCENSUS_H
#define AMBROSE_TRIGGERRESULTCENSUS_H

#include "TypeRegistry.h"

#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <vector>

struct ZoneExtraction;

struct ResultClassUsage
{
    std::string ClassName;
    std::size_t Results = 0;
    std::size_t Zones = 0;
    std::size_t Undecoded = 0;
};

struct ResultValueCount
{
    std::string ClassName;
    std::string Property;
    std::string Value;
    std::size_t Results = 0;
};

class TriggerResultCensus
{
public:
    static TriggerResultCensus Take(ZoneExtraction const& zones, TypeCatalogPtr const& catalog, std::set<std::string, std::less<>> const& detailed);

    std::vector<ResultClassUsage> const& GetUsage() const noexcept { return _usage; }
    std::vector<ResultValueCount> const& GetValues() const noexcept { return _values; }
    std::size_t GetResultCount() const noexcept { return _results; }
    std::size_t GetTriggerCount() const noexcept { return _triggers; }

private:
    std::vector<ResultClassUsage> _usage;
    std::vector<ResultValueCount> _values;
    std::size_t _results = 0;
    std::size_t _triggers = 0;
};

#endif
