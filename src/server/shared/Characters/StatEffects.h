/*
 * Project Ambrose by Imjustchico
 * WizStatisticEffectConfig as the server keeps it: every setting the client's class declares by its own name, the crit and block values and the pip conversion values each level band lists in order, and StatEffectSet, an immutable snapshot that validates them once and answers a setting by name.
 */

#ifndef AMBROSE_STATEFFECTS_H
#define AMBROSE_STATEFFECTS_H

#include "PlayerLevels.h"
#include "Types.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct CritAndBlockValues
{
    int32 MinLevel = 0;
    uint32 Position = 0;
    float CapValue = 0.0f;
    float CriticalHitScalarBase = 0.0f;
    float CriticalHitScalingFactor = 0.0f;
    float BlockScalarBase = 0.0f;
    float BlockScalingFactor = 0.0f;

    bool operator==(CritAndBlockValues const&) const = default;
};

struct PipConversionValues
{
    int32 MinLevel = 0;
    uint32 Position = 0;
    float CapValue = 0.0f;
    float ScalarBase = 0.0f;
    float ScalingFactor = 0.0f;

    bool operator==(PipConversionValues const&) const = default;
};

struct StatEffectData
{
    std::vector<ConfigValue> Settings;
    std::vector<CritAndBlockValues> CritAndBlock;
    std::vector<PipConversionValues> PipConversion;

    bool Empty() const noexcept { return Settings.empty() && CritAndBlock.empty() && PipConversion.empty(); }
};

class StatEffectSet
{
public:
    static constexpr std::size_t MaxSettingNameBytes = 128;
    static constexpr uint32 MaxBandPosition = 255;

    StatEffectSet() = default;
    StatEffectSet(StatEffectSet const&) = delete;
    StatEffectSet& operator=(StatEffectSet const&) = delete;

    static std::shared_ptr<StatEffectSet const> Build(StatEffectData data, std::vector<std::string>& errors);

    StatEffectData const& GetData() const noexcept { return _data; }
    bool IsEmpty() const noexcept { return _data.Empty(); }
    std::optional<double> Get(std::string_view name) const noexcept;

private:
    StatEffectData _data;
    std::map<std::string, double, std::less<>> _settings;
};

#endif
