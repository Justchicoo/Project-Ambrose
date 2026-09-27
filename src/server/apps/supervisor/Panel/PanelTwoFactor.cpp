/*
 * Project Ambrose by Imjustchico
 * Reads the requirement and its limits with each bounded value clamped and named, and keeps two-factor state in the store: a secret is sealed with associated data naming its operator, so a sealed row moved to another operator will not open; a code is accepted by one conditional statement that moves the last accepted step forward only past where it was and returns the row it changed, so a replay or a second request racing the first changes nothing, whatever else the shared connection runs meanwhile; turning on a new secret while another is active takes a current code or recovery code from the active one too, both codes judged against the last step accepted before either and the pair accepted by one statement that holds that step unchanged; a recovery code is found by its keyed hash under each key the keyring holds and spent by one statement that marks it used only while it is unused; and issuing codes replaces every earlier one, the plain codes living only in what is handed back.
 */

#include "PanelTwoFactor.h"
#include "Base32.h"
#include "Base64.h"
#include "ConfigMgr.h"
#include "CryptoRandom.h"
#include "PanelGrants.h"
#include "PanelKeyring.h"
#include "PanelPermissions.h"
#include "PanelStore.h"
#include "PanelUsers.h"
#include "RecoveryCode.h"
#include "SecureMemory.h"
#include "StringUtil.h"
#include "Totp.h"

#include <fmt/format.h>

#include <algorithm>
#include <utility>

namespace
{
    std::optional<bool> ReturnedRow(PanelStore::Statement& statement, std::string& error)
    {
        bool const returned = statement.Step(error);
        if (!error.empty() || (returned && !statement.Run(error)))
            return std::nullopt;
        return returned;
    }
}

std::string_view PanelTwoFactorSettings::NameOf(PanelTwoFactorPolicy policy) noexcept
{
    switch (policy)
    {
        case PanelTwoFactorPolicy::None: return "none";
        case PanelTwoFactorPolicy::Admins: return "admins";
        case PanelTwoFactorPolicy::Danger: return "danger";
        case PanelTwoFactorPolicy::Everyone: break;
    }
    return "everyone";
}

std::string_view PanelTwoFactorSettings::Describe(PanelTwoFactorPolicy policy) noexcept
{
    switch (policy)
    {
        case PanelTwoFactorPolicy::None: return "nobody";
        case PanelTwoFactorPolicy::Admins: return "owners and admins";
        case PanelTwoFactorPolicy::Danger: return "everyone holding a danger permission";
        case PanelTwoFactorPolicy::Everyone: break;
    }
    return "everyone";
}

std::optional<PanelTwoFactorPolicy> PanelTwoFactorSettings::ParsePolicy(std::string_view text)
{
    for (PanelTwoFactorPolicy const policy : { PanelTwoFactorPolicy::None, PanelTwoFactorPolicy::Admins, PanelTwoFactorPolicy::Danger, PanelTwoFactorPolicy::Everyone })
        if (Ambrose::EqualsIgnoreCase(Ambrose::Trim(text), NameOf(policy)))
            return policy;
    return std::nullopt;
}

std::optional<PanelTwoFactorSettings> PanelTwoFactorSettings::Load(ConfigMgr const& config, std::vector<std::string>& problems, std::string& error)
{
    PanelTwoFactorSettings settings;
    std::string const required(Ambrose::Trim(config.GetOption<std::string>("Panel.TwoFactorRequired", "none", true)));
    std::optional<PanelTwoFactorPolicy> const policy = ParsePolicy(required);
    if (!policy)
    {
        error = fmt::format("Panel.TwoFactorRequired is {}, which is not none, admins, danger or everyone, so the panel will not guess which operators it holds to "
            "two-factor sign-in", required.empty() ? std::string("empty") : required);
        return std::nullopt;
    }
    settings.Required = *policy;
    auto const bounded = [&config, &problems](std::string const& key, int64 fallback, int64 low, int64 high)
    {
        int64 const value = config.GetOption<int64>(key, fallback, true);
        int64 const kept = std::clamp(value, low, high);
        if (kept != value)
            problems.push_back(fmt::format("{} is {}, outside {} to {}; using {}", key, value, low, high, kept));
        return kept;
    };
    settings.Window = static_cast<uint32>(bounded("Panel.TwoFactorWindow", DefaultWindow, 0, MaxWindow));
    settings.ChallengeLifetime = std::chrono::minutes(bounded("Panel.TwoFactorChallengeMinutes", DefaultChallengeMinutes, 1, 30));
    settings.ChallengeAttempts = static_cast<uint32>(bounded("Panel.TwoFactorChallengeAttempts", DefaultChallengeAttempts, 1, 20));
    settings.FailureLimit = static_cast<uint32>(bounded("Panel.TwoFactorFailureLimit", DefaultFailureLimit, 1, 1000));
    settings.FailureWindow = std::chrono::minutes(bounded("Panel.TwoFactorFailureWindowMinutes", DefaultFailureWindowMinutes, 1, 1440));
    settings.StepUpWindow = std::chrono::minutes(bounded("Panel.StepUpMinutes", DefaultStepUpMinutes, 1, 120));
    return settings;
}

