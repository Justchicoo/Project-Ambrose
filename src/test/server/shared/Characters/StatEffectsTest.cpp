/*
 * Project Ambrose by Imjustchico
 * Tests the stat effect set: a complete set answers its settings by name and keeps its bands in order, an empty set is valid, and a setting named twice, too long or not finite, a band position given twice, past the limit or leaving a gap, a band value that is not finite, and bands without settings are each refused with the reason.
 */

#include "StatEffects.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace
{
    StatEffectData Complete()
    {
        StatEffectData data;
        data.Settings = { { "m_criticalHitLevelThreshold", 50.0 }, { "m_shadowPipMax", 2.0 } };
        data.CritAndBlock = { { 0, 0, 1.0f, 0.00205f, 0.002f, 0.00255f, 0.002f }, { 50, 0, 1.0f, 0.00205f, 0.002f, 0.0025f, 0.002f }, { 50, 1, 0.03f, 0.00205f, 0.002f, 0.00255f, 0.002f } };
        data.PipConversion = { { 0, 0, 1.0f, 0.00205f, 0.009f }, { 0, 1, 0.33f, 0.00205f, 0.0085f } };
        return data;
    }

    bool Mentions(std::vector<std::string> const& errors, std::string_view text)
    {
        return std::any_of(errors.begin(), errors.end(), [text](std::string const& error) { return error.find(text) != std::string::npos; });
    }
}

TEST(StatEffectsTest, ACompleteSetAnswersItsSettingsAndKeepsItsBands)
{
    std::vector<std::string> errors;
    std::shared_ptr<StatEffectSet const> const set = StatEffectSet::Build(Complete(), errors);
    ASSERT_TRUE(set) << errors.front();
    EXPECT_FALSE(set->IsEmpty());
    EXPECT_EQ(set->Get("m_shadowPipMax"), 2.0);
    EXPECT_FALSE(set->Get("m_blockLevelThreshold"));
    EXPECT_EQ(set->GetData().CritAndBlock, Complete().CritAndBlock);
    EXPECT_EQ(set->GetData().PipConversion, Complete().PipConversion);

    std::shared_ptr<StatEffectSet const> const empty = StatEffectSet::Build({}, errors);
    ASSERT_TRUE(empty);
    EXPECT_TRUE(empty->IsEmpty());
    EXPECT_TRUE(errors.empty());
}

TEST(StatEffectsTest, EachBadRowIsRefusedWithItsReason)
{
    auto const refused = [](StatEffectData data, std::string_view expected)
    {
        std::vector<std::string> errors;
        EXPECT_FALSE(StatEffectSet::Build(std::move(data), errors)) << expected;
        EXPECT_TRUE(Mentions(errors, expected)) << "wanted: " << expected << "\n" << (errors.empty() ? std::string() : errors.front());
    };

    StatEffectData data = Complete();
    data.Settings.push_back({ "m_shadowPipMax", 3.0 });
    refused(data, "stat_effect_config lists m_shadowPipMax twice");
    data = Complete();
    data.Settings.push_back({ std::string(StatEffectSet::MaxSettingNameBytes + 1, 'm'), 1.0 });
    refused(data, "stat_effect_config has a setting whose name is 129 bytes");
    data = Complete();
    data.Settings[0].Value = std::numeric_limits<double>::infinity();
    refused(data, "stat_effect_config gives m_criticalHitLevelThreshold a value that is not a finite number");
    data = Complete();
    data.CritAndBlock.push_back({ 50, 1, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f });
    refused(data, "stat_crit_block_band lists position 1 of the band from level 50 twice");
    data = Complete();
    data.CritAndBlock.push_back({ 50, 3, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f });
    refused(data, "stat_crit_block_band gives the band from level 50 3 positions that do not run from 0 without a gap");
    data = Complete();
    data.PipConversion.push_back({ 0, 256, 0.5f, 0.0f, 0.0f });
    refused(data, "stat_pip_conversion_band gives the band from level 0 position 256, above the 255 a band can hold");
    data = Complete();
    data.PipConversion[1].ScalingFactor = std::numeric_limits<float>::quiet_NaN();
    refused(data, "stat_pip_conversion_band gives the band from level 0 a value at position 1 that is not a finite number");
    data = Complete();
    data.Settings.clear();
    refused(data, "stat_effect_config has no settings, while its bands have rows");
}
