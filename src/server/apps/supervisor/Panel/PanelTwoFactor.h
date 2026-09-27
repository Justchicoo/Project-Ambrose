/*
 * Project Ambrose by Imjustchico
 * Two-factor sign-in for panel operators: the requirement an owner sets, from nobody through the owners and admins and everyone holding a danger permission to everyone, with the window, challenge, throttle and step-up limits beside it, read from the Panel options and refused by name when the requirement is not one the panel knows; and each operator's TOTP secret, sealed under the keyring and bound to that operator, set up apart from the active one until a code from it turns it on, which while another secret is active also takes a current code or recovery code from that one, checked so no time step at or below the last one accepted is ever accepted again, and their ten recovery codes, kept only as keyed hashes found by lookup and each spent by the one statement that marks it used.
 */

#ifndef AMBROSE_PANELTWOFACTOR_H
#define AMBROSE_PANELTWOFACTOR_H

#include "Types.h"

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ConfigMgr;
class PanelKeyring;
class PanelStore;
struct PanelGrant;
struct PanelUser;

enum class PanelTwoFactorPolicy : uint8
{
    None,
    Admins,
    Danger,
    Everyone
};

struct PanelTwoFactorSettings
{
    static constexpr uint32 DefaultWindow = 1;
    static constexpr uint32 MaxWindow = 3;
    static constexpr int64 DefaultChallengeMinutes = 5;
    static constexpr uint32 DefaultChallengeAttempts = 5;
    static constexpr uint32 DefaultFailureLimit = 10;
    static constexpr int64 DefaultFailureWindowMinutes = 15;
    static constexpr int64 DefaultStepUpMinutes = 5;

    PanelTwoFactorPolicy Required = PanelTwoFactorPolicy::None;
    uint32 Window = DefaultWindow;
    std::chrono::minutes ChallengeLifetime{ DefaultChallengeMinutes };
    uint32 ChallengeAttempts = DefaultChallengeAttempts;
    uint32 FailureLimit = DefaultFailureLimit;
    std::chrono::minutes FailureWindow{ DefaultFailureWindowMinutes };
    std::chrono::minutes StepUpWindow{ DefaultStepUpMinutes };

    static std::optional<PanelTwoFactorSettings> Load(ConfigMgr const& config, std::vector<std::string>& problems, std::string& error);
    static std::string_view NameOf(PanelTwoFactorPolicy policy) noexcept;
    static std::string_view Describe(PanelTwoFactorPolicy policy) noexcept;
    static std::optional<PanelTwoFactorPolicy> ParsePolicy(std::string_view text);
};

struct PanelTwoFactorState
{
    bool Enabled = false;
    bool Pending = false;
    int64 EnabledEpochMs = 0;
    uint32 RecoveryCodesLeft = 0;
};

struct PanelTwoFactorSetup
{
    std::string Secret = {};
    std::string Uri = {};
    bool Fresh = false;
};

enum class PanelSecondFactor : uint8
{
    Accepted,
    Wrong,
    Replayed,
    NotEnabled,
    NotPending,
    StoreFailed
};

struct PanelFactor
{
    bool Recovery = false;
    std::string Typed = {};
};

class PanelTwoFactor
{
public:
    using UnixClock = std::function<int64()>;

    PanelTwoFactor(PanelStore& store, PanelKeyring& keyring);

    PanelTwoFactor(PanelTwoFactor const&) = delete;
    PanelTwoFactor& operator=(PanelTwoFactor const&) = delete;

    void SetWindow(uint32 steps);
    uint32 GetWindow() const;
    void SetClock(UnixClock clock);
    int64 NowSeconds() const;

    static std::string_view Explain(PanelSecondFactor result) noexcept;
    static std::string AssociatedData(int64 userId);
    static bool Covers(PanelTwoFactorPolicy policy, PanelUser const& user, std::vector<PanelGrant> const& grants);

    PanelTwoFactorState State(int64 userId, std::string& error);
    std::optional<PanelTwoFactorSetup> Setup(int64 userId, std::string_view issuer, std::string_view account, bool replace, std::string& error);
    PanelSecondFactor Activate(int64 userId, std::string_view code, PanelFactor const* current, std::string& error);
    PanelSecondFactor Verify(int64 userId, std::string_view code, std::string& error);
    PanelSecondFactor UseRecoveryCode(int64 userId, std::string_view typed, std::string& error);
    std::optional<std::vector<std::string>> IssueRecoveryCodes(int64 userId, std::string& error);
    uint32 RecoveryCodesLeft(int64 userId, std::string& error);
    bool Disable(int64 userId, std::string& error);
    std::optional<std::string> RecoveryHash(int64 keyId, int64 userId, std::string_view normalized) const;

private:
    std::optional<std::vector<uint8>> OpenSecret(std::string const& sealed, int64 keyId, int64 userId) const;
    PanelSecondFactor CheckCode(std::vector<uint8> const& secret, std::string_view code, int64 lastStep, uint64& accepted) const;
    PanelSecondFactor CheckSealed(std::string const& sealed, int64 keyId, int64 userId, std::string_view code, int64 lastStep, uint64& accepted, std::string& error) const;

    PanelStore& _store;
    PanelKeyring& _keyring;
    mutable std::mutex _mutex;
    uint32 _window = PanelTwoFactorSettings::DefaultWindow;
    UnixClock _clock;
};

#endif
