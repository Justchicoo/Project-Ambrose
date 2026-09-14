/*
 * Project Ambrose by Imjustchico
 * The Rec1 login field cipher: Twofish-256 in OFB mode with a key built from the session id and offer time and a fixed descending IV, with no padding.
 */

#ifndef AMBROSE_REC1_H
#define AMBROSE_REC1_H

#include "LoginSalt.h"

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Rec1
{
    std::array<uint8, 32> DeriveKey(LoginSalt const& salt) noexcept;
    std::array<uint8, 16> DeriveIv() noexcept;
    std::vector<uint8> Transform(std::span<uint8 const> data, LoginSalt const& salt);
    std::string Decode(std::string_view encoded, LoginSalt const& salt);
    std::string Encode(std::string_view plain, LoginSalt const& salt);
}

#endif
