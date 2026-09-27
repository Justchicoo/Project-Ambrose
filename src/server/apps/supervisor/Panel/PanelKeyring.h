/*
 * Project Ambrose by Imjustchico
 * The supervisor's keyring: a file of its own beside the panel's store rather than inside it, so a copy of the store never carries the keys that open its secrets, holding master keys that each have an id, one of them active for everything sealed from now on and the rest kept so what they sealed still opens, with a separate key derived for each purpose so the key that seals a two-factor secret is never the one that hashes a recovery code. It seals with AES-256-GCM bound to what the secret belongs to, opens with the key id the sealed row carries, and makes the keyed hashes a lookup finds rows by; the keys are wiped from memory when the keyring closes.
 */

#ifndef AMBROSE_PANELKEYRING_H
#define AMBROSE_PANELKEYRING_H

#include "AES256GCM.h"
#include "Types.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

enum class PanelKeyPurpose : uint8
{
    TwoFactorSecret,
    RecoveryCode
};

struct PanelSealed
{
    int64 KeyId = 0;
    std::vector<uint8> Bytes = {};
};

class PanelKeyring
{
public:
    static constexpr int Schema = 1;
    static constexpr std::size_t KeyBytes = AES256GCM::KeySize;
    static constexpr std::size_t PurposeCount = 2;

    PanelKeyring() = default;
    ~PanelKeyring();

    PanelKeyring(PanelKeyring const&) = delete;
    PanelKeyring& operator=(PanelKeyring const&) = delete;

    static std::string_view InfoOf(PanelKeyPurpose purpose) noexcept;

    bool Load(std::filesystem::path const& file, std::vector<std::string>& notes, std::vector<std::string>& warnings, std::string& error);
    void Close();

    bool IsOpen() const;
    bool WasCreated() const;
    std::filesystem::path GetFile() const;
    int64 ActiveId() const;
    std::vector<int64> KeyIds() const;

    std::optional<PanelSealed> Seal(PanelKeyPurpose purpose, std::span<uint8 const> plain, std::string_view associated) const;
    std::optional<std::vector<uint8>> Unseal(PanelKeyPurpose purpose, int64 keyId, std::span<uint8 const> sealed, std::string_view associated) const;
    std::optional<std::vector<uint8>> KeyedHash(PanelKeyPurpose purpose, int64 keyId, std::string_view data) const;

private:
    struct Key
    {
        int64 Id = 0;
        int64 CreatedEpochMs = 0;
        AES256GCM::Key Master = {};
        std::array<AES256GCM::Key, PurposeCount> Derived = {};
    };

    Key const* Find(int64 keyId) const;
    static void Derive(Key& key);
    static bool Parse(std::string const& text, std::vector<Key>& keys, int64& active, std::string& error);
    static std::string Render(std::vector<Key> const& keys, int64 active);
    void Wipe();

    mutable std::shared_mutex _mutex;
    std::filesystem::path _file;
    std::vector<Key> _keys;
    int64 _active = 0;
    bool _created = false;
};

#endif
