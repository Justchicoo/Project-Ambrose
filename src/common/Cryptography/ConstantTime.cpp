/*
 * Project Ambrose by Imjustchico
 * Compares byte strings and text through Botan's constant-time routine so timing does not reveal where they differ.
 */

#include "ConstantTime.h"

#include <botan/mem_ops.h>

bool Ambrose::Crypto::ConstantTimeEquals(std::span<uint8 const> left, std::span<uint8 const> right) noexcept
{
    if (left.size() != right.size())
        return false;
    if (left.empty())
        return true;
    return Botan::constant_time_compare(left.data(), right.data(), left.size());
}

bool Ambrose::Crypto::ConstantTimeEquals(std::string_view left, std::string_view right) noexcept
{
    return ConstantTimeEquals(std::span<uint8 const>(reinterpret_cast<uint8 const*>(left.data()), left.size()), std::span<uint8 const>(reinterpret_cast<uint8 const*>(right.data()), right.size()));
}
