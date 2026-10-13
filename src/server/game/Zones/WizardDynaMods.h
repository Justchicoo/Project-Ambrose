/*
 * Project Ambrose by Imjustchico
 * The Dynamods one wizard's progress has made to the world it sees, one for each zone and client tag: a state its tagged objects stand in, or their removal, with the index its client knows the mod by and the GID of what made it, so the server can show that wizard alone the gate its quest opened, and arm in its client the spawners a zone's mods name.
 */

#ifndef AMBROSE_WIZARDDYNAMODS_H
#define AMBROSE_WIZARDDYNAMODS_H

#include "Types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct DynaMod
{
    std::string Zone;
    std::string Tag;
    std::string State;
    bool Remove = false;
    int32 Index = 0;
    uint64 Originator = 0;

    bool operator==(DynaMod const&) const = default;
};

struct DynaModChange
{
    std::optional<DynaMod> Removed;
    std::optional<DynaMod> Added;

    bool Changed() const noexcept { return Removed || Added; }
};

class WizardDynaMods
{
public:
    WizardDynaMods() = default;
    explicit WizardDynaMods(std::vector<DynaMod> mods);

    DynaModChange Apply(std::string_view zone, std::string_view tag, std::string_view state, bool remove, uint64 originator);

    DynaMod const* Find(std::string_view zone, std::string_view tag) const;
    std::optional<bool> Visibility(std::string_view zone, std::string_view tag) const;
    std::vector<DynaMod> Armed(std::string_view zone) const;
    std::vector<DynaMod> const& All() const noexcept { return _mods; }

private:
    std::vector<DynaMod> _mods;
    int32 _lastIndex = 0;
};

#endif
