/*
 * Project Ambrose by Imjustchico
 * One NPC's menu for one wizard: the options of each provider in turn, providers of higher priority first and equal ones in the order given, numbered from 0 across the whole menu as the client numbers them, so the index a client sends back finds the provider that owns it and that provider's own index; an index past the menu finds nothing. The first provider that sets a radius, a name, text or icon key gives the NPC that one.
 */

#ifndef AMBROSE_NPCSERVICEMENU_H
#define AMBROSE_NPCSERVICEMENU_H

#include "NpcServiceProvider.h"

#include <optional>
#include <span>
#include <string>
#include <vector>

class NpcServiceMenu
{
public:
    struct Entry
    {
        NpcServiceOption Option;
        NpcServiceProvider* Provider = nullptr;
        uint32 Index = 0;
    };

    static NpcServiceMenu Build(std::span<NpcServiceProvider* const> providers, NpcServiceTarget const& target);

    std::vector<Entry> const& GetEntries() const noexcept { return _entries; }
    Entry const* Find(uint32 flatIndex) const noexcept;
    bool Route(NpcServiceTarget const& target, uint32 flatIndex) const;

    std::optional<float> GetInteractionRadius() const noexcept { return _radius; }
    std::optional<std::string> const& GetNameKey() const noexcept { return _nameKey; }
    std::optional<std::string> const& GetTextKey() const noexcept { return _textKey; }
    std::optional<std::string> const& GetIcon() const noexcept { return _icon; }

private:
    std::vector<Entry> _entries;
    std::optional<float> _radius;
    std::optional<std::string> _nameKey;
    std::optional<std::string> _textKey;
    std::optional<std::string> _icon;
};

#endif
