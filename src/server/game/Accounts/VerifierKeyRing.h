/*
 * Project Ambrose by Imjustchico
 * The keys that encrypt stored password verifiers at rest: numbered AES-256 keys parsed from configuration, one of them active for new verifiers, all of them able to open older ones.
 */

#ifndef AMBROSE_VERIFIERKEYRING_H
#define AMBROSE_VERIFIERKEYRING_H

#include "AES256GCM.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>

class VerifierKeyRing
{
public:
    struct SealedVerifier
    {
        std::string Stored;
        uint8 KeyId = 0;
    };

    VerifierKeyRing() = default;
    VerifierKeyRing(VerifierKeyRing const& other);
    VerifierKeyRing& operator=(VerifierKeyRing const& other);
    ~VerifierKeyRing();

    static std::optional<VerifierKeyRing> Parse(std::string_view keys, uint32 activeKeyId, std::string& error);

    uint8 GetActiveKeyId() const noexcept { return _activeKeyId; }
    bool HasKey(uint8 keyId) const noexcept { return keyId == 0 || _keys.contains(keyId); }
    std::size_t GetKeyCount() const noexcept { return _keys.size(); }

    SealedVerifier Seal(std::string_view verifier, std::string_view username) const;
    std::optional<std::string> Open(std::string_view stored, uint8 keyId, std::string_view username) const;

private:
    void Wipe() noexcept;

    std::map<uint8, AES256GCM::Key> _keys;
    uint8 _activeKeyId = 0;
};

#endif
