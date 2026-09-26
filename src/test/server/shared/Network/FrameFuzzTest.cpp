/*
 * Project Ambrose by Imjustchico
 * Deterministic randomized coverage of fragmented frame decoding on compilers without libFuzzer.
 */

#include "FrameReassembler.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <random>
#include <vector>

TEST(FrameFuzzTest, RandomizedFragmentsDoNotEscapeFrameBounds)
{
    std::mt19937 random(0xF00D);
    std::uniform_int_distribution<std::size_t> sizeDistribution(0, 8192);
    std::uniform_int_distribution<uint32> byteDistribution(0, 255);

    for (std::size_t iteration = 0; iteration < 10000; ++iteration)
    {
        std::vector<uint8> input(sizeDistribution(random));
        for (uint8& byte : input)
            byte = static_cast<uint8>(byteDistribution(random));

        FrameReassembler reassembler;
        for (std::size_t offset = 0; offset < input.size() && !reassembler.HasError(); offset += 128)
        {
            std::size_t const count = std::min<std::size_t>(128, input.size() - offset);
            reassembler.Feed(std::span<uint8 const>(input.data() + offset, count));
            while (reassembler.Next())
            {
            }
            EXPECT_LE(reassembler.GetBufferedSize(), FrameLimits::DefaultMaxFrameSize);
        }
    }
}
