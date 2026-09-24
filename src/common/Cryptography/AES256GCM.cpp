/*
 * Project Ambrose by Imjustchico
 * Seals and opens nonce, ciphertext and tag buffers through Botan's AES-256/GCM, returning nothing when opening fails for any reason, including a tag or associated data that does not match.
 */

#include "AES256GCM.h"
#include "CryptoRandom.h"

#include <botan/aead.h>
#include <botan/exceptn.h>

#include <memory>

std::vector<uint8> AES256GCM::Seal(Key const& key, std::span<uint8 const> plaintext, std::span<uint8 const> associatedData)
{
    return SealWithNonce(key, Ambrose::Crypto::GetRandomArray<NonceSize>(), plaintext, associatedData);
}

std::vector<uint8> AES256GCM::SealWithNonce(Key const& key, Nonce const& nonce, std::span<uint8 const> plaintext, std::span<uint8 const> associatedData)
{
    std::unique_ptr<Botan::AEAD_Mode> const mode = Botan::AEAD_Mode::create_or_throw("AES-256/GCM", Botan::Cipher_Dir::Encryption);
    mode->set_key(std::span<uint8 const>(key));
    mode->set_associated_data(associatedData);
    mode->start(std::span<uint8 const>(nonce));
    Botan::secure_vector<uint8> buffer(plaintext.begin(), plaintext.end());
    mode->finish(buffer);

    std::vector<uint8> sealed;
    sealed.reserve(NonceSize + buffer.size());
    sealed.insert(sealed.end(), nonce.begin(), nonce.end());
    sealed.insert(sealed.end(), buffer.begin(), buffer.end());
    return sealed;
}

std::optional<std::vector<uint8>> AES256GCM::Open(Key const& key, std::span<uint8 const> sealed, std::span<uint8 const> associatedData)
{
    if (sealed.size() < NonceSize + TagSize)
        return std::nullopt;
    try
    {
        std::unique_ptr<Botan::AEAD_Mode> const mode = Botan::AEAD_Mode::create_or_throw("AES-256/GCM", Botan::Cipher_Dir::Decryption);
        mode->set_key(std::span<uint8 const>(key));
        mode->set_associated_data(associatedData);
        mode->start(sealed.first(NonceSize));
        Botan::secure_vector<uint8> buffer(sealed.begin() + NonceSize, sealed.end());
        mode->finish(buffer);
        return std::vector<uint8>(buffer.begin(), buffer.end());
    }
    catch (Botan::Exception const&)
    {
        return std::nullopt;
    }
}
