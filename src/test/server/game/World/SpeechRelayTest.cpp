/*
 * Project Ambrose by Imjustchico
 * Tests who hears what a wizard said or played: every other open wizard in the speaker's instance within the say range, in the order the world holds them, and anywhere in it when the range is 0; never a wizard in another instance or a closed one; the speaker only when its own client does not show what it did; and nobody when the speaker stands in no instance.
 */

#include "SpeechRelay.h"

#include <gtest/gtest.h>

#include <vector>

namespace
{
    SpeechListener At(uint32 map, float x, float y = 0.0f, float z = 0.0f, bool open = true)
    {
        return SpeechListener{ open, map, x, y, z };
    }
}

TEST(SpeechRelayTest, EveryOtherWizardInTheInstanceWithinRangeHearsTheSpeaker)
{
    std::vector<SpeechListener> const listeners{ At(7, 0.0f), At(7, 30.0f, 40.0f), At(7, 300.0f), At(8, 1.0f), At(7, 2.0f, 0.0f, 0.0f, false) };
    EXPECT_EQ(PlanHearers(listeners, 0, false, 50.0f), (std::vector<std::size_t>{ 1 })) << "not the far one, the other instance's or the closed one";
    EXPECT_EQ(PlanHearers(listeners, 0, false, 0.0f), (std::vector<std::size_t>{ 1, 2 })) << "0 reaches the whole instance";
    EXPECT_EQ(PlanHearers(listeners, 2, false, 0.0f), (std::vector<std::size_t>{ 0, 1 }));
}

TEST(SpeechRelayTest, TheSpeakerHearsItselfOnlyWhenItsClientDoesNotShowWhatItDid)
{
    std::vector<SpeechListener> const listeners{ At(7, 0.0f), At(7, 10.0f) };
    EXPECT_EQ(PlanHearers(listeners, 1, false, 0.0f), (std::vector<std::size_t>{ 0 }));
    EXPECT_EQ(PlanHearers(listeners, 1, true, 0.0f), (std::vector<std::size_t>{ 0, 1 }));
}

TEST(SpeechRelayTest, NobodyHearsASpeakerInNoInstance)
{
    std::vector<SpeechListener> listeners{ At(7, 0.0f), At(7, 1.0f) };
    listeners[0].MapId.reset();
    listeners[1].MapId.reset();
    EXPECT_TRUE(PlanHearers(listeners, 0, true, 0.0f).empty()) << "two wizards in no instance are not in the same one";
    EXPECT_TRUE(PlanHearers(listeners, 5, true, 0.0f).empty());
}