PanelTwoFactor::PanelTwoFactor(PanelStore& store, PanelKeyring& keyring)
    : _store(store), _keyring(keyring), _clock([] { return static_cast<int64>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()); })
{
}

void PanelTwoFactor::SetWindow(uint32 steps)
{
    std::lock_guard const lock(_mutex);
    _window = std::min(steps, PanelTwoFactorSettings::MaxWindow);
}

uint32 PanelTwoFactor::GetWindow() const
{
    std::lock_guard const lock(_mutex);
    return _window;
}

void PanelTwoFactor::SetClock(UnixClock clock)
{
    std::lock_guard const lock(_mutex);
    _clock = std::move(clock);
}

int64 PanelTwoFactor::NowSeconds() const
{
    UnixClock clock;
    {
        std::lock_guard const lock(_mutex);
        clock = _clock;
    }
    return clock();
}

std::string_view PanelTwoFactor::Explain(PanelSecondFactor result) noexcept
{
    switch (result)
    {
        case PanelSecondFactor::Accepted: return "accepted";
        case PanelSecondFactor::Wrong: return "wrong";
        case PanelSecondFactor::Replayed: return "replayed";
        case PanelSecondFactor::NotEnabled: return "not enabled";
        case PanelSecondFactor::NotPending: return "nothing set up";
        case PanelSecondFactor::StoreFailed: break;
    }
    return "store failed";
}

std::string PanelTwoFactor::AssociatedData(int64 userId)
{
    return fmt::format("panel totp secret for user {}", userId);
}

bool PanelTwoFactor::Covers(PanelTwoFactorPolicy policy, PanelUser const& user, std::vector<PanelGrant> const& grants)
{
    switch (policy)
    {
        case PanelTwoFactorPolicy::None:
            return false;
        case PanelTwoFactorPolicy::Admins:
            return user.Role == PanelRole::Owner || user.Role == PanelRole::Admin;
        case PanelTwoFactorPolicy::Danger:
        {
            for (std::string_view const key : PanelPermissions::KeysOf(user.Role))
                if (PanelPermission const* const held = PanelPermissions::Find(key); held && held->Danger)
                    return true;
            for (PanelGrant const& grant : grants)
                if (PanelPermission const* const held = PanelPermissions::Find(grant.Permission); held && held->Danger)
                    return true;
            return false;
        }
        case PanelTwoFactorPolicy::Everyone:
            break;
    }
    return true;
}

std::optional<std::vector<uint8>> PanelTwoFactor::OpenSecret(std::string const& sealed, int64 keyId, int64 userId) const
{
    std::optional<std::vector<uint8>> const bytes = Base64::Decode(sealed);
    if (!bytes)
        return std::nullopt;
    return _keyring.Unseal(PanelKeyPurpose::TwoFactorSecret, keyId, *bytes, AssociatedData(userId));
}

PanelSecondFactor PanelTwoFactor::CheckCode(std::vector<uint8> const& secret, std::string_view code, int64 lastStep, uint64& accepted) const
{
    uint64 const now = Totp::StepAt(NowSeconds());
    uint32 const window = GetWindow();
    uint64 const last = lastStep < 0 ? 0 : static_cast<uint64>(lastStep);
    if (std::optional<uint64> const step = Totp::Match(secret, code, now, window, last))
    {
        accepted = *step;
        return PanelSecondFactor::Accepted;
    }
    return Totp::Match(secret, code, now, window, 0) ? PanelSecondFactor::Replayed : PanelSecondFactor::Wrong;
}

