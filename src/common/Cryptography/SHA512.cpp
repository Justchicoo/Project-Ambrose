/*
 * Project Ambrose by Imjustchico
 * Fixed-length SHA-512 wrapper that finalizes into a 64-byte array and resets for reuse.
 */

#include "SHA512.h"

SHA512::SHA512() : _hash(CryptoHash::Algorithm::Sha512)
{
}

SHA512& SHA512::Update(std::span<uint8 const> data)
{
    _hash.Update(data);
    return *this;
}

SHA512& SHA512::Update(std::string_view data)
{
    _hash.Update(data);
    return *this;
}

SHA512::Digest SHA512::Finalize()
{
    Digest digest{};
    _hash.Finalize(digest);
    return digest;
}

void SHA512::Reset()
{
    _hash.Reset();
}

SHA512::Digest SHA512::GetDigestOf(std::span<uint8 const> data)
{
    return SHA512().Update(data).Finalize();
}

SHA512::Digest SHA512::GetDigestOf(std::string_view data)
{
    return SHA512().Update(data).Finalize();
}
