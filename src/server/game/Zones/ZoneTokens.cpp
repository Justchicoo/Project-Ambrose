/*
 * Project Ambrose by Imjustchico
 * Reads a token's kind, name, storage and counter bounds by property name, refusing an unnamed token, a kind the server does not keep and bounds that hold no start value, and moves token states the way the client's own tokens move, starting each counter at its start value, recording a change only when a state really changes; a client's view of its zone's tokens is moved to the states it should see, removing everything it was shown when its zone changes.
 */

#include "ZoneTokens.h"

#include <fmt/format.h>

#include <algorithm>
#include <utility>

namespace
{
    constexpr int64 ZoneStorage = 1;

    std::optional<int64> WholeOf(PropertyObject const& object, std::string_view name)
    {
        PropertyValue const* const value = object.Get(name);
        if (!value)
            return std::nullopt;
        std::optional<int64> found;
        auto const take = [value, &found]<typename Stored>()
        {
            if (Stored const* const stored = value->GetIf<Stored>())
                found = static_cast<int64>(*stored);
        };
        take.template operator()<int8>();
        take.template operator()<uint8>();
        take.template operator()<int16>();
        take.template operator()<uint16>();
        take.template operator()<int32>();
        take.template operator()<uint32>();
        take.template operator()<int64>();
        take.template operator()<uint64>();
        return found;
    }
}

ZoneTokenDefinition const* ZoneTokenSet::Find(std::string_view name) const
{
    auto const found = std::find_if(Tokens.begin(), Tokens.end(), [name](ZoneTokenDefinition const& token) { return token.Name == name; });
    return found == Tokens.end() ? nullptr : &*found;
}

std::optional<ZoneTokenDefinition> ZoneTokenStates::DefinitionOf(std::shared_ptr<PropertyObject const> token, uint32 objectId, std::string& error)
{
    if (!token)
    {
        error = "a token does not decode";
        return std::nullopt;
    }
    ZoneTokenDefinition definition;
    definition.ObjectId = objectId;
    if (PropertyValue const* const name = token->Get("m_debugName"))
        if (std::string const* const text = name->GetIf<std::string>())
            definition.Name = *text;
    if (definition.Name.empty())
    {
        error = fmt::format("a {} has no m_debugName", token->GetClass().Name);
        return std::nullopt;
    }
    if (token->IsA(BooleanClass))
        definition.Kind = ZoneTokenKind::Boolean;
    else if (token->IsA(CountClass))
    {
        definition.Kind = ZoneTokenKind::Count;
        definition.MinValue = static_cast<int32>(WholeOf(*token, "m_minValue").value_or(0));
        definition.StartValue = static_cast<int32>(WholeOf(*token, "m_startValue").value_or(0));
        definition.MaxValue = static_cast<int32>(WholeOf(*token, "m_maxValue").value_or(0));
        if (definition.MinValue > definition.MaxValue || definition.StartValue < definition.MinValue || definition.StartValue > definition.MaxValue)
        {
            error = fmt::format("token {} counts from {} to {} but starts at {}", definition.Name, definition.MinValue, definition.MaxValue, definition.StartValue);
            return std::nullopt;
        }
    }
    else
    {
        error = fmt::format("token {} is a {}, which the server does not keep", definition.Name, token->GetClass().Name);
        return std::nullopt;
    }
    definition.Storage = WholeOf(*token, "m_storage").value_or(0) == ZoneStorage ? ZoneTokenStorage::Zone : ZoneTokenStorage::Player;
    definition.Object = std::move(token);
    return definition;
}

ZoneTokenState ZoneTokenStates::Get(ZoneTokenDefinition const& token) const
{
    auto const found = _states.find(token.Name);
    return found == _states.end() ? ZoneTokenState{ false, false, token.StartValue } : found->second;
}

bool ZoneTokenStates::Enable(ZoneTokenDefinition const& token)
{
    ZoneTokenState state = Get(token);
    state.Enabled = true;
    return Put(token, state);
}

bool ZoneTokenStates::Disable(ZoneTokenDefinition const& token)
{
    ZoneTokenState state = Get(token);
    state.Enabled = false;
    return Put(token, state);
}

bool ZoneTokenStates::Modify(ZoneTokenDefinition const& token, int32 delta)
{
    ZoneTokenState state = Get(token);
    if (token.Kind == ZoneTokenKind::Boolean)
        state.Set = delta >= 1;
    else
        state.Value = static_cast<int32>(std::clamp<int64>(int64{ state.Value } + delta, token.MinValue, token.MaxValue));
    return Put(token, state);
}

bool ZoneTokenStates::Put(ZoneTokenDefinition const& token, ZoneTokenState state)
{
    auto const [found, added] = _states.try_emplace(token.Name, ZoneTokenState{ false, false, token.StartValue });
    if (found->second == state)
        return false;
    found->second = state;
    ++_changes;
    return true;
}

std::vector<ZoneTokenChange> ZoneTokenView::Update(std::string_view zone, ZoneTokenSet const* tokens, StateReader const& stateOf)
{
    std::vector<ZoneTokenChange> changes;
    if (zone != _zone)
    {
        for (auto const& [name, state] : _shown)
            changes.push_back({ ZoneTokenChange::Kind::Remove, name, nullptr, state, 0 });
        _shown.clear();
        _zone = std::string(zone);
    }
    std::map<std::string, ZoneTokenState, std::less<>> shown;
    if (tokens)
        for (ZoneTokenDefinition const& token : tokens->Tokens)
        {
            ZoneTokenState const state = stateOf(token);
            if (!state.Enabled)
                continue;
            auto const before = _shown.find(token.Name);
            if (before == _shown.end())
                changes.push_back({ ZoneTokenChange::Kind::Add, token.Name, &token, state, 0 });
            else if (token.Kind == ZoneTokenKind::Boolean && before->second.Set != state.Set)
                changes.push_back({ ZoneTokenChange::Kind::Modify, token.Name, &token, state, state.Set ? 1 : 0 });
            else if (token.Kind == ZoneTokenKind::Count && before->second.Value != state.Value)
                changes.push_back({ ZoneTokenChange::Kind::Modify, token.Name, &token, state, state.Value - before->second.Value });
            shown.emplace(token.Name, state);
        }
    for (auto const& [name, state] : _shown)
        if (!shown.contains(name))
            changes.push_back({ ZoneTokenChange::Kind::Remove, name, nullptr, state, 0 });
    _shown = std::move(shown);
    return changes;
}