PanelSecondFactor PanelTwoFactor::CheckSealed(std::string const& sealed, int64 keyId, int64 userId, std::string_view code, int64 lastStep, uint64& accepted, std::string& error) const
{
    std::optional<std::vector<uint8>> secret = OpenSecret(sealed, keyId, userId);
    if (!secret)
    {
        error = "a two-factor secret does not open with the keyring";
        return PanelSecondFactor::StoreFailed;
    }
    PanelSecondFactor const checked = CheckCode(*secret, code, lastStep, accepted);
    Ambrose::Crypto::SecureWipe(*secret);
    return checked;
}

PanelTwoFactorState PanelTwoFactor::State(int64 userId, std::string& error)
{
    PanelTwoFactorState state;
    std::optional<PanelStore::Statement> rows = _store.Prepare(
        "SELECT secret IS NOT NULL, pending_secret IS NOT NULL, COALESCE(enabled_epoch_ms, 0) FROM panel_two_factor WHERE user_id = ?", error);
    if (!rows)
        return state;
    rows->Bind(1, userId);
    if (rows->Step(error))
    {
        state.Enabled = rows->Int64(0) != 0;
        state.Pending = rows->Int64(1) != 0;
        state.EnabledEpochMs = rows->Int64(2);
    }
    if (!error.empty())
        return state;
    rows.reset();
    state.RecoveryCodesLeft = state.Enabled ? RecoveryCodesLeft(userId, error) : 0;
    return state;
}

std::optional<PanelTwoFactorSetup> PanelTwoFactor::Setup(int64 userId, std::string_view issuer, std::string_view account, bool replace, std::string& error)
{
    std::vector<uint8> secret;
    bool fresh = true;
    if (!replace)
    {
        std::optional<PanelStore::Statement> rows = _store.Prepare(
            "SELECT pending_secret, pending_key_id FROM panel_two_factor WHERE user_id = ? AND pending_secret IS NOT NULL", error);
        if (!rows)
            return std::nullopt;
        rows->Bind(1, userId);
        if (rows->Step(error))
        {
            if (std::optional<std::vector<uint8>> kept = OpenSecret(rows->Text(0), rows->Int64(1), userId))
            {
                secret = std::move(*kept);
                fresh = false;
            }
        }
        if (!error.empty())
            return std::nullopt;
    }

    if (fresh)
    {
        secret = Ambrose::Crypto::GetRandomBytes(Totp::SecretBytes);
        std::optional<PanelSealed> const sealed = _keyring.Seal(PanelKeyPurpose::TwoFactorSecret, secret, AssociatedData(userId));
        if (!sealed)
        {
            Ambrose::Crypto::SecureWipe(secret);
            error = "the keyring is not open, so no secret can be sealed";
            return std::nullopt;
        }
        std::optional<PanelStore::Statement> write = _store.Prepare(
            "INSERT INTO panel_two_factor (user_id, pending_secret, pending_key_id, pending_epoch_ms) VALUES (?, ?, ?, ?)"
            " ON CONFLICT(user_id) DO UPDATE SET pending_secret = excluded.pending_secret, pending_key_id = excluded.pending_key_id, pending_epoch_ms = excluded.pending_epoch_ms",
            error);
        if (!write)
        {
            Ambrose::Crypto::SecureWipe(secret);
            return std::nullopt;
        }
        write->Bind(1, userId);
        write->Bind(2, Base64::Encode(sealed->Bytes));
        write->Bind(3, sealed->KeyId);
        write->Bind(4, PanelStore::NowEpochMs());
        if (!write->Run(error))
        {
            Ambrose::Crypto::SecureWipe(secret);
            return std::nullopt;
        }
    }

    PanelTwoFactorSetup setup;
    setup.Secret = Base32::Encode(secret);
    setup.Uri = Totp::Uri(issuer, account, secret);
    setup.Fresh = fresh;
    Ambrose::Crypto::SecureWipe(secret);
    return setup;
}

