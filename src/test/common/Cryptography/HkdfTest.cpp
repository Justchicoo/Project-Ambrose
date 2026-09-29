/*
 * Project Ambrose by Imjustchico
 * Tests HKDF-SHA-256 against RFC 5869's first test case and that two info strings give two unrelated keys from one master key.
 */

#include "Hex.h"
#include "Hkdf.h"

#include <gtest/gtest.h>

#include <string_view>
#include <vector>

namespace
{
    std::span<uint8 const> Bytes(std::string_view text)
    {
        return { reinterpret_cast<uint8 const*>(text.data()), text.size() };
    }
}

TEST(HkdfTest, MatchesRfc5869TestCaseOne)
{
    std::vector<uint8> const inputKey(22, 0x0B);
    std::vector<uint8> salt;
    for (uint8 value = 0x00; value <= 0x0C; ++value)
        salt.push_back(value);
    std::vector<uint8> info;
    for (uint8 value = 0xF0; value <= 0xF9; ++value)
        info.push_back(value);
    std::vector<uint8> const key = Hkdf::Derive(inputKey, salt, info, 42);
    EXPECT_EQ(Hex::Encode(key), "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865");
}

TEST(HkdfTest, TwoPurposesDeriveTwoKeys)
{
    std::vector<uint8> const master(32, 0x5A);
    std::vector<uint8> const sealing = Hkdf::Derive(master, {}, Bytes("ambrose panel two-factor secret"), 32);
    std::vector<uint8> const hashing = Hkdf::Derive(master, {}, Bytes("ambrose panel recovery code"), 32);
    EXPECT_EQ(sealing.size(), 32u);
    EXPECT_NE(sealing, hashing);
    EXPECT_EQ(sealing, Hkdf::Derive(master, {}, Bytes("ambrose panel two-factor secret"), 32));
}
