/*
 * Project Ambrose by Imjustchico
 * Checks a stat effect set whole before anything reads it: each setting is named once, fits its column and holds a finite number, each band entry's values are finite, a band lists each position once and runs from position 0 without a gap, and a set holding bands or settings holds settings; a set with nothing in it is valid and empty.
 */

#include "StatEffects.h"

#include <fmt/format.h>

#include <cmath>
#include <set>
#include <utility>

namespace
{
    template<typename Entry, typename FiniteCheck>
    void CheckBands(std::vector<Entry> const& entries, std::string_view table, FiniteCheck finite, std::vector<std::string>& errors)
    {
        std::map<int32, std::set<uint32>> bands;
        for (Entry const& entry : entries)
        {
            if (!finite(entry))
                errors.push_back(fmt::format("{} gives the band from level {} a value at position {} that is not a finite number", table, entry.MinLevel, entry.Position));
            if (entry.Position > StatEffectSet::MaxBandPosition)
                errors.push_back(fmt::format("{} gives the band from level {} position {}, above the {} a band can hold", table, entry.MinLevel, entry.Position, StatEffectSet::MaxBandPosition));
            else if (!bands[entry.MinLevel].insert(entry.Position).second)
                errors.push_back(fmt::format("{} lists position {} of the band from level {} twice", table, entry.Position, entry.MinLevel));
        }
        for (auto const& [minLevel, positions] : bands)
            if (!positions.empty() && *positions.rbegin() + 1 != positions.size())
                errors.push_back(fmt::format("{} gives the band from level {} {} positions that do not run from 0 without a gap", table, minLevel, positions.size()));
    }
}

std::shared_ptr<StatEffectSet const> StatEffectSet::Build(StatEffectData data, std::vector<std::string>& errors)
{
    std::size_t const before = errors.size();
    auto set = std::make_shared<StatEffectSet>();
    set->_data = std::move(data);
    for (ConfigValue const& setting : set->_data.Settings)
    {
        if (setting.Name.empty() || setting.Name.size() > MaxSettingNameBytes)
            errors.push_back(fmt::format("stat_effect_config has a setting whose name is {} bytes, where 1 to {} fit", setting.Name.size(), MaxSettingNameBytes));
        else if (!std::isfinite(setting.Value))
            errors.push_back(fmt::format("stat_effect_config gives {} a value that is not a finite number", setting.Name));
        else if (!set->_settings.emplace(setting.Name, setting.Value).second)
            errors.push_back(fmt::format("stat_effect_config lists {} twice", setting.Name));
    }
    CheckBands(set->_data.CritAndBlock, "stat_crit_block_band", [](CritAndBlockValues const& entry)
    {
        return std::isfinite(entry.CapValue) && std::isfinite(entry.CriticalHitScalarBase) && std::isfinite(entry.CriticalHitScalingFactor) && std::isfinite(entry.BlockScalarBase)
            && std::isfinite(entry.BlockScalingFactor);
    }, errors);
    CheckBands(set->_data.PipConversion, "stat_pip_conversion_band", [](PipConversionValues const& entry)
    {
        return std::isfinite(entry.CapValue) && std::isfinite(entry.ScalarBase) && std::isfinite(entry.ScalingFactor);
    }, errors);
    if (!set->_data.Empty() && set->_data.Settings.empty())
        errors.emplace_back("stat_effect_config has no settings, while its bands have rows");
    if (errors.size() != before)
        return nullptr;
    return set;
}

std::optional<double> StatEffectSet::Get(std::string_view name) const noexcept
{
    auto const found = _settings.find(name);
    if (found == _settings.end())
        return std::nullopt;
    return found->second;
}
