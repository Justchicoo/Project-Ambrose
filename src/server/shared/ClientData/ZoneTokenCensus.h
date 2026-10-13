/*
 * Project Ambrose by Imjustchico
 * Takes the zone token census of extracted zones: each ZoneTokenInfo placement's token container is decoded through the type dump, its tokens counted by class with the name each gives itself, and every ResultList a token holds, those of its time triggers included, counted when it holds a result, with the result classes it holds; the triggers' own result, cooldown result and requirement classes are counted with them, and every string a decoded requirement or result holds that names a token is counted by the class and property holding it, so a server knows which token classes, hooks, results and requirements it must handle first, each row keeping the first zone that uses it. A part whose class the type dump cannot describe is counted by its class hash, never guessed at, and the server-only zone token classes, which zone data names only by hash, are named by the hash of their names.
 */

#ifndef AMBROSE_ZONETOKENCENSUS_H
#define AMBROSE_ZONETOKENCENSUS_H

#include "TypeRegistry.h"
#include "Types.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

struct ZoneExtraction;

struct ClassUsage
{
    std::string Type;
    std::size_t Count = 0;
    std::size_t Zones = 0;
    std::string Example;
};

struct TokenHookUsage
{
    std::string Hook;
    std::size_t Lists = 0;
    std::size_t Zones = 0;
    std::vector<ClassUsage> Results;
};

struct TriggerResultUsage
{
    std::string Type;
    std::size_t Results = 0;
    std::size_t CooldownResults = 0;
    std::size_t Zones = 0;
    std::string Example;
};

struct ZoneToken
{
    std::string Type;
    std::string Name;
    uint32 ObjectId = 0;
    std::vector<std::string> Hooks;
};

struct ZoneTokens
{
    std::string Zone;
    std::size_t Placements = 0;
    std::vector<ZoneToken> Tokens;
};

class ZoneTokenCensus
{
public:
    static constexpr std::string_view UnknownClass = "(undecoded)";

    static ZoneTokenCensus Take(ZoneExtraction const& zones, TypeCatalogPtr const& catalog);

    std::vector<ZoneTokens> const& GetZones() const noexcept { return _zones; }
    std::vector<ClassUsage> const& GetTokenClasses() const noexcept { return _tokenClasses; }
    std::vector<TokenHookUsage> const& GetHooks() const noexcept { return _hooks; }
    std::vector<TriggerResultUsage> const& GetTriggerResults() const noexcept { return _triggerResults; }
    std::vector<ClassUsage> const& GetTriggerRequirements() const noexcept { return _triggerRequirements; }
    std::vector<ClassUsage> const& GetReferences() const noexcept { return _references; }
    std::size_t GetPlacementCount() const noexcept { return _placements; }
    std::size_t GetTokenCount() const noexcept { return _tokens; }
    std::size_t GetTriggerCount() const noexcept { return _triggers; }
    std::size_t GetUndecodedCount() const noexcept { return _undecoded; }

private:
    std::vector<ZoneTokens> _zones;
    std::vector<ClassUsage> _tokenClasses;
    std::vector<TokenHookUsage> _hooks;
    std::vector<TriggerResultUsage> _triggerResults;
    std::vector<ClassUsage> _triggerRequirements;
    std::vector<ClassUsage> _references;
    std::size_t _placements = 0;
    std::size_t _tokens = 0;
    std::size_t _triggers = 0;
    std::size_t _undecoded = 0;
};

#endif
