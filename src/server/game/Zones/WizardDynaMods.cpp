/*
 * Project Ambrose by Imjustchico
 * Keeps one mod for each zone and tag, the first of any stored twice, giving each new mod the next index after the client's placeholder and replacing a changed one under a fresh index so the client can drop the old by its index and take the new, and leaving an identical one alone; a removal mod hides its tagged objects, one that shows its object shows them and any other leaves them to their requirements, a zone's mods other than removals are the ones its client arms spawners for, and erasing a tag takes every zone's mod of it away without giving its index out again.
 */

#include "WizardDynaMods.h"

#include <algorithm>
#include <utility>

WizardDynaMods::WizardDynaMods(std::vector<DynaMod> mods)
{
    for (DynaMod& mod : mods)
    {
        if (Find(mod.Zone, mod.Tag))
            continue;
        _lastIndex = std::max(_lastIndex, mod.Index);
        _mods.push_back(std::move(mod));
    }
}

DynaModChange WizardDynaMods::Apply(std::string_view zone, std::string_view tag, std::string_view state, bool remove, uint64 originator, bool showsObject)
{
    DynaModChange change;
    auto const found = std::ranges::find_if(_mods, [zone, tag](DynaMod const& mod) { return mod.Zone == zone && mod.Tag == tag; });
    if (found != _mods.end())
    {
        if (found->State == state && found->Remove == remove && found->Originator == originator && found->ShowsObject == showsObject)
            return change;
        change.Removed = *found;
        _mods.erase(found);
    }
    DynaMod added{ std::string(zone), std::string(tag), std::string(state), remove, ++_lastIndex, originator, showsObject };
    _mods.push_back(added);
    change.Added = std::move(added);
    return change;
}

std::vector<DynaMod> WizardDynaMods::Erase(std::string_view tag)
{
    std::vector<DynaMod> erased;
    std::erase_if(_mods, [tag, &erased](DynaMod const& mod)
    {
        if (mod.Tag != tag)
            return false;
        erased.push_back(mod);
        return true;
    });
    return erased;
}

DynaMod const* WizardDynaMods::Find(std::string_view zone, std::string_view tag) const
{
    auto const found = std::ranges::find_if(_mods, [zone, tag](DynaMod const& mod) { return mod.Zone == zone && mod.Tag == tag; });
    return found == _mods.end() ? nullptr : &*found;
}

std::optional<bool> WizardDynaMods::Visibility(std::string_view zone, std::string_view tag) const
{
    if (tag.empty())
        return std::nullopt;
    DynaMod const* const mod = Find(zone, tag);
    if (!mod)
        return std::nullopt;
    if (mod->Remove)
        return false;
    return mod->ShowsObject ? std::optional<bool>(true) : std::nullopt;
}

std::vector<DynaMod> WizardDynaMods::Armed(std::string_view zone) const
{
    std::vector<DynaMod> armed;
    for (DynaMod const& mod : _mods)
        if (mod.Zone == zone && !mod.Remove)
            armed.push_back(mod);
    return armed;
}
