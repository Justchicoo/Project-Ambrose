/*
 * Project Ambrose by Imjustchico
 * Reads the keyring as JSON holding each key's id, when it was made and its 32 bytes, wrapped on Windows with DPAPI for the account the supervisor runs as and written with an access list naming only that account, the local system account and the Administrators group, and on other systems kept at mode 0600 in a folder tightened to 0700; a missing keyring is made with one key, while one that is there but cannot be opened or read stops the panel and is left exactly as it was, since replacing it would leave every secret it sealed unreadable. Each purpose's key is derived once with HKDF-SHA-256 and its own info string, and every copy of a key, the file's text included, is wiped once it has been used.
 */

#include "PanelKeyring.h"
#include "AdminToken.h"
#include "Base64.h"
#include "ConfigMgr.h"
#include "CryptoRandom.h"
#include "Hkdf.h"
#include "Hmac.h"
#include "PanelStore.h"
#include "SecureMemory.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <mutex>
#include <set>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <windows.h>

#include <wincrypt.h>
#include <dpapi.h>
#endif

namespace
{
    constexpr std::size_t MaxFileBytes = 1024 * 1024;

    void WipeText(std::string& text) noexcept
    {
        Ambrose::Crypto::SecureWipe(std::span<uint8>(reinterpret_cast<uint8*>(text.data()), text.size()));
        text.clear();
    }

    std::span<uint8 const> Bytes(std::string_view text)
    {
        return { reinterpret_cast<uint8 const*>(text.data()), text.size() };
    }

    int64 IntegerOf(nlohmann::json const& object, char const* key)
    {
        auto const found = object.find(key);
        return found != object.end() && found->is_number_integer() ? found->get<int64>() : 0;
    }

#ifdef _WIN32
    std::string SystemError(DWORD code)
    {
        return std::error_code(static_cast<int>(code), std::system_category()).message();
    }

