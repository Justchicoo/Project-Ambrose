/*
 * Project Ambrose by Imjustchico
 * The game effects one object in the world carries: each is a GameEffectBase object, given an internal id unique on that object when it is added, the lowest id no effect on it holds, so an id is free again once its effect is taken away, because the client finds the effect to take away by that id alone; and the GameEffectContainer the object's effect behavior is sent with, empty and never null, so the client starts from the same empty list the server holds and every effect the object carries follows it as its own MSG_ADDEFFECT, whose EffectData is the effect in CoreObject form.
 */

#ifndef AMBROSE_GAMEEFFECTHOLDER_H
#define AMBROSE_GAMEEFFECTHOLDER_H

#include "CoreObjectSerializer.h"
#include "PropertyObject.h"
#include "Types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct ActiveGameEffect
{
    int32 InternalId = 0;
    uint32 EffectNameId = 0;
    PropertyObjectPtr Effect;
};

class GameEffectHolder
{
public:
    static constexpr std::string_view EffectClass = "class GameEffectBase";
    static constexpr std::string_view ContainerClass = "class GameEffectContainer";
    static constexpr std::string_view BehaviorClass = "class BaseGameEffectBehavior";
    static constexpr std::string_view AddEffectMessage = "MSG_ADDEFFECT";
    static constexpr std::string_view AddEffectField = "EffectData";
    static constexpr int32 FirstInternalId = 1;

    std::optional<int32> Add(PropertyObjectPtr effect, std::string& problem);
    std::optional<ActiveGameEffect> Remove(int32 internalId);
    ActiveGameEffect const* Find(int32 internalId) const noexcept;
    std::vector<ActiveGameEffect> const& GetEffects() const noexcept { return _effects; }
    std::size_t Count() const noexcept { return _effects.size(); }
    void Clear() noexcept { _effects.clear(); }

    PropertyObjectPtr BuildContainer(TypeCatalogPtr const& catalog, std::string& problem) const;
    static bool FillBehavior(PropertyObject& behavior, std::string& problem);
    static std::optional<uint32> GetEffectNameId(PropertyObject const& effect) noexcept;
    static EncodeResult Encode(PropertyObject const& effect, CoreObjectTypeTable const& types);

private:
    int32 NextFreeId() const noexcept;

    std::vector<ActiveGameEffect> _effects;
};

#endif