PanelSecondFactor PanelTwoFactor::Activate(int64 userId, std::string_view code, PanelFactor const* current, std::string& error)
{
    std::optional<PanelStore::Statement> rows = _store.Prepare(
        "SELECT pending_secret, pending_key_id, last_step, secret, COALESCE(secret_key_id, 0) FROM panel_two_factor WHERE user_id = ? AND pending_secret IS NOT NULL", error);
    if (!rows)
        return PanelSecondFactor::StoreFailed;
    rows->Bind(1, userId);
    if (!rows->Step(error))
        return error.empty() ? PanelSecondFactor::NotPending : PanelSecondFactor::StoreFailed;
    std::string const pending = rows->Text(0);
    int64 const pendingKeyId = rows->Int64(1);
    int64 const lastStep = rows->Int64(2);
    bool const active = !rows->IsNull(3);
    std::string const sealed = active ? rows->Text(3) : std::string();
    int64 const keyId = rows->Int64(4);
    rows.reset();

    uint64 proven = 0;
    if (active)
    {
        if (!current)
            return PanelSecondFactor::Wrong;
        PanelSecondFactor const held = current->Recovery ? UseRecoveryCode(userId, current->Typed, error) : CheckSealed(sealed, keyId, userId, current->Typed, lastStep, proven, error);
        if (held != PanelSecondFactor::Accepted)
            return held;
    }

    uint64 accepted = 0;
    PanelSecondFactor const checked = CheckSealed(pending, pendingKeyId, userId, code, lastStep, accepted, error);
    if (checked != PanelSecondFactor::Accepted)
        return checked;

    std::optional<PanelStore::Statement> update = _store.Prepare(
        "UPDATE panel_two_factor SET secret = pending_secret, secret_key_id = pending_key_id, enabled_epoch_ms = ?, last_step = ?,"
        " pending_secret = NULL, pending_key_id = NULL, pending_epoch_ms = NULL WHERE user_id = ? AND pending_secret = ? AND last_step = ? RETURNING user_id", error);
    if (!update)
        return PanelSecondFactor::StoreFailed;
    update->Bind(1, PanelStore::NowEpochMs());
    update->Bind(2, static_cast<int64>(std::max(accepted, proven)));
    update->Bind(3, userId);
    update->Bind(4, pending);
    update->Bind(5, lastStep);
    std::optional<bool> const claimed = ReturnedRow(*update, error);
    if (!claimed)
        return PanelSecondFactor::StoreFailed;
    return *claimed ? PanelSecondFactor::Accepted : PanelSecondFactor::Replayed;
}

PanelSecondFactor PanelTwoFactor::Verify(int64 userId, std::string_view code, std::string& error)
{
    std::optional<PanelStore::Statement> rows = _store.Prepare(
        "SELECT secret, secret_key_id, last_step FROM panel_two_factor WHERE user_id = ? AND secret IS NOT NULL", error);
    if (!rows)
        return PanelSecondFactor::StoreFailed;
    rows->Bind(1, userId);
    if (!rows->Step(error))
        return error.empty() ? PanelSecondFactor::NotEnabled : PanelSecondFactor::StoreFailed;
    std::string const sealed = rows->Text(0);
    int64 const keyId = rows->Int64(1);
    int64 const lastStep = rows->Int64(2);
    rows.reset();

    uint64 accepted = 0;
    PanelSecondFactor const checked = CheckSealed(sealed, keyId, userId, code, lastStep, accepted, error);
    if (checked != PanelSecondFactor::Accepted)
        return checked;

    std::optional<PanelStore::Statement> update = _store.Prepare(
        "UPDATE panel_two_factor SET last_step = ? WHERE user_id = ? AND secret = ? AND last_step < ? RETURNING user_id", error);
    if (!update)
        return PanelSecondFactor::StoreFailed;
    update->Bind(1, static_cast<int64>(accepted));
    update->Bind(2, userId);
    update->Bind(3, sealed);
    update->Bind(4, static_cast<int64>(accepted));
    std::optional<bool> const claimed = ReturnedRow(*update, error);
    if (!claimed)
        return PanelSecondFactor::StoreFailed;
    return *claimed ? PanelSecondFactor::Accepted : PanelSecondFactor::Replayed;
}

