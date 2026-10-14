/*
 * Project Ambrose by Imjustchico
 * Builds an added token from the object its zone data holds, enabled and set or counted as the server keeps it, written in the binary format under the transmit mask as the client's MSG_AddZoneToken handler reads it, and names a moved or removed token by its name.
 */

#include "ZoneTokenMessages.h"
#include "ObjectSerializer.h"
#include "PropertyFiller.h"

#include <fmt/format.h>

std::optional<GameMessages::AddZoneToken> ZoneTokenMessages::Add(uint64 player, ZoneTokenDefinition const& token, ZoneTokenState const& state, std::string& problem)
{
    PropertyObjectPtr const object = token.Object ? token.Object->Clone() : nullptr;
    if (!object)
    {
        problem = fmt::format("token {} has no object to send", token.Name);
        return std::nullopt;
    }
    PropertyFiller filler(*object, problem);
    filler.Set("m_enabled", true);
    if (token.Kind == ZoneTokenKind::Boolean)
        filler.Set("m_bSet", state.Set);
    else
        filler.Set("m_curValue", state.Value);
    if (!problem.empty())
        return std::nullopt;
    EncodeResult const encoded = ObjectSerializer::Encode(object.get(), SerializerOptions());
    if (!encoded.Ok())
    {
        problem = fmt::format("token {} does not encode: {}", token.Name, encoded.Detail);
        return std::nullopt;
    }
    GameMessages::AddZoneToken message;
    message.GlobalId = player;
    message.Add.assign(encoded.Bytes.begin(), encoded.Bytes.end());
    return message;
}

GameMessages::ModifyZoneToken ZoneTokenMessages::Modify(uint64 player, ZoneTokenChange const& change)
{
    return { player, change.Name, change.Delta };
}

GameMessages::RemoveZoneToken ZoneTokenMessages::Remove(uint64 player, ZoneTokenChange const& change)
{
    return { player, change.Name };
}
