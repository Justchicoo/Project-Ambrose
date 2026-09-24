/*
 * Project Ambrose by Imjustchico
 * Drives a Botan MessageAuthenticationCode for HMAC and verifies tags in constant time.
 */

#include "Hmac.h"
#include "ConstantTime.h"

#include <botan/mac.h>

#include <fmt/format.h>

#include <stdexcept>

struct Hmac::Impl
{
    std::unique_ptr<Botan::MessageAuthenticationCode> Code;
};

Hmac::Hmac(CryptoHash::Algorithm algorithm, std::span<uint8 const> key) : _impl(std::make_unique<Impl>()), _algorithm(algorithm)
{
    _impl->Code = Botan::MessageAuthenticationCode::create_or_throw(fmt::format("HMAC({})", CryptoHash::GetBotanName(algorithm)));
    _impl->Code->set_key(key.data(), key.size());
}

Hmac::~Hmac() = default;

Hmac::Hmac(Hmac&& other) noexcept = default;

Hmac& Hmac::operator=(Hmac&& other) noexcept = default;

Hmac& Hmac::Update(std::span<uint8 const> data)
{
    RequireState();
    if (!data.empty())
        _impl->Code->update(data.data(), data.size());
    return *this;
}

Hmac& Hmac::Update(std::string_view data)
{
    return Update(std::span<uint8 const>(reinterpret_cast<uint8 const*>(data.data()), data.size()));
}

std::vector<uint8> Hmac::Finalize()
{
    RequireState();
    std::vector<uint8> tag(GetLength());
    _impl->Code->final(tag.data());
    return tag;
}

void Hmac::RequireState() const
{
    if (!_impl)
        throw std::logic_error("Hmac used after it was moved from");
}

std::size_t Hmac::GetLength() const noexcept
{
    return CryptoHash::GetDigestLength(_algorithm);
}

std::vector<uint8> Hmac::Compute(CryptoHash::Algorithm algorithm, std::span<uint8 const> key, std::span<uint8 const> data)
{
    return Hmac(algorithm, key).Update(data).Finalize();
}

bool Hmac::Verify(CryptoHash::Algorithm algorithm, std::span<uint8 const> key, std::span<uint8 const> data, std::span<uint8 const> expected)
{
    std::vector<uint8> const actual = Compute(algorithm, key, data);
    return Ambrose::Crypto::ConstantTimeEquals(actual, expected);
}