    bool Wrap(std::string const& plain, std::string& wrapped, std::string& error)
    {
        DATA_BLOB in{ static_cast<DWORD>(plain.size()), reinterpret_cast<BYTE*>(const_cast<char*>(plain.data())) };
        DATA_BLOB out{};
        if (!::CryptProtectData(&in, L"Ambrose panel keyring", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        {
            error = fmt::format("Windows would not protect it for this account: {}", SystemError(::GetLastError()));
            return false;
        }
        wrapped.assign(reinterpret_cast<char const*>(out.pbData), out.cbData);
        ::LocalFree(out.pbData);
        return true;
    }

    bool Unwrap(std::string const& wrapped, std::string& plain, std::string& error)
    {
        DATA_BLOB in{ static_cast<DWORD>(wrapped.size()), reinterpret_cast<BYTE*>(const_cast<char*>(wrapped.data())) };
        DATA_BLOB out{};
        if (!::CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        {
            error = fmt::format("Windows would not open it for this account, which is the only account that can: {}", SystemError(::GetLastError()));
            return false;
        }
        plain.assign(reinterpret_cast<char const*>(out.pbData), out.cbData);
        ::SecureZeroMemory(out.pbData, out.cbData);
        ::LocalFree(out.pbData);
        return true;
    }
#else
    bool Wrap(std::string const& plain, std::string& wrapped, std::string&)
    {
        wrapped = plain;
        return true;
    }

    bool Unwrap(std::string const& wrapped, std::string& plain, std::string&)
    {
        plain = wrapped;
        return true;
    }

    void TightenFolder(std::filesystem::path const& folder, std::vector<std::string>& notes, std::vector<std::string>& warnings)
    {
        std::error_code code;
        std::filesystem::perms const held = std::filesystem::status(folder, code).permissions();
        if (code)
            return;
        std::filesystem::perms const others = std::filesystem::perms::group_all | std::filesystem::perms::others_all;
        if ((held & others) == std::filesystem::perms::none)
            return;
        std::filesystem::permissions(folder, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace, code);
        if (code)
            warnings.push_back(fmt::format("The folder {} holding the panel keyring is readable by others and could not be made its owner's alone: {}", ConfigMgr::PathToUtf8(folder), code.message()));
        else
            notes.push_back(fmt::format("The folder {} holding the panel keyring was readable by others, so it is now its owner's alone", ConfigMgr::PathToUtf8(folder)));
    }
#endif

    bool ReadKeyring(std::filesystem::path const& file, std::string& contents, std::string& error)
    {
        std::error_code code;
        std::uintmax_t const size = std::filesystem::file_size(file, code);
        if (code)
        {
            error = code.message();
            return false;
        }
        if (size > MaxFileBytes)
        {
            error = fmt::format("it is {} bytes, far more than a keyring holds", size);
            return false;
        }
        std::ifstream stream(file, std::ios::binary);
        if (!stream)
        {
            error = "it cannot be opened for reading";
            return false;
        }
        contents.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        return true;
    }
}

PanelKeyring::~PanelKeyring()
{
    Close();
}

std::string_view PanelKeyring::InfoOf(PanelKeyPurpose purpose) noexcept
{
    switch (purpose)
    {
        case PanelKeyPurpose::TwoFactorSecret: return "ambrose panel two-factor secret";
        case PanelKeyPurpose::RecoveryCode: return "ambrose panel recovery code";
        case PanelKeyPurpose::SmtpPassword: return "ambrose panel smtp password";
        case PanelKeyPurpose::CaptchaSecret: break;
    }
    return "ambrose panel captcha secret";
}

void PanelKeyring::Derive(Key& key)
{
    for (std::size_t index = 0; index < PurposeCount; ++index)
    {
        std::vector<uint8> derived = Hkdf::Derive(key.Master, {}, Bytes(InfoOf(static_cast<PanelKeyPurpose>(index))), KeyBytes);
        std::copy(derived.begin(), derived.end(), key.Derived[index].begin());
        Ambrose::Crypto::SecureWipe(derived);
    }
}

bool PanelKeyring::Parse(std::string const& text, std::vector<Key>& keys, int64& active, std::string& error)
{
    nlohmann::json const document = nlohmann::json::parse(text, nullptr, false);
    if (!document.is_object())
    {
        error = "it does not hold a keyring";
        return false;
    }
    if (int64 const schema = IntegerOf(document, "schema"); schema != Schema)
    {
        error = fmt::format("it is keyring schema {}, and this supervisor reads schema {}", schema, Schema);
        return false;
    }
    auto const listed = document.find("keys");
    if (listed == document.end() || !listed->is_array() || listed->empty())
    {
        error = "it names no key";
        return false;
    }
    std::set<int64> seen;
    for (nlohmann::json const& entry : *listed)
    {
        if (!entry.is_object() || !entry.contains("id") || !entry["id"].is_number_integer() || !entry.contains("secret") || !entry["secret"].is_string())
        {
            error = "a key in it has no id or no secret";
            return false;
        }
        Key key;
        key.Id = entry["id"].get<int64>();
        key.CreatedEpochMs = IntegerOf(entry, "created_epoch_ms");
        std::optional<std::vector<uint8>> secret = Base64::Decode(entry["secret"].get_ref<std::string const&>(), Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
        if (key.Id <= 0 || !seen.insert(key.Id).second || !secret || secret->size() != KeyBytes)
        {
            if (secret)
                Ambrose::Crypto::SecureWipe(*secret);
            error = fmt::format("key {} in it is not a 32-byte key with an id of its own", key.Id);
            return false;
        }
        std::copy(secret->begin(), secret->end(), key.Master.begin());
        Ambrose::Crypto::SecureWipe(*secret);
        Derive(key);
        keys.push_back(key);
        Ambrose::Crypto::SecureWipe(key.Master);
        for (AES256GCM::Key& derived : key.Derived)
            Ambrose::Crypto::SecureWipe(derived);
    }
    active = IntegerOf(document, "active");
    if (!seen.contains(active))
    {
        error = fmt::format("its active key {} is not one of the keys it holds", active);
        return false;
    }
    return true;
}

std::string PanelKeyring::Render(std::vector<Key> const& keys, int64 active)
{
    nlohmann::json listed = nlohmann::json::array();
    for (Key const& key : keys)
        listed.push_back({ { "id", key.Id }, { "created_epoch_ms", key.CreatedEpochMs }, { "secret", Base64::Encode(key.Master, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted) } });
    nlohmann::json document;
    document["schema"] = Schema;
    document["active"] = active;
    document["keys"] = std::move(listed);
    return document.dump();
}

bool PanelKeyring::Load(std::filesystem::path const& file, std::vector<std::string>& notes, std::vector<std::string>& warnings, std::string& error)
{
    std::unique_lock const lock(_mutex);
    Wipe();
    _file = file;
    _created = false;
    std::string const path = ConfigMgr::PathToUtf8(file);
    std::error_code code;
    bool const exists = std::filesystem::exists(file, code);
    if (code)
    {
        error = fmt::format("the panel keyring {} cannot be looked for: {}", path, code.message());
        return false;
    }

    if (exists)
    {
        std::string wrapped;
        std::string text;
        std::string why;
        std::vector<Key> keys;
        int64 active = 0;
        bool const read = ReadKeyring(file, wrapped, why) && Unwrap(wrapped, text, why) && Parse(text, keys, active, why);
        WipeText(text);
        WipeText(wrapped);
        if (!read)
        {
            for (Key& key : keys)
            {
                Ambrose::Crypto::SecureWipe(key.Master);
                for (AES256GCM::Key& derived : key.Derived)
                    Ambrose::Crypto::SecureWipe(derived);
            }
            error = fmt::format("the panel keyring {} cannot be read: {}. It is left as it is, because a new keyring would leave every two-factor secret it sealed "
                "unreadable; restore it from a backup, or set Panel.KeyringFile to start a new one", path, why);
            return false;
        }
        _keys = std::move(keys);
        _active = active;
        std::string secured;
        if (!AdminToken::SecureFile(file, secured, AdminToken::SecretReaders::OwnerAndAdministrators))
            warnings.push_back(fmt::format("The panel keyring {} could not be limited to this account and the machine's administrators: {}", path, secured));
#ifndef _WIN32
        TightenFolder(file.parent_path(), notes, warnings);
#endif
        return true;
    }

    std::vector<Key> fresh(1);
    Key& key = fresh.front();
    key.Id = 1;
    key.CreatedEpochMs = PanelStore::NowEpochMs();
    Ambrose::Crypto::GetRandomBytes(key.Master);
    std::string text = Render(fresh, key.Id);
    std::string wrapped;
    std::string why;
    bool const sealed = Wrap(text, wrapped, why);
    WipeText(text);
    bool const written = sealed && AdminToken::WriteSecretFile(file, wrapped, why, AdminToken::SecretReaders::OwnerAndAdministrators);
    WipeText(wrapped);
    if (!written)
    {
        Ambrose::Crypto::SecureWipe(key.Master);
        error = fmt::format("the panel keyring {} could not be made: {}", path, why);
        return false;
    }
#ifndef _WIN32
    TightenFolder(file.parent_path(), notes, warnings);
#endif
    Derive(key);
    _keys = std::move(fresh);
    _active = 1;
    _created = true;
    notes.push_back(fmt::format("The panel made its keyring in {}, readable only by this account and the machine's administrators", path));
    return true;
}

void PanelKeyring::Wipe()
{
    for (Key& key : _keys)
    {
        Ambrose::Crypto::SecureWipe(key.Master);
        for (AES256GCM::Key& derived : key.Derived)
            Ambrose::Crypto::SecureWipe(derived);
    }
    _keys.clear();
    _active = 0;
}

void PanelKeyring::Close()
{
    std::unique_lock const lock(_mutex);
    Wipe();
}

bool PanelKeyring::IsOpen() const
{
    std::shared_lock const lock(_mutex);
    return !_keys.empty();
}

bool PanelKeyring::WasCreated() const
{
    std::shared_lock const lock(_mutex);
    return _created;
}

std::filesystem::path PanelKeyring::GetFile() const
{
    std::shared_lock const lock(_mutex);
    return _file;
}

int64 PanelKeyring::ActiveId() const
{
    std::shared_lock const lock(_mutex);
    return _active;
}

std::vector<int64> PanelKeyring::KeyIds() const
{
    std::shared_lock const lock(_mutex);
    std::vector<int64> ids;
    ids.reserve(_keys.size());
    for (Key const& key : _keys)
        ids.push_back(key.Id);
    return ids;
}

PanelKeyring::Key const* PanelKeyring::Find(int64 keyId) const
{
    auto const found = std::find_if(_keys.begin(), _keys.end(), [keyId](Key const& key) { return key.Id == keyId; });
    return found == _keys.end() ? nullptr : &*found;
}

std::optional<PanelSealed> PanelKeyring::Seal(PanelKeyPurpose purpose, std::span<uint8 const> plain, std::string_view associated) const
{
    std::shared_lock const lock(_mutex);
    Key const* const key = Find(_active);
    if (!key)
        return std::nullopt;
    PanelSealed sealed;
    sealed.KeyId = key->Id;
    sealed.Bytes = AES256GCM::Seal(key->Derived[static_cast<std::size_t>(purpose)], plain, Bytes(associated));
    return sealed;
}

std::optional<std::vector<uint8>> PanelKeyring::Unseal(PanelKeyPurpose purpose, int64 keyId, std::span<uint8 const> sealed, std::string_view associated) const
{
    std::shared_lock const lock(_mutex);
    Key const* const key = Find(keyId);
    if (!key)
        return std::nullopt;
    return AES256GCM::Open(key->Derived[static_cast<std::size_t>(purpose)], sealed, Bytes(associated));
}

std::optional<std::vector<uint8>> PanelKeyring::KeyedHash(PanelKeyPurpose purpose, int64 keyId, std::string_view data) const
{
    std::shared_lock const lock(_mutex);
    Key const* const key = Find(keyId);
    if (!key)
        return std::nullopt;
    return Hmac::Compute(CryptoHash::Algorithm::Sha256, key->Derived[static_cast<std::size_t>(purpose)], Bytes(data));
}
