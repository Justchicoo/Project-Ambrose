/*
 * Project Ambrose by Imjustchico
 * A zone's tokens as its ZoneTokenInfo placements define them, each a flag or a counter kept for each wizard or for the whole zone instance, and the states a set of tokens is in: enabling, disabling and moving a token by a delta as the client moves its own copy, a flag set by a delta of one or more and cleared by less, a counter clamped to its bounds, each change counted so a reader can tell when to look again; and what one client has been told of its zone's tokens, so each enabled token is added once, moved by the delta its state changed by and removed when it is disabled or its wizard leaves the zone.
 */

#ifndef AMBROSE_ZONETOKENS_H
#define AMBROSE_ZONETOKENS_H

#include "PropertyObject.h"
#include "Types.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class ZoneTokenKind : uint8
{
    Boolean,
    Count
};

enum class ZoneTokenStorage : uint8
{
    Player,
    Zone
};

struct ZoneTokenDefinition
{
    std::string Name;
    uint32 ObjectId = 0;
    ZoneTokenKind Kind = ZoneTokenKind::Boolean;
    ZoneTokenStorage Storage = ZoneTokenStorage::Player;
    int32 MinValue = 0;
    int32 StartValue = 0;
    int32 MaxValue = 0;
    std::shared_ptr<PropertyObject const> Object;
};

struct ZoneTokenSet
{
    std::vector<ZoneTokenDefinition> Tokens;

    ZoneTokenDefinition const* Find(std::string_view name) const;
};

struct ZoneTokenState
{
    bool Enabled = false;
    bool Set = false;
    int32 Value = 0;

    bool operator==(ZoneTokenState const&) const = default;
};

class ZoneTokenStates
{
public:
    static constexpr std::string_view BooleanClass = "class ZoneTokenBoolean";
    static constexpr std::string_view CountClass = "class ZoneTokenCount";

    static std::optional<ZoneTokenDefinition> DefinitionOf(std::shared_ptr<PropertyObject const> token, uint32 objectId, std::string& error);

    ZoneTokenState Get(ZoneTokenDefinition const& token) const;
    bool Enable(ZoneTokenDefinition const& token);
    bool Disable(ZoneTokenDefinition const& token);
    bool Modify(ZoneTokenDefinition const& token, int32 delta);
    uint64 GetChangeCount() const noexcept { return _changes; }

private:
    bool Put(ZoneTokenDefinition const& token, ZoneTokenState state);

    std::map<std::string, ZoneTokenState, std::less<>> _states;
    uint64 _changes = 0;
};

struct ZoneTokenChange
{
    enum class Kind : uint8
    {
        Add,
        Remove,
        Modify
    };

    Kind Change = Kind::Add;
    std::string Name;
    ZoneTokenDefinition const* Token = nullptr;
    ZoneTokenState State;
    int32 Delta = 0;
};

class ZoneTokenView
{
public:
    using StateReader = std::function<ZoneTokenState(ZoneTokenDefinition const&)>;

    std::vector<ZoneTokenChange> Update(std::string_view zone, ZoneTokenSet const* tokens, StateReader const& stateOf);

private:
    std::string _zone;
    std::map<std::string, ZoneTokenState, std::less<>> _shown;
};

#endif
