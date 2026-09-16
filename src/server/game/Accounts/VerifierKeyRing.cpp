/*
 * Project Ambrose by Imjustchico
 * Parses id:hex key lists without echoing key material, seals verifiers as base64 nonce, ciphertext and tag bound to the lowercased username, opens them by key id, and scrubs key bytes when a ring is copied over or destroyed.
 */

#include "VerifierKeyRing.h"
#include "Base64.h"
#include "Hex.h"
#include "SecureMemory.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>

namespace
{
    std::string AssociatedData(std::string_view username)
    {
        return "ambrose.verifier:" + Ambrose::ToLower(username);
    }

    std::span<uint8 const> AsBytes(std::string_view text) noexcept
    {
        return std::span<uint8 const>(reinterpret_cast<uint8 const*>(text.data()), text.size());
    }
}

VerifierKeyRing::VerifierKeyRing(VerifierKeyRing const& other)
    : _keys(other._keys), _activeKeyId(other._activeKeyId)
{
}

VerifierKeyRing& VerifierKeyRing::operator=(VerifierKeyRing const& other)
{
    if (this != &other)
    {
        Wipe();
        _keys = other._keys;
        _activeKeyId = other._activeKeyId;
    }
    return *this;
}

VerifierKeyRing::~VerifierKeyRing()
{
    Wipe();
}

void VerifierKeyRing::Wipe() noexcept
{
    for (auto& [id, key] : _keys)
        Ambrose::Crypto::SecureWipe(key);
    _keys.clear();
}

std::optional<VerifierKeyRing> VerifierKeyRing::Parse(std::string_view keys, uint32 activeKeyId, std::string& error)
{
    VerifierKeyRing ring;
    for (std::string_view const rawEntry : Ambrose::Tokenize(keys, ',', false))
    {
        std::string_view const entry = Ambrose::Trim(rawEntry);
        if (entry.empty())
            continue;
        std::size_t const colon = entry.find(':');
        std::optional<uint32> const id = colon == std::string_view::npos ? std::nullopt : Ambrose::StringTo<uint32>(Ambrose::Trim(entry.substr(0, colon)));
        if (!id || *id == 0 || *id > 255)
        {
            error = "each verifier key must be written id:hex with an id from 1 to 255";
            return std::nullopt;
        }
        if (ring._keys.contains(static_cast<uint8>(*id)))
        {
            error = fmt::format("verifier key {} is listed twice", *id);
            return std::nullopt;
        }
        std::string_view const hex = Ambrose::Trim(entry.substr(colon + 1));
        std::optional<std::vector<uint8>> bytes = hex.size() == AES256GCM::KeySize * 2 ? Hex::Decode(hex) : std::nullopt;
        if (!bytes || bytes->size() != AES256GCM::KeySize)
        {
            if (bytes)
                Ambrose::Crypto::SecureWipe(*bytes);
            error = fmt::format("verifier key {} must be {} hex digits", *id, AES256GCM::KeySize * 2);
            return std::nullopt;
        }
        AES256GCM::Key& key = ring._keys[static_cast<uint8>(*id)];
        std::copy(bytes->begin(), bytes->end(), key.begin());
        Ambrose::Crypto::SecureWipe(*bytes);
    }
    if (activeKeyId > 255 || (activeKeyId != 0 && !ring._keys.contains(static_cast<uint8>(activeKeyId))))
    {
        error = fmt::format("the active verifier key {} is not in the key list", activeKeyId);
        return std::nullopt;
    }
    ring._activeKeyId = static_cast<uint8>(activeKeyId);
    return ring;
}

VerifierKeyRing::SealedVerifier VerifierKeyRing::Seal(std::string_view verifier, std::string_view username) const
{
    auto const key = _keys.find(_activeKeyId);
    if (_activeKeyId == 0 || key == _keys.end())
        return { std::string(verifier), 0 };
    std::string const associatedData = AssociatedData(username);
    std::vector<uint8> const sealed = AES256GCM::Seal(key->second, AsBytes(verifier), AsBytes(associatedData));
    return { Base64::Encode(sealed), _activeKeyId };
}

std::optional<std::string> VerifierKeyRing::Open(std::string_view stored, uint8 keyId, std::string_view username) const
{
    if (keyId == 0)
        return std::string(stored);
    auto const key = _keys.find(keyId);
    if (key == _keys.end())
        return std::nullopt;
    std::optional<std::vector<uint8>> const sealed = Base64::Decode(stored);
    if (!sealed)
        return std::nullopt;
    std::string const associatedData = AssociatedData(username);
    std::optional<std::vector<uint8>> const opened = AES256GCM::Open(key->second, *sealed, AsBytes(associatedData));
    if (!opened)
        return std::nullopt;
    return std::string(opened->begin(), opened->end());
}
