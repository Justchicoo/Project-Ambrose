/*
 * Project Ambrose by Imjustchico
 * Builds an NPC's menu from its providers in priority order and routes a flat index back to the provider that owns it.
 */

#include "NpcServiceMenu.h"

#include <algorithm>

NpcServiceMenu NpcServiceMenu::Build(std::span<NpcServiceProvider* const> providers, NpcServiceTarget const& target)
{
    std::vector<NpcServiceProvider*> ordered;
    for (NpcServiceProvider* provider : providers)
        if (provider)
            ordered.push_back(provider);
    std::stable_sort(ordered.begin(), ordered.end(), [](NpcServiceProvider const* left, NpcServiceProvider const* right) { return left->GetPriority() > right->GetPriority(); });

    NpcServiceMenu menu;
    for (NpcServiceProvider* provider : ordered)
    {
        std::vector<NpcServiceOption> options = provider->GetServiceOptions(target);
        for (uint32 index = 0; index < options.size(); ++index)
            menu._entries.push_back({ std::move(options[index]), provider, index });
        if (!menu._radius)
            menu._radius = provider->GetInteractionRadius();
        if (!menu._nameKey)
            menu._nameKey = provider->GetNameKey();
        if (!menu._textKey)
            menu._textKey = provider->GetTextKey();
        if (!menu._icon)
            menu._icon = provider->GetIcon();
    }
    return menu;
}

NpcServiceMenu::Entry const* NpcServiceMenu::Find(uint32 flatIndex) const noexcept
{
    return flatIndex < _entries.size() ? &_entries[flatIndex] : nullptr;
}

bool NpcServiceMenu::Route(NpcServiceTarget const& target, uint32 flatIndex) const
{
    Entry const* const entry = Find(flatIndex);
    if (!entry)
        return false;
    entry->Provider->OnServiceInteraction(target, entry->Index);
    return true;
}
