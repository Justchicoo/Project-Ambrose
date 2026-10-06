/*
 * Project Ambrose by Imjustchico
 * Keeps an object's effects in internal id order, so the lowest free id is the first gap in that order, writes the id an effect is given into its m_internalID before the effect is kept, refuses an object that is not a GameEffectBase, and builds the container an effect behavior carries from a copy of each effect, in internal id order, under m_publicEffects, which is the list the client receives, an object entering a client's view carrying it empty.
 */

#include "GameEffectHolder.h"
#include "PropertyFiller.h"

#include <fmt/format.h>

#include <algorithm>
#include <limits>
#include <utility>

std::optional<int32> GameEffectHolder::Add(PropertyObjectPtr effect, std::string& problem)
{
    problem.clear();
    if (!effect)
    {
        problem = "no effect was given";
        return std::nullopt;
    }
    if (!effect->IsA(EffectClass))
    {
        problem = fmt::format("{} is not a {}", effect->GetClass().Name, EffectClass);
        return std::nullopt;
    }
    std::optional<uint32> const nameId = GetEffectNameId(*effect);
    if (!nameId)
    {
        problem = fmt::format("{} holds no m_effectNameID", effect->GetClass().Name);
        return std::nullopt;
    }
    int32 const id = NextFreeId();
    if (id == 0)
    {
        problem = "every internal id is in use on this object";
        return std::nullopt;
    }
    PropertyFiller(*effect, problem).Set("m_internalID", id);
    if (!problem.empty())
        return std::nullopt;
    auto const at = std::lower_bound(_effects.begin(), _effects.end(), id, [](ActiveGameEffect const& active, int32 wanted) { return active.InternalId < wanted; });
    _effects.insert(at, ActiveGameEffect{ id, *nameId, std::move(effect) });
    return id;
}

std::optional<ActiveGameEffect> GameEffectHolder::Remove(int32 internalId)
{
    auto const at = std::find_if(_effects.begin(), _effects.end(), [internalId](ActiveGameEffect const& active) { return active.InternalId == internalId; });
    if (at == _effects.end())
        return std::nullopt;
    ActiveGameEffect removed = std::move(*at);
    _effects.erase(at);
    return removed;
}

ActiveGameEffect const* GameEffectHolder::Find(int32 internalId) const noexcept
{
    auto const at = std::find_if(_effects.begin(), _effects.end(), [internalId](ActiveGameEffect const& active) { return active.InternalId == internalId; });
    return at == _effects.end() ? nullptr : &*at;
}

PropertyObjectPtr GameEffectHolder::BuildContainer(TypeCatalogPtr const& catalog, std::string& problem) const
{
    PropertyObjectPtr container = PropertyObject::Create(catalog, ContainerClass);
    if (!container)
    {
        problem = fmt::format("the type dump has no property class {}", ContainerClass);
        return nullptr;
    }
    PropertyValue::List effects;
    effects.reserve(_effects.size());
    for (ActiveGameEffect const& active : _effects)
        effects.emplace_back(active.Effect->Clone());
    PropertyFiller(*container, problem).Set("m_publicEffects", std::move(effects));
    if (!problem.empty())
        return nullptr;
    return container;
}

bool GameEffectHolder::FillBehavior(PropertyObject& behavior, std::string& problem)
{
    PropertyObjectPtr container = GameEffectHolder().BuildContainer(behavior.GetCatalog(), problem);
    if (!container)
        return false;
    PropertyFiller(behavior, problem).Set("m_gameEffects", std::move(container));
    return problem.empty();
}

std::optional<uint32> GameEffectHolder::GetEffectNameId(PropertyObject const& effect) noexcept
{
    PropertyValue const* const value = effect.Get("m_effectNameID");
    uint32 const* const id = value != nullptr ? value->GetIf<uint32>() : nullptr;
    return id != nullptr ? std::optional<uint32>(*id) : std::nullopt;
}

int32 GameEffectHolder::NextFreeId() const noexcept
{
    int32 wanted = FirstInternalId;
    for (ActiveGameEffect const& active : _effects)
    {
        if (active.InternalId != wanted)
            break;
        if (wanted == std::numeric_limits<int32>::max())
            return 0;
        ++wanted;
    }
    return wanted;
}
