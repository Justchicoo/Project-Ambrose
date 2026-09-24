/*
 * Project Ambrose by Imjustchico
 * Drives a Botan HashFunction for the selected algorithm, keeping Botan headers out of the public interface.
 */

#include "CryptoHash.h"

#include <botan/hash.h>

#include <stdexcept>

struct CryptoHash::Impl
{
    std::unique_ptr<Botan::HashFunction> Function;
};

namespace
{
    std::unique_ptr<Botan::HashFunction> MakeFunction(CryptoHash::Algorithm algorithm)
    {
        return Botan::HashFunction::create_or_throw(CryptoHash::GetBotanName(algorithm));
    }
}

CryptoHash::CryptoHash(Algorithm algorithm) : _impl(std::make_unique<Impl>()), _algorithm(algorithm)
{
    _impl->Function = MakeFunction(algorithm);
}

CryptoHash::~CryptoHash() = default;

CryptoHash::CryptoHash(CryptoHash&& other) noexcept = default;

CryptoHash& CryptoHash::operator=(CryptoHash&& other) noexcept = default;

Botan::HashFunction& CryptoHash::GetFunction()
{
    if (!_impl)
    {
        _impl = std::make_unique<Impl>();
        _impl->Function = MakeFunction(_algorithm);
    }
    return *_impl->Function;
}

void CryptoHash::Update(std::span<uint8 const> data)
{
    Botan::HashFunction& function = GetFunction();
    if (!data.empty())
        function.update(data.data(), data.size());
}

void CryptoHash::Update(std::string_view data)
{
    Update(std::span<uint8 const>(reinterpret_cast<uint8 const*>(data.data()), data.size()));
}

void CryptoHash::Finalize(std::span<uint8> digest)
{
    if (digest.size() != GetDigestLength())
        throw std::invalid_argument("digest buffer has the wrong length");
    GetFunction().final(digest.data());
}

void CryptoHash::Reset()
{
    GetFunction().clear();
}

std::size_t CryptoHash::GetDigestLength() const noexcept
{
    return GetDigestLength(_algorithm);
}

CryptoHash::Algorithm CryptoHash::GetAlgorithm() const noexcept
{
    return _algorithm;
}

std::size_t CryptoHash::GetDigestLength(Algorithm algorithm) noexcept
{
    return algorithm == Algorithm::Sha512 ? 64 : 32;
}

std::string_view CryptoHash::GetBotanName(Algorithm algorithm) noexcept
{
    return algorithm == Algorithm::Sha512 ? "SHA-512" : "SHA-256";
}