std::optional<std::string> PanelTwoFactor::RecoveryHash(int64 keyId, int64 userId, std::string_view normalized) const
{
    std::optional<std::vector<uint8>> const hash = _keyring.KeyedHash(PanelKeyPurpose::RecoveryCode, keyId, fmt::format("{}:{}", userId, normalized));
    if (!hash)
        return std::nullopt;
    return Base64::Encode(*hash, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
}

PanelSecondFactor PanelTwoFactor::UseRecoveryCode(int64 userId, std::string_view typed, std::string& error)
{
    std::optional<std::string> const normalized = RecoveryCode::Normalize(typed);
    if (!normalized)
        return PanelSecondFactor::Wrong;
    std::vector<std::pair<int64, std::string>> hashes;
    for (int64 const keyId : _keyring.KeyIds())
        if (std::optional<std::string> hash = RecoveryHash(keyId, userId, *normalized))
            hashes.emplace_back(keyId, std::move(*hash));

    for (auto const& [keyId, hash] : hashes)
    {
        std::optional<PanelStore::Statement> spend = _store.Prepare(
            "UPDATE panel_recovery_code SET used_epoch_ms = ? WHERE user_id = ? AND key_id = ? AND code_hash = ? AND used_epoch_ms IS NULL RETURNING id", error);
        if (!spend)
            return PanelSecondFactor::StoreFailed;
        spend->Bind(1, PanelStore::NowEpochMs());
        spend->Bind(2, userId);
        spend->Bind(3, keyId);
        spend->Bind(4, hash);
        std::optional<bool> const spent = ReturnedRow(*spend, error);
        if (!spent)
            return PanelSecondFactor::StoreFailed;
        if (*spent)
            return PanelSecondFactor::Accepted;
    }

    for (auto const& [keyId, hash] : hashes)
    {
        std::optional<PanelStore::Statement> used = _store.Prepare(
            "SELECT COUNT(*) FROM panel_recovery_code WHERE user_id = ? AND key_id = ? AND code_hash = ? AND used_epoch_ms IS NOT NULL", error);
        if (!used)
            return PanelSecondFactor::StoreFailed;
        used->Bind(1, userId);
        used->Bind(2, keyId);
        used->Bind(3, hash);
        if (used->Step(error) && used->Int64(0) > 0)
            return PanelSecondFactor::Replayed;
        if (!error.empty())
            return PanelSecondFactor::StoreFailed;
    }
    return PanelSecondFactor::Wrong;
}

std::optional<std::vector<std::string>> PanelTwoFactor::IssueRecoveryCodes(int64 userId, std::string& error)
{
    int64 const keyId = _keyring.ActiveId();
    if (keyId == 0)
    {
        error = "the keyring is not open, so no recovery code can be kept";
        return std::nullopt;
    }
    std::optional<PanelStore::Statement> clear = _store.Prepare("DELETE FROM panel_recovery_code WHERE user_id = ?", error);
    if (!clear)
        return std::nullopt;
    clear->Bind(1, userId);
    if (!clear->Run(error))
        return std::nullopt;
    clear.reset();

    std::vector<std::string> codes;
    codes.reserve(RecoveryCode::Count);
    int64 const now = PanelStore::NowEpochMs();
    while (codes.size() < RecoveryCode::Count)
    {
        std::string code = RecoveryCode::Generate();
        if (std::find(codes.begin(), codes.end(), code) != codes.end())
            continue;
        std::optional<std::string> const hash = RecoveryHash(keyId, userId, code);
        if (!hash)
        {
            error = "the keyring did not hash a recovery code";
            return std::nullopt;
        }
        std::optional<PanelStore::Statement> insert = _store.Prepare(
            "INSERT INTO panel_recovery_code (user_id, code_hash, key_id, created_epoch_ms) VALUES (?, ?, ?, ?)", error);
        if (!insert)
            return std::nullopt;
        insert->Bind(1, userId);
        insert->Bind(2, *hash);
        insert->Bind(3, keyId);
        insert->Bind(4, now);
        if (!insert->Run(error))
            return std::nullopt;
        codes.push_back(std::move(code));
    }
    return codes;
}

uint32 PanelTwoFactor::RecoveryCodesLeft(int64 userId, std::string& error)
{
    std::optional<PanelStore::Statement> rows = _store.Prepare("SELECT COUNT(*) FROM panel_recovery_code WHERE user_id = ? AND used_epoch_ms IS NULL", error);
    if (!rows)
        return 0;
    rows->Bind(1, userId);
    if (!rows->Step(error))
        return 0;
    return static_cast<uint32>(rows->Int64(0));
}

bool PanelTwoFactor::Disable(int64 userId, std::string& error)
{
    for (std::string_view const sql : { std::string_view("DELETE FROM panel_recovery_code WHERE user_id = ?"), std::string_view("DELETE FROM panel_two_factor WHERE user_id = ?") })
    {
        std::optional<PanelStore::Statement> remove = _store.Prepare(sql, error);
        if (!remove)
            return false;
        remove->Bind(1, userId);
        if (!remove->Run(error))
            return false;
    }
    return true;
}
