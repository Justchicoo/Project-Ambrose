/*
 * Project Ambrose by Imjustchico
 * Fixed-length SHA-256 wrapper that finalizes into a 32-byte array and resets for reuse.
 */

#include "SHA256.h"

SHA256::SHA256() : _hash(CryptoHash::Algorithm::Sha256)
{
}

SHA256& SHA256::Update(std::span<uint8 const> data)
{
    _hash.Update(data);
    return *this;
}

SHA256& SHA256::Update(std::string_view data)
{
    _hash.Update(data);
    return *this;
}

SHA256::Digest SHA256::Finalize()
{
    Digest digest{};
    _hash.Finalize(digest);
    return digest;
}

void SHA256::Reset()
{
    _hash.Reset();
}

SHA256::Digest SHA256::GetDigestOf(std::span<uint8 const> data)
{
    return SHA256().Update(data).Finalize();
}

SHA256::Digest SHA256::GetDigestOf(std::string_view data)
{
    return SHA256().Update(data).Finalize();
}
