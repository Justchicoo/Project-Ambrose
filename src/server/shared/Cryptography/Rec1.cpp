/*
 * Project Ambrose by Imjustchico
 * Fills the Rec1 key with 0x17 plus its index, overwrites the session id, seconds and milliseconds bytes, counts the IV down from 0xB6, and runs Twofish-OFB over the field bytes.
 */

#include "Rec1.h"
#include "TwofishOfb.h"

std::array<uint8, 32> Rec1::DeriveKey(LoginSalt const& salt) noexcept
{
    std::array<uint8, 32> key{};
    for (std::size_t i = 0; i < key.size(); ++i)
        key[i] = static_cast<uint8>(0x17 + i);
    key[4] = static_cast<uint8>(salt.SessionId & 0xFF);
    key[5] = 0;
    key[6] = static_cast<uint8>((salt.SessionId >> 8) & 0xFF);
    key[8] = static_cast<uint8>(salt.Seconds & 0xFF);
    key[9] = static_cast<uint8>((salt.Seconds >> 16) & 0xFF);
    key[12] = static_cast<uint8>((salt.Seconds >> 8) & 0xFF);
    key[13] = static_cast<uint8>((salt.Seconds >> 24) & 0xFF);
    key[14] = static_cast<uint8>(salt.Milliseconds & 0xFF);
    key[15] = static_cast<uint8>((salt.Milliseconds >> 8) & 0xFF);
    return key;
}

std::array<uint8, 16> Rec1::DeriveIv() noexcept
{
    std::array<uint8, 16> iv{};
    for (std::size_t i = 0; i < iv.size(); ++i)
        iv[i] = static_cast<uint8>(0xB6 - i);
    return iv;
}

std::vector<uint8> Rec1::Transform(std::span<uint8 const> data, LoginSalt const& salt)
{
    std::array<uint8, 32> const key = DeriveKey(salt);
    std::array<uint8, 16> const iv = DeriveIv();
    return TwofishOfb(key, iv).Process(data);
}

std::string Rec1::Decode(std::string_view encoded, LoginSalt const& salt)
{
    std::vector<uint8> const plain = Transform(std::span<uint8 const>(reinterpret_cast<uint8 const*>(encoded.data()), encoded.size()), salt);
    return std::string(plain.begin(), plain.end());
}

std::string Rec1::Encode(std::string_view plain, LoginSalt const& salt)
{
    std::vector<uint8> const encoded = Transform(std::span<uint8 const>(reinterpret_cast<uint8 const*>(plain.data()), plain.size()), salt);
    return std::string(encoded.begin(), encoded.end());
}
