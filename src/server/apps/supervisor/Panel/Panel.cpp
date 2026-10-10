/*
 * Project Ambrose by Imjustchico
 * Reads the Panel options into a listener of the same shape as an app's admin API, opens the store and then the keyring before the listener so nothing serves without somewhere to write or the keys its secrets need, lends that store under its own lock to the supervisor's live settings and to an owner's protected file patterns, which are saved in the same transaction as the audit row naming who changed them, names the certificate and key in Panel option names when the bind rule refuses them, and starts, reloads and stops the listener beside the supervisor's own; a reload that would leave the bind unsafe or the certificate unservable, or name a two-factor requirement the panel does not know, is refused and the old listener and requirement keep serving. Signing in also says which role the operator holds and every permission that role allows, so the pages a person cannot use are never drawn for them and the panel never has to ask again what somebody is allowed to do. An operator with two-factor sign-in is given no session for a password alone: the password earns a challenge held in memory under the hash of a short-lived cookie, which dies after a few attempts or minutes, and only a code or a recovery code from that operator turns it into a session, the one-time password link included; an operator who already has two-factor sign-in moves to another authenticator only with a current code or recovery code from the one in use as well as the password and a code from the new one, so a session and a password alone cannot swap the factor out. A code or recovery code is checked and spent under the same lock every recorded change holds, so it never lands inside another request's transaction and is never undone with it. Every authenticated route and socket is held to the two-factor requirement except the routes that turn it on, and a danger permission, a secret reveal or a restricted change asks for a check of who the caller is within the last few minutes, records what that check authorized, and changes nothing while it is missing. A route that asks for a permission the catalog does not hold is left out and named in a warning as the panel starts, so a misnamed key costs its page loudly rather than silently. A settings change, reset, batch or reload an app answered through the relay is recorded, a dry run not being a change, with who asked, from where, why and how it ended, refused ones too, and a read that showed a secret is recorded with the keys it showed, never a value; no code, secret or password ever reaches an audit row or a log line. The event socket and its ticket route are registered with the rest, its streams and its sweeper start once the listener is up and stop before it closes. Every single-use link, the owner claim printed at each start while there is no operator, a password link, a local link and a pairing link, is issued through one path that audits it with the store change in one transaction and never writes its token anywhere but the answer, makes the owner with a password nobody is told when a desktop link finds the panel empty, refuses a local link a loopback peer cannot reach and a pairing a plain listener would carry unencrypted, and pins a pairing to the certificate the listener serves now; a link is traded once, counted per address when wrong or spent, burned when used from where it does not belong, and opens a session only through the second factor its operator has, and no Argon2id hash ever runs inside an open transaction. The mail test sends only to the signed-in operator's own address through the saved mail settings and is audited, answering the SMTP server's own refusal when it fails, and once a name has failed to sign in repeatedly with the captcha on, sign-in needs a captcha answer the provider verifies, refused with a clear 503 when the provider cannot be reached rather than let through.
 */

#include "Panel.h"
#include "AdminClient.h"
#include "AdminConfigView.h"
#include "PanelCaptcha.h"
#include "PanelErrorReport.h"
#include "PanelMail.h"
#include "ConfigMgr.h"
#include "PanelSettingStore.h"
#include "CryptoRandom.h"
#include "Environment.h"
#include "Base64.h"
#include "IpAddress.h"
#include "Log.h"
#include "RecoveryCode.h"
#include "SecureMemory.h"
#include "SHA256.h"
#include "LogRedaction.h"
#include "SourceFolder.h"
#include "StringUtil.h"
#include "Totp.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <mutex>
#include <utility>

namespace
{
    constexpr char const* PanelCategory = "server.panel";
    constexpr std::size_t MaxReasonBytes = 200;

    std::filesystem::path Configured(ConfigMgr const& config, std::string const& key, std::filesystem::path const& fallback)
    {
        std::filesystem::path const value = ConfigMgr::PathFromUtf8(Ambrose::Trim(config.GetOption<std::string>(key, "", true)));
        return value.empty() ? fallback : value.lexically_normal();
    }

    std::string ChallengeKey(std::string_view secret)
    {
        return Base64::Encode(SHA256::GetDigestOf(secret), Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
    }

    nlohmann::json ParseBody(AdminRequest const& request)
    {
        return request.Body.empty() ? nlohmann::json::object() : nlohmann::json::parse(request.Body, nullptr, false);
    }

    bool HasText(nlohmann::json const& body, char const* key)
    {
        return body.is_object() && body.contains(key) && body[key].is_string() && !body[key].get_ref<std::string const&>().empty();
    }

    std::string TextOf(nlohmann::json const& body, char const* key)
    {
        return HasText(body, key) ? body[key].get<std::string>() : std::string();
    }

    void WipeText(std::string& text) noexcept
    {
        Ambrose::Crypto::SecureWipe(std::span<uint8>(reinterpret_cast<uint8*>(text.data()), text.size()));
        text.clear();
    }

    nlohmann::json MethodsFor(bool twoFactor)
    {
        return twoFactor ? nlohmann::json::array({ "totp", "recovery_code" }) : nlohmann::json::array({ "password" });
    }

    AdminResponse CheckRefused()
    {
        return AdminResponse::Problem(403, "check_refused", "That password and code do not confirm it is you");
    }

    constexpr std::string_view PlainRemoteMessage =
        "The panel serves plain HTTP beyond this machine, so a pairing would send its token and the session it opens unencrypted; set Panel.CertificateFile and "
        "Panel.PrivateKeyFile, which supervisor --panel-self-signed can write, and pair again";
    constexpr std::string_view PlainLocalMessage =
        "The panel serves plain HTTP on this machine only, so a pairing can name only a loopback address; to pair from another machine set Panel.CertificateFile and "
        "Panel.PrivateKeyFile, which supervisor --panel-self-signed can write";

    std::string UrlHost(std::string_view host)
    {
        return host.find(':') == std::string_view::npos ? std::string(host) : fmt::format("[{}]", host);
    }

    bool IsLoopbackHost(std::string_view host)
    {
        if (Ambrose::EqualsIgnoreCase(host, "localhost"))
            return true;
        std::optional<asio::ip::address> const address = Ambrose::Asio::MakeAddress(host);
        return address && Ambrose::Asio::IsLoopback(*address);
    }

    bool IsHostName(std::string_view host)
    {
        if (host.empty() || host.size() > 253 || host.front() == '.' || host.back() == '.' || host.front() == '-' || host.back() == '-')
            return false;
        return std::all_of(host.begin(), host.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-'; });
    }

    bool ReadAddress(std::string_view text, uint16 defaultPort, std::string& host, uint16& port, std::string& problem)
    {
        text = Ambrose::Trim(text);
        if (text.empty())
        {
            problem = "Give the address the program reaches the panel at, as host or host:port";
            return false;
        }
        std::string_view name = text;
        std::string_view portText;
        bool bracketed = false;
        if (text.front() == '[')
        {
            std::size_t const close = text.find(']');
            if (close == std::string_view::npos)
            {
                problem = "An IPv6 address in brackets needs its closing bracket";
                return false;
            }
            name = text.substr(1, close - 1);
            std::string_view const rest = text.substr(close + 1);
            if (!rest.empty() && rest.front() != ':')
            {
                problem = "Write the port after the bracket and a colon, as [::1]:12080";
                return false;
            }
            portText = rest.empty() ? std::string_view() : rest.substr(1);
            if (!rest.empty() && portText.empty())
            {
                problem = "A colon after the address takes a port";
                return false;
            }
            bracketed = true;
        }
        else if (std::size_t const colon = text.find(':'); colon != std::string_view::npos && text.find(':', colon + 1) == std::string_view::npos)
        {
            name = text.substr(0, colon);
            portText = text.substr(colon + 1);
            if (portText.empty())
            {
                problem = "A colon after the host takes a port";
                return false;
            }
        }
        std::optional<asio::ip::address> const address = Ambrose::Asio::MakeAddress(name);
        bool const valid = bracketed ? (address && address->is_v6()) : (address.has_value() || IsHostName(name));
        if (!valid || (address && Ambrose::Asio::IsUnspecified(*address)))
        {
            problem = fmt::format("{} is not an address or a host name; write IPv6 in brackets, as [::1]:12080", Ambrose::ForLog(text));
            return false;
        }
        port = defaultPort;
        if (!portText.empty())
        {
            std::optional<uint16> const parsed = Ambrose::StringTo<uint16>(portText);
            if (!parsed || *parsed == 0)
            {
                problem = fmt::format("{} is not a port from 1 to 65535", Ambrose::ForLog(portText));
                return false;
            }
            port = *parsed;
        }
        host = address ? address->to_string() : Ambrose::ToLower(name);
        return true;
    }
}

Panel::Panel(Log& log, std::filesystem::path dataFolder, std::filesystem::path configFolder)
    : _log(log), _dataFolder(std::move(dataFolder)), _store(), _settings(_store), _users(_store), _sessions(_store), _errors(_store), _grants(_store), _keyring(),
      _twoFactor(_store, _keyring), _fileRules(_store), _links(_store), _listener(log, "panel", _dataFolder, std::move(configFolder))
{
    _listener.Routes().SetThrottle([this](AdminRequest const& request, uint32 cost) { return Throttle(request, cost); });
    _listener.SetSessionSource(&_sessions);
    _authorization = std::make_unique<PanelAuthorization>(_grants,
        [this](AdminRequest const& request) { return UserOf(request); },
        [this](AdminRequest const& request, std::string_view permission, std::string_view app, PermissionVerdict verdict)
        {
            AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "{} {} {}{} (request {})", request.Principal.empty() ? std::string("somebody") : request.Principal,
                verdict == PermissionVerdict::Allowed ? "used" : "was refused", permission, app.empty() ? std::string() : " on " + std::string(app), request.Id);
            if (verdict == PermissionVerdict::Allowed)
                return;

            AuditScope scope(request, app.empty() ? "panel:permission.refused" : "app:permission.refused");
            AuditEvent& event = scope.Event();
            event.Result = AuditResult::Refused;
            event.Reason = verdict == PermissionVerdict::OutOfScope
                ? fmt::format("the caller holds no grant in the {} app scope", app)
                : fmt::format("the caller does not hold {}", permission);
            event.Properties = nlohmann::json{
                { "permission", permission },
                { "verdict", verdict == PermissionVerdict::OutOfScope ? "out_of_scope" : "forbidden" }
            }.dump();
            std::optional<PanelUser> const user = UserOf(request);
            if (user)
            {
                event.Actor = AuditActor::User;
                event.ActorId = std::to_string(user->Id);
                event.ActorName = user->Username;
                event.On("panel_user", std::to_string(user->Id), user->Username);
            }
            if (!app.empty())
                event.On("app", std::string(app), std::string(app));
            std::string error;
            if (!Record(event, {}, error))
                AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A refused permission {} could not be recorded: {}", permission, error);
        });
    _listener.Routes().SetPermissionCheck([this](AdminRequest const& request, std::string_view permission)
    {
        return _authorization->Decide(request, permission);
    });
    _listener.Routes().SetPermissionKnown([](std::string_view permission) { return PanelPermissions::Holds(permission); });
    _listener.Routes().SetAdmission([this](AdminRequest const& request) { return Admit(request); });
    _listener.Routes().SetStepUp([this](AdminRequest const& request, std::string_view permission, StepUpWhen when) { return StepUpCheck(request, permission, when); });
    _secondFactor.SetLimits(PanelTwoFactorSettings::DefaultFailureLimit, std::chrono::minutes(PanelTwoFactorSettings::DefaultFailureWindowMinutes));
    _settingStore = std::make_shared<PanelSettingStore>(_store, _storeMutex);
    RegisterSignIn();
    _eventSocket = std::make_unique<PanelEventSocket>(_log, _events, _tickets, _sessions, _users, _grants, _listener.Routes());
    _listener.AddSocket(_eventSocket->MakeRoute());
    _listener.Routes().AddOpenCosting("POST", std::string(PanelEventSocket::TicketPath), PanelEventSocket::TicketCost, [this](AdminRequest const& request)
    {
        return _eventSocket->MintTicket(request);
    });
    RegisterTwoFactor();
    RegisterCommandHistory();
    RegisterMaintenance();
    RegisterOpsCalendar();
}

void Panel::SetAppSource(PanelEventSocket::AppSource source)
{
    _eventSocket->SetAppSource(std::move(source));
}

std::shared_ptr<SettingStore> Panel::LiveSettingStore()
{
    return _settingStore;
}

bool Panel::IsStoreOpen()
{
    std::lock_guard const lock(_storeMutex);
    return _store.IsOpen();
}

bool Panel::ReadFileRules(std::map<std::string, std::vector<std::string>, std::less<>>& rules, std::string& error)
{
    std::lock_guard const lock(_storeMutex);
    rules.clear();
    if (!_store.IsOpen())
        return true;
    return _fileRules.Read(rules, error);
}

bool Panel::SaveFileRules(AdminRequest const& request, AuditEvent const& event, std::string const& root, std::vector<std::string> const& patterns, std::string& error)
{
    std::optional<PanelUser> const user = UserOf(request);
    std::optional<int64> const by = user ? std::optional<int64>(user->Id) : std::nullopt;
    return Record(event, [this, &root, &patterns, by](std::string& failure) { return _fileRules.Replace(root, patterns, by, failure); }, error);
}

ListenerSettings Panel::LoadSettings(ConfigMgr const& config, std::vector<std::string>* problems)
{
    ListenerSettings settings = ListenerSettings::Load(config, "Panel", DefaultPort, problems);
    settings.Label = "the panel";
    settings.LogCategory = PanelCategory;
    settings.Secrets = "the token, sign-in passwords, two-factor codes and session cookies";
    return settings;
}

std::filesystem::path Panel::StoreFile(ConfigMgr const& config, std::filesystem::path const& dataFolder)
{
    std::filesystem::path const home = (dataFolder.empty() ? config.GetFilename().parent_path() : dataFolder) / "panel";
    return Configured(config, "Panel.StoreFile", home / "panel.sqlite3");
}

std::filesystem::path Panel::KeyringFile(ConfigMgr const& config, std::filesystem::path const& dataFolder)
{
    std::filesystem::path const home = dataFolder.empty() ? config.GetFilename().parent_path() : dataFolder;
    return Configured(config, "Panel.KeyringFile", home / "keyring");
}

bool Panel::OpenStore(ConfigMgr const& config, std::string& error)
{
    std::lock_guard const lock(_storeMutex);
    if (_store.IsOpen())
        return true;
    std::vector<std::string> warnings;
    std::filesystem::path const file = StoreFile(config, _dataFolder);
    if (!_store.Open(file, Ambrose::FindSourceFolder(), warnings, error))
        return false;
    if (!PanelAudit::EnsureChain(_store, error))
    {
        _store.Close();
        return false;
    }
    AuditChainVerification verification;
    std::string verifyError;
    if (!PanelAudit::VerifyChain(_store, verification, verifyError))
        AMBROSE_LOG(_log, LogLevel::Warn, PanelCategory, "The panel's audit chain could not be verified: {}", verifyError);
    else if (!verification.Valid)
        AMBROSE_LOG(_log, LogLevel::Warn, PanelCategory, "The panel's audit chain does not verify from row {} to row {}: {}",
            verification.FirstInvalidId, verification.LastRowId, verification.Problem);
    for (std::string const& warning : warnings)
        AMBROSE_LOG(_log, LogLevel::Warn, PanelCategory, "{}", warning);
    for (std::string const& applied : _store.GetApplied())
        AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "The panel store applied {}", applied);
    AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "The panel store is open in {}", ConfigMgr::PathToUtf8(file));
    return true;
}

bool Panel::OpenKeyring(ConfigMgr const& config, std::string& error)
{
    if (_keyring.IsOpen())
        return true;
    std::vector<std::string> notes;
    std::vector<std::string> warnings;
    std::filesystem::path const file = KeyringFile(config, _dataFolder);
    if (!_keyring.Load(file, notes, warnings, error))
        return false;
    for (std::string const& note : notes)
        AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "{}", note);
    for (std::string const& warning : warnings)
        AMBROSE_LOG(_log, LogLevel::Warn, PanelCategory, "{}", warning);
    AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "The panel keyring is open in {} with {} key(s)", ConfigMgr::PathToUtf8(file), _keyring.KeyIds().size());
    return true;
}

std::optional<PanelTwoFactorSettings> Panel::LoadTwoFactorSettings(ConfigMgr const& config, std::string& error)
{
    std::vector<std::string> problems;
    std::optional<PanelTwoFactorSettings> loaded = PanelTwoFactorSettings::Load(config, problems, error);
    for (std::string const& problem : problems)
        AMBROSE_LOG(_log, LogLevel::Warn, PanelCategory, "{}", problem);
    return loaded;
}

void Panel::ApplyTwoFactorSettings(ConfigMgr const& config, PanelTwoFactorSettings const& loaded)
{
    PanelTwoFactorPolicy previous = PanelTwoFactorPolicy::None;
    {
        std::lock_guard const lock(_twoFactorMutex);
        previous = _twoFactorSettings.Required;
        _twoFactorSettings = loaded;
    }
    _twoFactor.SetWindow(loaded.Window);
    _secondFactor.SetLimits(loaded.FailureLimit, loaded.FailureWindow);
    std::optional<ConfigEntry> const entry = config.Resolve("Panel.TwoFactorRequired");
    _settings.SetOwned("Panel.TwoFactorRequired", std::string(PanelTwoFactorSettings::NameOf(loaded.Required)),
        entry ? std::string(AdminConfigView::LayerName(entry->Kind)) : std::string("default"));
    if (previous != loaded.Required)
        AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "The panel requires two-factor sign-in of {}", PanelTwoFactorSettings::Describe(loaded.Required));
}

PanelTwoFactorSettings Panel::TwoFactorSettings() const
{
    std::lock_guard const lock(_twoFactorMutex);
    return _twoFactorSettings;
}

void Panel::SetDashboard(EmbeddedPage const* page)
{
    _listener.SetEmbeddedDashboard(page);
}

bool Panel::Start(ConfigMgr const& config, std::string& error)
{
    std::vector<std::string> problems;
    ListenerSettings const settings = LoadSettings(config, &problems);
    for (std::string const& problem : problems)
        AMBROSE_LOG(_log, LogLevel::Warn, PanelCategory, "{}", problem);
    if (!settings.Enable)
    {
        AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "Panel.Enable = 0, so the panel serves nothing of its own; the supervisor's admin API still serves it");
        return true;
    }
    std::optional<PanelTwoFactorSettings> const twoFactor = LoadTwoFactorSettings(config, error);
    if (!twoFactor)
        return false;
    ApplyTwoFactorSettings(config, *twoFactor);
    if (!OpenStore(config, error))
        return false;
    if (!OpenKeyring(config, error))
        return false;
    _rateLimit.SetLimits(settings.RateLimitBurst, settings.RateLimitPerSecond);
    _secure = settings.HasTls();
    _sessionIdle = std::chrono::minutes(settings.SessionIdleMinutes);
    _sessionLifetime = std::chrono::hours(settings.SessionLifetimeHours);
    _sessions.SetLifetimes(_sessionIdle, _sessionLifetime);
    if (std::vector<std::string> const undeclared = _listener.Routes().RouteProblems(); !undeclared.empty())
    {
        error = "the panel will not serve routes that do not say what they need:";
        for (std::string const& undeclaredRoute : undeclared)
            error += " " + undeclaredRoute + ";";
        return false;
    }
    for (std::string const& refused : _listener.Routes().RefusedRoutes())
        AMBROSE_LOG(_log, LogLevel::Warn, PanelCategory, "The panel leaves out {}", refused);
    std::string const auditCollectorUrl = config.GetOption<std::string>("Panel.AuditCollectorUrl", "", true);
    std::string const auditCollectorToken = config.GetOption<std::string>("Panel.AuditCollectorToken", "", true);
    if (!_auditForwarder.Start(_store, _storeMutex, auditCollectorUrl, auditCollectorToken,
        [this](std::string_view problem)
        {
            AMBROSE_LOG(_log, LogLevel::Warn, PanelCategory, "Audit collector forwarding will retry: {}", problem);
        }, error))
        return false;
    _auditForwarding = !auditCollectorUrl.empty();
    if (!_listener.Start(settings, error))
    {
        _auditForwarder.Stop();
        _auditForwarding = false;
        return false;
    }
    _events.Start();
    _eventSocket->Start();
    OfferTheOwnerLink();
    StartGathering();
    return true;
}

bool Panel::Reload(ConfigMgr const& config)
{
    std::vector<std::string> problems;
    ListenerSettings const settings = LoadSettings(config, &problems);
    for (std::string const& problem : problems)
        AMBROSE_LOG(_log, LogLevel::Warn, PanelCategory, "{}", problem);
    std::optional<PanelTwoFactorSettings> twoFactor;
    if (settings.Enable)
    {
        std::string refused;
        twoFactor = LoadTwoFactorSettings(config, refused);
        if (!twoFactor)
        {
            AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "{}; the panel keeps requiring two-factor sign-in as it did ({}) and the reload changes nothing",
                refused, PanelTwoFactorSettings::NameOf(TwoFactorSettings().Required));
            return false;
        }
    }
    if (settings.Enable && !_store.IsOpen())
    {
        std::string error;
        if (!OpenStore(config, error))
        {
            AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "The panel store could not be opened, so the panel stays as it is: {}", error);
            return false;
        }
    }
    if (settings.Enable && !_keyring.IsOpen())
    {
        std::string error;
        if (!OpenKeyring(config, error))
        {
            AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "The panel keyring could not be opened, so the panel stays as it is: {}", error);
            return false;
        }
    }
    _rateLimit.SetLimits(settings.RateLimitBurst, settings.RateLimitPerSecond);
    if (!_listener.Reload(settings))
        return false;
    if (twoFactor)
        ApplyTwoFactorSettings(config, *twoFactor);
    _secure = settings.Enable && settings.HasTls();
    return true;
}

void Panel::Stop()
{
    StopGathering();
    _eventSocket->Stop();
    _listener.Stop();
    _auditForwarder.Stop();
    _auditForwarding = false;
    _events.Stop();
    _tickets.Clear();
    _rateLimit.Clear();
    {
        std::lock_guard const lock(_challengeMutex);
        _challenges.clear();
    }
    _keyring.Close();
    std::lock_guard const lock(_storeMutex);
    _store.Close();
    _secure = false;
}

void Panel::SetErrorSource(std::function<std::vector<std::pair<std::string, std::string>>()> source)
{
    std::lock_guard const lock(_gatherMutex);
    _errorSource = std::move(source);
}

void Panel::StartGathering()
{
    {
        std::lock_guard const lock(_gatherMutex);
        if (_gathering || !_errorSource)
            return;
        _gathering = true;
    }
    _gatherThread = std::thread([this]
    {
        std::unique_lock lock(_gatherMutex);
        while (_gathering)
        {
            _gatherWake.wait_for(lock, GatherInterval, [this] { return !_gathering; });
            if (!_gathering)
                return;
            lock.unlock();
            GatherErrorsOnce();
            lock.lock();
        }
    });
}

void Panel::StopGathering()
{
    {
        std::lock_guard const lock(_gatherMutex);
        if (!_gathering)
            return;
        _gathering = false;
    }
    _gatherWake.notify_all();
    if (_gatherThread.joinable())
        _gatherThread.join();
}

std::size_t Panel::GatherErrorsOnce()
{
    std::function<std::vector<std::pair<std::string, std::string>>()> source;
    {
        std::lock_guard const lock(_gatherMutex);
        source = _errorSource;
    }
    if (!source)
        return 0;

    std::size_t recorded = 0;
    for (auto const& [app, body] : source())
    {
        nlohmann::json const answer = nlohmann::json::parse(body, nullptr, false);
        if (!answer.is_object() || !answer.contains("groups") || !answer["groups"].is_array())
        {
            AMBROSE_LOG(_log, LogLevel::Warn, PanelCategory, "{} answered its errors in a shape this panel does not read, so none were kept", app);
            continue;
        }
        std::vector<PanelErrorGroup> groups;
        for (nlohmann::json const& entry : answer["groups"])
        {
            if (!entry.is_object())
                continue;
            PanelErrorGroup group;
            group.Category = entry.value("category", std::string());
            group.File = entry.value("file", std::string());
            group.Line = entry.value("line", 0u);
            group.Function = entry.value("function", std::string());
            group.Template = entry.value("template", std::string());
            group.Level = entry.value("level", std::string("error"));
            group.Revision = entry.value("revision", std::string());
            group.Count = entry.value("count", uint64{ 0 });
            group.FirstEpochMs = entry.value("first_epoch_ms", int64{ 0 });
            group.LastEpochMs = entry.value("last_epoch_ms", int64{ 0 });
            group.LastMessage = entry.value("last_message", std::string());
            nlohmann::json context = entry.value("context_before", nlohmann::json::array());
            if (context.is_array())
                group.ContextBeforeJson = context.dump();
            if (group.File.empty() || group.Template.empty())
                continue;
            groups.push_back(std::move(group));
        }
        if (groups.empty())
            continue;

        std::string error;
        std::lock_guard const lock(_storeMutex);
        if (!_store.IsOpen())
            return recorded;
        if (!_errors.Record(app, groups, error))
        {
            AMBROSE_LOG(_log, LogLevel::Warn, PanelCategory, "The errors {} reported could not be kept: {}", app, error);
            continue;
        }
        recorded += groups.size();
    }
    return recorded;
}

bool Panel::Record(AuditEvent const& event, std::function<bool(std::string& error)> const& change, std::string& error)
{
    std::lock_guard const lock(_storeMutex);
    if (!_store.IsOpen())
    {
        error = "the panel store is not open, so nothing can be recorded and nothing is changed";
        return false;
    }
    bool const recorded = PanelAudit::Record(_store, event, change, error, _auditForwarding);
    if (recorded && _auditForwarding)
        _auditForwarder.Wake();
    return recorded;
}

bool Panel::VerifyAuditChain(AuditChainVerification& verification, std::string& error)
{
    std::lock_guard const lock(_storeMutex);
    if (!_store.IsOpen())
    {
        error = "the panel store is not open";
        return false;
    }
    if (!PanelAudit::VerifyChain(_store, verification, error))
        return false;
    verification.PendingEvents = PanelAudit::PendingCount(_store, error);
    return verification.PendingEvents >= 0;
}

bool Panel::Record(AuditEvent& event, std::function<bool(AuditEvent& event, std::string& error)> const& change, std::string& error)
{
    std::lock_guard const lock(_storeMutex);
    if (!_store.IsOpen())
    {
        error = "the panel store is not open, so nothing can be recorded and nothing is changed";
        return false;
    }
    bool const recorded = PanelAudit::Record(_store, event, change, error, _auditForwarding);
    if (recorded && _auditForwarding)
        _auditForwarder.Wake();
    return recorded;
}

AdminResponse Panel::AuditRequest(AdminRequest const& request, std::string_view app, std::string_view action, std::function<AdminResponse()> operation)
{
    AuditScope scope(request, std::string(action));
    AuditEvent& event = scope.Event();
    event.Properties = nlohmann::json{
        { "method", request.Method },
        { "path", request.Path }
    }.dump();
    std::optional<PanelUser> const user = UserOf(request);
    if (user)
    {
        event.Actor = AuditActor::User;
        event.ActorId = std::to_string(user->Id);
        event.ActorName = user->Username;
        event.On("panel_user", std::to_string(user->Id), user->Username);
    }
    if (!app.empty())
        event.On("app", std::string(app), std::string(app));
    std::optional<AdminResponse> answer;
    std::string error;
    bool operationStarted = false;
    if (!Record(event, [&](AuditEvent& recorded, std::string&)
    {
        operationStarted = true;
        answer = operation();
        if (answer->Status >= 500)
        {
            recorded.Result = AuditResult::Failed;
            recorded.Error = answer->Body;
        }
        else if (answer->Status >= 400)
        {
            recorded.Result = AuditResult::Refused;
            nlohmann::json const body = nlohmann::json::parse(answer->Body, nullptr, false);
            recorded.Reason = body.is_object() && body.contains("message") && body["message"].is_string()
                ? body["message"].get<std::string>() : fmt::format("the request was refused with HTTP {}", answer->Status);
        }
        if (request.Path.ends_with("/api/command") && answer->Status < 500)
        {
            nlohmann::json const body = nlohmann::json::parse(answer->Body, nullptr, false);
            if (body.is_object() && body.contains("command") && body["command"].is_string()
                && !StoreCommandHistoryWithinAudit(request, app, body["command"].get<std::string>(), error))
            {
                recorded.Result = AuditResult::Failed;
                recorded.Error = fmt::format("the command ran but its history could not be saved: {}", error);
            }
        }
        return true;
    }, error))
    {
        if (operationStarted)
            return AdminResponse::Problem(503, "audit_incomplete", fmt::format("The action may have run, but the panel could not finalize its audit record: {}", error));
        return AdminResponse::Problem(503, "audit_unavailable", fmt::format("The action was not run because the panel could not record it: {}", error));
    }
    return answer.value_or(AdminResponse::Problem(503, "audit_unavailable", "The action did not produce an answer"));
}

uint8 Panel::CommandLevel(AdminRequest const& request)
{
    return _authorization->CommandLevel(request);
}

std::string Panel::CommandActorName(AdminRequest const& request)
{
    std::optional<PanelUser> const user = UserOf(request);
    return user ? user->Username : std::string();
}

bool Panel::StoreCommandHistoryWithinAudit(AdminRequest const& request, std::string_view app, std::string_view command, std::string& error)
{
    std::optional<PanelUser> const user = UserOf(request);
    if (!user || app.empty() || command.empty())
        return true;
    std::optional<PanelStore::Statement> insert = _store.Prepare(
        "INSERT INTO panel_command_history (user_id, app, command, created_epoch_ms) VALUES (?, ?, ?, ?)", error);
    if (!insert)
        return false;
    insert->Bind(1, user->Id);
    insert->Bind(2, app);
    insert->Bind(3, command);
    insert->Bind(4, PanelStore::NowEpochMs());
    if (!insert->Run(error))
        return false;
    std::optional<PanelStore::Statement> trim = _store.Prepare(
        "DELETE FROM panel_command_history WHERE user_id = ? AND id NOT IN "
        "(SELECT id FROM panel_command_history WHERE user_id = ? ORDER BY id DESC LIMIT 500)", error);
    if (!trim)
        return false;
    trim->Bind(1, user->Id);
    trim->Bind(2, user->Id);
    return trim->Run(error);
}

AdminResponse Panel::CommandHistoryGet(AdminRequest const& request)
{
    constexpr std::string_view HistoryPrefix = "/api/panel/apps/";
    constexpr std::string_view Suffix = "/command-history";
    std::string_view path(request.Path);
    if (!path.starts_with(HistoryPrefix) || !path.ends_with(Suffix))
        return AdminResponse::Problem(404, "not_found", "That command history does not exist");
    path.remove_prefix(HistoryPrefix.size());
    path.remove_suffix(Suffix.size());
    if (path.empty() || path.find('/') != std::string_view::npos)
        return AdminResponse::Problem(404, "not_found", "That command history does not exist");
    std::optional<PanelUser> const user = UserOf(request);
    if (!user)
        return AdminResponse::Problem(403, "forbidden", "A panel user is needed to read command history");
    std::lock_guard const lock(_storeMutex);
    std::string error;
    std::optional<PanelStore::Statement> rows = _store.Prepare(
        "SELECT command FROM panel_command_history WHERE user_id = ? AND app = ? ORDER BY id DESC LIMIT 50", error);
    if (!rows)
        return AdminResponse::Problem(503, "history_unavailable", error);
    rows->Bind(1, user->Id);
    rows->Bind(2, path);
    nlohmann::json commands = nlohmann::json::array();
    while (rows->Step(error))
        commands.push_back(rows->Text(0));
    if (!error.empty())
        return AdminResponse::Problem(503, "history_unavailable", error);
    nlohmann::json answer{
        { "schema", 1 },
        { "app", path },
        { "commands", std::move(commands) }
    };
    return AdminResponse::Json(200, answer.dump());
}

std::optional<AdminResponse> Panel::Throttle(AdminRequest const& request, uint32 cost)
{
    if (cost == 0)
        return std::nullopt;
    PanelRateVerdict const verdict = _rateLimit.Take(request.Principal, request.RemoteAddress, cost);
    if (verdict.Allowed)
        return std::nullopt;
    if (verdict.FirstThisMinute)
    {
        AuditEvent event;
        event.Name = "panel:request.throttled";
        event.Actor = request.Principal == "token" ? AuditActor::Token : AuditActor::User;
        event.ActorId = request.Principal;
        event.Address = request.RemoteAddress;
        event.Result = AuditResult::Throttled;
        event.Reason = fmt::format("{} {} costs {}, which is more than the {} bucket has left", request.Method, request.Path, cost, verdict.Bucket);
        event.Properties = fmt::format(R"({{"method":"{}","path":"{}","cost":{},"bucket":"{}","retry_after":{}}})",
            request.Method, request.Path, cost, verdict.Bucket, verdict.RetryAfterSeconds);
        event.On("route", fmt::format("{} {}", request.Method, request.Path));
        std::string error;
        if (!Record(event, {}, error))
            AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A throttled request could not be recorded: {}", error);
    }
    AdminResponse held = AdminResponse::Problem(429, "too_many_requests",
        fmt::format("The panel is holding this request back; try again in {} second{}", verdict.RetryAfterSeconds, verdict.RetryAfterSeconds == 1 ? "" : "s"));
    held.Headers.emplace_back("Retry-After", std::to_string(verdict.RetryAfterSeconds));
    return held;
}

std::optional<AdminResponse> Panel::HeldBack(PanelSignInThrottle& throttle, std::string_view username, std::string_view address, std::string_view counted)
{
    PanelSignInVerdict const verdict = throttle.Check(username, address);
    if (verdict.Allowed)
        return std::nullopt;
    if (verdict.FirstThisWindow)
    {
        AuditEvent held;
        held.Name = std::string(counted);
        held.Actor = AuditActor::User;
        held.Address = std::string(address);
        held.Result = AuditResult::Throttled;
        held.Reason = fmt::format("too many failed checks counted against the {}", verdict.Counted);
        held.On("panel_user", "", std::string(username));
        std::string failure;
        if (!Record(held, {}, failure))
            AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A throttled check could not be recorded: {}", failure);
    }
    AdminResponse answer = AdminResponse::Problem(429, "too_many_requests", "Too many attempts; wait and try again");
    answer.Headers.emplace_back("Retry-After", std::to_string(verdict.RetryAfterSeconds));
    return answer;
}

void Panel::RecordRefused(std::string_view name, PanelUser const& user, AdminRequest const& request, std::string_view reason)
{
    AuditEvent refused;
    refused.Name = std::string(name);
    refused.Actor = AuditActor::User;
    refused.ActorId = std::to_string(user.Id);
    refused.ActorName = user.Username;
    refused.Address = request.RemoteAddress;
    refused.UserAgent = request.UserAgent;
    refused.Result = AuditResult::Refused;
    refused.Reason = std::string(reason);
    refused.On("panel_user", std::to_string(user.Id), user.Username);
    std::string failure;
    if (!Record(refused, {}, failure))
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A refused {} could not be recorded: {}", name, failure);
}

void Panel::RegisterSignIn()
{
    AdminRouter& routes = _listener.Routes();
    routes.AddPublic("POST", "/api/panel/session", [this](AdminRequest const& request) { return SignIn(request); });
    routes.AddPublic("POST", "/api/panel/session/second-factor", [this](AdminRequest const& request) { return SecondFactor(request); });
    routes.AddPublic("POST", "/api/panel/claim", [this](AdminRequest const& request) { return Claim(request); });
    routes.AddPublic("GET", "/api/panel/session", [this](AdminRequest const& request) { return Probe(request); });
    routes.AddPublic("GET", "/api/session", [this](AdminRequest const& request) { return Probe(request); });
    routes.AddPublic("POST", "/api/panel/reset", [this](AdminRequest const& request) { return Reset(request); });
    routes.AddPublic("POST", std::string(LinkPath), [this](AdminRequest const& request) { return TradeLink(request); });
    routes.AddEnrollment("DELETE", "/api/panel/session", [this](AdminRequest const& request) { return SignOut(request); });
    routes.AddEnrollment("GET", "/api/panel/me", [this](AdminRequest const& request) { return WhoAmI(request); });
    routes.AddOpen("GET", "/api/panel/permissions", [](AdminRequest const&) { return AdminResponse::Json(200, PanelPermissions::CatalogJson()); });
    routes.AddGuarded("GET", "/api/panel/settings", "panel.settings", [this](AdminRequest const& request) { return PanelSettingsGet(request); });
    routes.AddGuarded("PATCH", "/api/panel/settings", "panel.settings", [this](AdminRequest const& request) { return PanelSettingsUpdate(request); });
    routes.AddGuarded("POST", "/api/panel/settings/mail/test", "panel.settings", [this](AdminRequest const& request) { return MailTest(request); });
    routes.AddGuarded("GET", "/api/panel/audit/verify", "activity.read", [this](AdminRequest const&)
    {
        AuditChainVerification verification;
        std::string error;
        if (!VerifyAuditChain(verification, error))
            return AdminResponse::Problem(503, "audit_unavailable", error);
        nlohmann::json const answer{
            { "schema", 1 },
            { "valid", verification.Valid },
            { "rows_checked", verification.RowsChecked },
            { "elapsed_ms", verification.ElapsedMs },
            { "budget_ms", PanelAudit::VerificationBudgetMs },
            { "pending_events", verification.PendingEvents },
            { "collector_enabled", _auditForwarding },
            { "first_invalid_id", verification.FirstInvalidId == 0 ? nlohmann::json(nullptr) : nlohmann::json(verification.FirstInvalidId) },
            { "last_row_id", verification.LastRowId },
            { "problem", verification.Problem }
        };
        return AdminResponse::Json(200, answer.dump());
    });
    routes.AddGuarded("GET", "/api/panel/errors", "errors.read", [this](AdminRequest const&)
    {
        std::string error;
        std::lock_guard const lock(_storeMutex);
        std::vector<PanelErrorGroup> const groups = _errors.List(error);
        if (!error.empty())
            return AdminResponse::Problem(503, "errors_unavailable", error);
        nlohmann::json answer;
        answer["schema"] = 1;
        answer["groups"] = nlohmann::json::array();
        for (PanelErrorGroup const& group : groups)
        {
            nlohmann::json context = nlohmann::json::parse(group.ContextBeforeJson, nullptr, false);
            if (!context.is_array())
                return AdminResponse::Problem(503, "errors_unavailable", fmt::format("Error group {} has invalid saved log context", group.Id));
            answer["groups"].push_back({
                { "id", group.Id }, { "app", group.App }, { "category", group.Category }, { "level", group.Level },
                { "file", group.File }, { "line", group.Line }, { "function", group.Function }, { "template", group.Template },
                { "revision", group.Revision }, { "count", group.Count }, { "total_count", group.TotalCount },
                { "first_epoch_ms", group.FirstEpochMs }, { "last_epoch_ms", group.LastEpochMs },
                { "last_message", group.LastMessage }, { "context_before", std::move(context) },
                { "new_since_cleared", group.IsNewSinceCleared() }
            });
        }
        return AdminResponse::Json(200, answer.dump());
    });
    routes.AddGuarded("POST", "/api/panel/errors/clear", "errors.clear", [this](AdminRequest const& request)
    {
        return ClearError(request);
    });
    routes.AddGuarded("POST", "/api/panel/errors/report/preview", "errors.read", [this](AdminRequest const& request)
    {
        return ErrorReport(request, false);
    });
    routes.AddGuarded("POST", "/api/panel/errors/report", "errors.report", [this](AdminRequest const& request)
    {
        return ErrorReport(request, true);
    });
}

AdminResponse Panel::ClearError(AdminRequest const& request)
{
    nlohmann::json const body = nlohmann::json::parse(request.Body, nullptr, false);
    if (!body.is_object() || !body.contains("id") || !body["id"].is_number_integer())
        return AdminResponse::Invalid("Clearing an error group takes its positive id", { { "id", "Choose an error group" } });

    int64 id = 0;
    if (body["id"].is_number_unsigned())
    {
        uint64 const value = body["id"].get<uint64>();
        if (value > static_cast<uint64>(std::numeric_limits<int64>::max()))
            return AdminResponse::Invalid("The error group id must be a positive whole number", { { "id", "Choose a valid error group" } });
        id = static_cast<int64>(value);
    }
    else
    {
        id = body["id"].get<int64>();
    }
    if (id <= 0)
        return AdminResponse::Invalid("The error group id must be a positive whole number", { { "id", "Choose a valid error group" } });

    PanelErrorGroup selected;
    {
        std::lock_guard const lock(_storeMutex);
        std::string error;
        std::vector<PanelErrorGroup> const groups = _errors.List(error);
        if (!error.empty())
            return AdminResponse::Problem(503, "errors_unavailable", error);
        auto const found = std::find_if(groups.begin(), groups.end(), [id](PanelErrorGroup const& group) { return group.Id == id; });
        if (found == groups.end())
            return AdminResponse::Problem(404, "error_group_missing", fmt::format("Error group {} no longer exists", id));
        selected = *found;
    }

    AuditEvent event;
    event.Name = "errors:group.cleared";
    std::optional<PanelUser> const actor = UserOf(request);
    event.Actor = actor ? AuditActor::User : AuditActor::Token;
    event.ActorId = actor ? std::to_string(actor->Id) : request.Principal;
    event.ActorName = actor ? actor->Username : request.Principal;
    event.Address = request.RemoteAddress;
    event.UserAgent = request.UserAgent;
    nlohmann::json properties;
    properties["id"] = id;
    properties["request"] = request.Id;
    event.Properties = properties.dump();
    event.On("error_group", std::to_string(id), selected.App + " " + selected.Category);
    std::string error;
    if (!Record(event, [this, id](std::string& failure) { return _errors.Clear(id, PanelStore::NowEpochMs(), failure); }, error))
        return AdminResponse::Problem(503, "error_clear_unavailable", error);

    return AdminResponse::Json(200, R"({"schema":1,"cleared":true})");
}

AdminResponse Panel::ErrorReport(AdminRequest const& request, bool create)
{
    nlohmann::json const body = nlohmann::json::parse(request.Body, nullptr, false);
    if (!body.is_object() || !body.contains("groups") || !body["groups"].is_array() ||
        !body.contains("include_rendered") || !body["include_rendered"].is_boolean())
        return AdminResponse::Invalid("Building an error report takes group ids and an include_rendered choice",
            { { "groups", "Choose one or more error groups" }, { "include_rendered", "Choose whether to include rendered log text" } });
    if (body["groups"].empty() || body["groups"].size() > 200)
        return AdminResponse::Invalid("Choose from 1 to 200 error groups", { { "groups", "Choose from 1 to 200 groups" } });

    std::vector<int64> ids;
    for (nlohmann::json const& id : body["groups"])
    {
        int64 value = 0;
        if (id.is_number_unsigned())
        {
            uint64 const unsignedValue = id.get<uint64>();
            if (unsignedValue > static_cast<uint64>(std::numeric_limits<int64>::max()))
                return AdminResponse::Invalid("Error group ids must be positive whole numbers", { { "groups", "Every id must be a positive whole number" } });
            value = static_cast<int64>(unsignedValue);
        }
        else if (id.is_number_integer())
        {
            value = id.get<int64>();
        }
        if (value <= 0)
            return AdminResponse::Invalid("Error group ids must be positive whole numbers", { { "groups", "Every id must be a positive whole number" } });
        ids.push_back(value);
    }
    std::sort(ids.begin(), ids.end());
    if (std::adjacent_find(ids.begin(), ids.end()) != ids.end())
        return AdminResponse::Invalid("An error group can appear only once", { { "groups", "Remove repeated ids" } });

    std::string error;
    nlohmann::json report;
    std::vector<PanelErrorGroup> selected;
    {
        std::lock_guard const lock(_storeMutex);
        std::vector<PanelErrorGroup> const all = _errors.List(error);
        if (!error.empty())
            return AdminResponse::Problem(503, "errors_unavailable", error);
        selected.reserve(ids.size());
        for (int64 id : ids)
        {
            auto const found = std::find_if(all.begin(), all.end(), [id](PanelErrorGroup const& group) { return group.Id == id; });
            if (found == all.end())
                return AdminResponse::Problem(404, "error_group_missing", fmt::format("Error group {} no longer exists", id));
            selected.push_back(*found);
        }
        std::optional<nlohmann::json> built = PanelErrorReport::Build(selected, body["include_rendered"].get<bool>(), error);
        if (!built)
            return AdminResponse::Problem(503, "report_unavailable", error);
        report = std::move(*built);
    }

    if (create)
    {
        AuditEvent event;
        event.Name = "errors:report.created";
        std::optional<PanelUser> const actor = UserOf(request);
        event.Actor = actor ? AuditActor::User : AuditActor::Token;
        event.ActorId = actor ? std::to_string(actor->Id) : request.Principal;
        event.ActorName = actor ? actor->Username : request.Principal;
        event.Address = request.RemoteAddress;
        event.UserAgent = request.UserAgent;
        nlohmann::json properties;
        properties["group_count"] = selected.size();
        properties["include_rendered"] = body["include_rendered"];
        properties["request"] = request.Id;
        event.Properties = properties.dump();
        for (PanelErrorGroup const& group : selected)
            event.On("error_group", std::to_string(group.Id), group.App + " " + group.Category);
        if (!Record(event, {}, error))
            return AdminResponse::Problem(503, "audit_unavailable", error);
    }

    nlohmann::json answer;
    answer["schema"] = 1;
    answer["report"] = std::move(report);
    return AdminResponse::Json(200, answer.dump());
}

void Panel::RegisterTwoFactor()
{
    AdminRouter& routes = _listener.Routes();
    routes.AddEnrollment("GET", "/api/panel/me/two-factor", [this](AdminRequest const& request) { return TwoFactorGet(request); });
    routes.AddEnrollment("POST", "/api/panel/me/two-factor/setup", [this](AdminRequest const& request) { return TwoFactorSetup(request); });
    routes.AddEnrollment("POST", "/api/panel/me/two-factor/enable", [this](AdminRequest const& request) { return TwoFactorEnable(request); }, PasswordCheckCost);
    routes.AddEnrollment("POST", "/api/panel/me/two-factor/disable", [this](AdminRequest const& request) { return TwoFactorDisable(request); }, PasswordCheckCost);
    routes.AddEnrollment("POST", "/api/panel/me/two-factor/recovery-codes", [this](AdminRequest const& request) { return RecoveryCodes(request); }, PasswordCheckCost);
    routes.AddOpenCosting("POST", "/api/panel/step-up", PasswordCheckCost, [this](AdminRequest const& request) { return StepUpRoute(request); });
}

void Panel::RegisterCommandHistory()
{
    _listener.Routes().AddGuardedPrefix("GET", "/api/panel/apps/", "console.read", [this](AdminRequest const& request)
    {
        return CommandHistoryGet(request);
    });
}

void Panel::RegisterMaintenance()
{
    _listener.Routes().AddGuarded("GET", "/api/panel/maintenance", "status.read", [this](AdminRequest const& request)
    {
        return MaintenanceGet(request);
    });
    _listener.Routes().AddGuarded("POST", "/api/panel/maintenance/enter", "panel.maintenance", [this](AdminRequest const& request)
    {
        return MaintenanceEnter(request);
    });
    _listener.Routes().AddGuarded("POST", "/api/panel/maintenance/exit", "panel.maintenance", [this](AdminRequest const& request)
    {
        return MaintenanceExit(request);
    });
}

AdminResponse Panel::MaintenanceGet(AdminRequest const& request)
{
    (void)request;
    std::lock_guard const lock(_storeMutex);
    if (!_store.IsOpen())
        return AdminResponse::Problem(503, "maintenance_unavailable", "The panel store is not open");
    MaintenanceState state;
    std::string error;
    if (!PanelMaintenance::Read(_store, state, error))
        return AdminResponse::Problem(503, "maintenance_unavailable", error);
    return AdminResponse::Json(200, PanelMaintenance::Answer(state).dump());
}

AdminResponse Panel::MaintenanceEnter(AdminRequest const& request)
{
    if (!_appCall)
        return AdminResponse::Problem(503, "maintenance_unavailable", "The panel cannot reach the loginserver");
    nlohmann::json const body = request.Body.empty() ? nlohmann::json() : nlohmann::json::parse(request.Body, nullptr, false);
    if (body.is_discarded() || !body.is_object())
        return AdminResponse::Invalid("Entering maintenance takes a JSON object", { { "reason", "Say why the installation closes" } });
    auto const reason = body.find("reason");
    auto const windowStart = body.find("window_start_epoch_ms");
    auto const windowEnd = body.find("window_end_epoch_ms");
    if (reason == body.end() || !reason->is_string())
        return AdminResponse::Invalid("Entering maintenance takes a reason", { { "reason", "Say why the installation closes" } });
    std::optional<int64> start;
    std::optional<int64> end;
    if (windowStart != body.end() && !windowStart->is_null())
    {
        if (!windowStart->is_number_integer())
            return AdminResponse::Invalid("The window start is not a time", { { "window_start_epoch_ms", "Give the window start in epoch milliseconds" } });
        start = windowStart->get<int64>();
    }
    if (windowEnd != body.end() && !windowEnd->is_null())
    {
        if (!windowEnd->is_number_integer())
            return AdminResponse::Invalid("The window end is not a time", { { "window_end_epoch_ms", "Give the window end in epoch milliseconds" } });
        end = windowEnd->get<int64>();
    }

    AuditActor actor = AuditActor::Token;
    std::string actorId = request.Principal;
    std::string actorName = NameOf(request);
    if (std::optional<PanelUser> const user = UserOf(request))
    {
        actor = AuditActor::User;
        actorId = std::to_string(user->Id);
        actorName = user->Username;
    }
    MaintenanceState state;
    std::string error;
    bool const entered = PanelMaintenance::Enter(_store, _storeMutex, _appCall, _auditForwarding, actor, actorId, actorName,
        request.RemoteAddress, request.UserAgent, reason->get<std::string>(), start, end, state, error);
    if (entered && _auditForwarding)
        _auditForwarder.Wake();
    if (!entered)
        return AdminResponse::Problem(503, "maintenance_not_entered", error);
    return AdminResponse::Json(200, PanelMaintenance::Answer(state).dump());
}

AdminResponse Panel::MaintenanceExit(AdminRequest const& request)
{
    if (!_appCall)
        return AdminResponse::Problem(503, "maintenance_unavailable", "The panel cannot reach the loginserver");
    AuditActor actor = AuditActor::Token;
    std::string actorId = request.Principal;
    std::string actorName = NameOf(request);
    if (std::optional<PanelUser> const user = UserOf(request))
    {
        actor = AuditActor::User;
        actorId = std::to_string(user->Id);
        actorName = user->Username;
    }
    MaintenanceState state;
    std::string error;
    bool const left = PanelMaintenance::Exit(_store, _storeMutex, _appCall, _auditForwarding, actor, actorId, actorName,
        request.RemoteAddress, request.UserAgent, state, error);
    if (left && _auditForwarding)
        _auditForwarder.Wake();
    if (!left)
        return AdminResponse::Problem(503, "maintenance_not_left", error);
    return AdminResponse::Json(200, PanelMaintenance::Answer(state).dump());
}

void Panel::RegisterOpsCalendar()
{
    _listener.Routes().AddGuarded("GET", "/api/ops/calendar", "status.read", [this](AdminRequest const& request)
    {
        return OpsCalendarGet(request);
    });
}

AdminResponse Panel::OpsCalendarGet(AdminRequest const& request)
{
    int64 from = 0;
    int64 to = 0;
    if (!PanelOpsCalendar::ParseEpochMs(request.Query("from"), from) || !PanelOpsCalendar::ParseEpochMs(request.Query("to"), to))
        return AdminResponse::Invalid("Reading the operations calendar takes a from and a to in epoch milliseconds",
            { { "from", "Give the range start in epoch milliseconds" }, { "to", "Give the range end in epoch milliseconds" } });
    if (from >= to)
        return AdminResponse::Invalid("The calendar range starts after it ends",
            { { "from", "Give a from earlier than to" } });
    constexpr int64 MaxRangeMs = int64(93) * 24 * 60 * 60 * 1000;
    if (to - from > MaxRangeMs)
        return AdminResponse::Invalid("The calendar range is longer than 93 days",
            { { "to", "Give a range of at most 93 days" } });

    std::lock_guard const lock(_storeMutex);
    if (!_store.IsOpen())
        return AdminResponse::Problem(503, "calendar_unavailable", "The panel store is not open");

    std::vector<std::string> viewer;
    if (std::optional<PanelUser> const user = UserOf(request))
    {
        std::string error;
        for (PanelGrant const& grant : _grants.Of(user->Id, error))
            viewer.push_back(grant.Permission);
    }

    std::vector<PanelOpsCalendar::SourceRegistration> registrations;
    PanelOpsCalendar::RegisterDefaults(_store, registrations);
    std::vector<PanelOpsCalendar::CalendarEvent> events;
    std::vector<PanelOpsCalendar::SourceAvailability> sources;
    std::vector<PanelOpsCalendar::CalendarConflict> conflicts;
    std::string error;
    if (!PanelOpsCalendar::Collect(registrations, viewer, from, to, events, sources, conflicts, error))
        return AdminResponse::Problem(503, "calendar_unavailable", error);
    return AdminResponse::Json(200, PanelOpsCalendar::AnswerJson(events, sources, conflicts).dump());
}

AdminResponse Panel::PanelSettingsGet(AdminRequest const& request)
{
    std::string error;
    nlohmann::json const answer = _settings.Answer(request.Query("group"), error);
    return error.empty() ? AdminResponse::Json(200, answer.dump()) : AdminResponse::Problem(503, "settings_unavailable", error);
}

AdminResponse Panel::PanelSettingsUpdate(AdminRequest const& request)
{
    std::optional<PanelUser> const user = UserOf(request);
    nlohmann::json const body = request.Body.empty() ? nlohmann::json() : nlohmann::json::parse(request.Body, nullptr, false);
    if (!user || !body.is_object() || !body.contains("values"))
        return AdminResponse::Invalid("Updating panel settings takes a values object", { { "values", "Give the settings to change" } });
    std::string error;
    AuditEvent event;
    event.Name = "panel:settings.changed";
    event.Actor = AuditActor::User;
    event.ActorId = std::to_string(user->Id);
    event.ActorName = user->Username;
    event.Address = request.RemoteAddress;
    event.Properties = "{\"changed\":true}";
    event.On("panel_user", std::to_string(user->Id), user->Username);
    if (!Record(event, [&](AuditEvent& recorded, std::string& failure)
    {
        if (_settings.Update(body["values"], user->Id, failure, true))
            return true;
        recorded.Result = AuditResult::Refused;
        recorded.Reason = failure;
        return true;
    }, error))
        return AdminResponse::Problem(503, "audit_unavailable", error);
    if (event.Result == AuditResult::Refused)
        return AdminResponse::Problem(409, "settings_refused", event.Reason);
    return PanelSettingsGet(request);
}

namespace
{
    std::string CaptchaVerifyUrl(std::string_view provider)
    {
        if (std::optional<std::string> const override = Ambrose::GetEnv("AMBROSE_TEST_CAPTCHA_VERIFY_URL"); override && !override->empty())
            return *override;
        return PanelCaptcha::VerifyUrlFor(provider);
    }
}

AdminResponse Panel::MailTest(AdminRequest const& request)
{
    std::optional<PanelUser> const user = UserOf(request);
    if (!user)
        return AdminResponse::Problem(401, "not_signed_in", "Testing the mail settings needs a signed-in user");
    if (user->Email.empty())
        return AdminResponse::Problem(409, "mail_no_address", "The signed-in user has no email address, so there is nowhere to send the test mail");

    PanelMailSettings mail;
    mail.SmtpHost = _settings.ValueOf("Mail.SmtpHost");
    mail.TlsMode = _settings.ValueOf("Mail.TlsMode");
    mail.Username = _settings.ValueOf("Mail.Username");
    mail.Password = _settings.ValueOf("Mail.Password");
    mail.FromAddress = _settings.ValueOf("Mail.FromAddress");
    mail.FromName = _settings.ValueOf("Mail.FromName");
    try
    {
        mail.SmtpPort = static_cast<uint16>(std::stoi(_settings.ValueOf("Mail.SmtpPort")));
    }
    catch (std::exception const&)
    {
        mail.SmtpPort = 587;
    }
    if (mail.SmtpHost.empty() || mail.FromAddress.empty())
        return AdminResponse::Problem(409, "mail_not_configured", "Set Mail.SmtpHost and Mail.FromAddress before testing the mail settings");

    PanelMailResult const sent = PanelMail::SendTestMail(mail, user->Email);

    AuditEvent event;
    event.Name = "panel:settings.mail_tested";
    event.Actor = AuditActor::User;
    event.ActorId = std::to_string(user->Id);
    event.ActorName = user->Username;
    event.Address = request.RemoteAddress;
    event.Result = sent.Sent ? AuditResult::Succeeded : AuditResult::Refused;
    event.Reason = sent.Sent ? "the test mail reached " + user->Email : sent.Error;
    event.On("panel_user", std::to_string(user->Id), user->Username);
    std::string failure;
    if (!Record(event, {}, failure))
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A tested mail setting could not be recorded: {}", failure);

    if (!sent.Sent)
        return AdminResponse::Problem(502, "mail_test_failed", sent.Error);
    nlohmann::json answer;
    answer["sent"] = true;
    answer["to"] = user->Email;
    return AdminResponse::Json(200, answer.dump());
}

std::optional<AdminResponse> Panel::CaptchaGate(AdminRequest const& request, nlohmann::json const& body, std::string_view username)
{
    std::string const provider = _settings.ValueOf("Security.CaptchaProvider");
    if (provider.empty() || provider == "off")
        return std::nullopt;
    if (_signIn.RecentFailures(username) < PanelCaptcha::AfterFailures)
        return std::nullopt;

    std::string const token = body.contains("captcha") && body["captcha"].is_string() ? body["captcha"].get<std::string>() : std::string();
    if (token.empty())
        return AdminResponse::Problem(401, "captcha_required", "Too many failed sign-ins; answer the captcha to try again");

    PanelCaptchaResult const checked = PanelCaptcha::Verify(
        provider, _settings.ValueOf("Security.CaptchaSecret"), token, request.RemoteAddress, CaptchaVerifyUrl(provider));
    if (checked.Result == PanelCaptchaResult::Outcome::Verified)
        return std::nullopt;

    _signIn.Failed(username, request.RemoteAddress);
    AuditEvent refused;
    refused.Name = "panel:session.refused";
    refused.Actor = AuditActor::User;
    refused.Address = request.RemoteAddress;
    refused.UserAgent = request.UserAgent;
    refused.Result = AuditResult::Refused;
    refused.On("panel_user", "", std::string(username));
    std::string failure;
    if (checked.Result == PanelCaptchaResult::Outcome::Unreachable || checked.Result == PanelCaptchaResult::Outcome::Misconfigured)
    {
        refused.Reason = checked.Detail.empty()
            ? "the captcha could not be checked, so the sign-in is refused"
            : checked.Detail + ", so the sign-in is refused";
        if (!Record(refused, {}, failure))
            AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A captcha-refused sign-in could not be recorded: {}", failure);
        return AdminResponse::Problem(503, "captcha_unreachable", refused.Reason);
    }
    refused.Reason = checked.Detail;
    if (!Record(refused, {}, failure))
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A captcha-refused sign-in could not be recorded: {}", failure);
    return AdminResponse::Problem(403, "captcha_invalid", checked.Detail);
}

std::string Panel::NameOf(AdminRequest const& request)
{
    if (std::optional<PanelUser> const user = UserOf(request))
        return user->Username;
    return request.Principal;
}

void Panel::RecordReveal(AdminRequest const& request, std::string_view app, std::vector<std::string> const& keys)
{
    if (keys.empty())
        return;
    AuditEvent event;
    event.Name = "settings:secret.revealed";
    event.Actor = request.Principal == "token" ? AuditActor::Token : AuditActor::User;
    event.ActorId = request.Principal;
    event.ActorName = NameOf(request);
    event.Address = request.RemoteAddress;
    event.UserAgent = request.UserAgent;
    event.Node = std::string(app);
    nlohmann::json properties;
    properties["app"] = std::string(app);
    properties["keys"] = keys;
    properties["request"] = request.Id;
    event.Properties = properties.dump();
    event.On("app", std::string(app), std::string(app));
    for (std::string const& key : keys)
        event.On("setting", key, key);
    std::string error;
    if (!Record(event, {}, error))
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A reveal of secret settings on {} could not be recorded: {}", app, error);
}

void Panel::RecordRelayed(AdminRequest const& request, std::string_view app, std::string_view method, std::string_view path, int status, std::string const& body)
{
    std::string_view const bare = path.substr(0, path.find('?'));
    nlohmann::json const answer = nlohmann::json::parse(body, nullptr, false);
    if (method == "GET")
    {
        if (bare != "/api/settings" || status != 200 || !answer.is_object() || !answer.value("revealed", false) || !answer.contains("settings") || !answer["settings"].is_array())
            return;
        std::vector<std::string> keys;
        if (answer.contains("revealed_keys") && answer["revealed_keys"].is_array())
        {
            for (nlohmann::json const& key : answer["revealed_keys"])
                if (key.is_string())
                    keys.push_back(key.get<std::string>());
        }
        else
            for (nlohmann::json const& setting : answer["settings"])
                if (setting.is_object() && setting.value("secret", false) && setting.contains("value") && setting["value"].is_string()
                    && !setting["value"].get_ref<std::string const&>().empty())
                    keys.push_back(setting.value("key", std::string()));
        RecordReveal(request, app, keys);
        return;
    }

    constexpr std::string_view SettingPrefix = "/api/settings/";
    constexpr std::string_view ReloadPrefix = "/api/reload/";
    nlohmann::json const sent = nlohmann::json::parse(request.Body, nullptr, false);
    AuditEvent event;
    std::vector<std::string> keys;
    if (bare == "/api/settings/batch")
    {
        if (auto const dry = sent.find("dry_run"); dry != sent.end() && dry->is_boolean() && dry->get<bool>())
            return;
        event.Name = "settings:batch.changed";
        if (sent.is_object() && sent.contains("entries") && sent["entries"].is_array())
            for (nlohmann::json const& entry : sent["entries"])
                if (entry.is_object() && entry.contains("key") && entry["key"].is_string())
                    keys.push_back(entry["key"].get<std::string>());
    }
    else if (bare.starts_with(SettingPrefix))
    {
        event.Name = method == "DELETE" ? "settings:setting.reset" : "settings:setting.changed";
        keys.emplace_back(bare.substr(SettingPrefix.size()));
    }
    else if (bare.starts_with(ReloadPrefix))
        event.Name = "reload:target.run";
    else
        return;

    event.Actor = request.Principal == "token" ? AuditActor::Token : AuditActor::User;
    event.ActorId = request.Principal;
    event.ActorName = NameOf(request);
    event.Address = request.RemoteAddress;
    event.UserAgent = request.UserAgent;
    event.Node = std::string(app);
    event.Result = status < 300 ? AuditResult::Succeeded : status < 500 ? AuditResult::Refused : AuditResult::Failed;
    if (status >= 300 && answer.is_object() && answer.contains("message") && answer["message"].is_string())
        event.Error = answer["message"].get<std::string>();
    if (sent.is_object() && sent.contains("reason") && sent["reason"].is_string())
        event.Reason = sent["reason"].get<std::string>();
    nlohmann::json properties;
    properties["app"] = std::string(app);
    properties["status"] = status;
    properties["request"] = request.Id;
    if (!keys.empty())
        properties["keys"] = keys;
    if (bare.starts_with(ReloadPrefix))
        properties["target"] = std::string(bare.substr(ReloadPrefix.size()));
    if (answer.is_object() && answer.contains("changed") && answer["changed"].is_boolean())
        properties["changed"] = answer["changed"].get<bool>();
    event.Properties = properties.dump();
    event.On("app", std::string(app), std::string(app));
    for (std::string const& key : keys)
        event.On("setting", key, key);
    if (bare.starts_with(ReloadPrefix))
        event.On("reload_target", std::string(bare.substr(ReloadPrefix.size())));
    std::string error;
    if (!Record(event, {}, error))
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "{} on {} could not be recorded: {}", event.Name, app, error);
}

std::optional<PanelUser> Panel::UserOf(AdminRequest const& request)
{
    constexpr std::string_view Signed = "user:";
    if (!request.Principal.starts_with(Signed))
        return std::nullopt;
    std::optional<int64> const id = Ambrose::StringTo<int64>(std::string_view(request.Principal).substr(Signed.size()));
    if (!id)
        return std::nullopt;
    std::string error;
    return _users.FindById(*id, error);
}

bool Panel::Required(PanelUser const& user)
{
    PanelTwoFactorPolicy const policy = TwoFactorSettings().Required;
    if (policy == PanelTwoFactorPolicy::None)
        return false;
    std::string error;
    std::vector<PanelGrant> const grants = _grants.Of(user.Id, error);
    return PanelTwoFactor::Covers(policy, user, grants);
}

std::optional<AdminResponse> Panel::Admit(AdminRequest const& request)
{
    if (TwoFactorSettings().Required == PanelTwoFactorPolicy::None || request.Principal == "token")
        return std::nullopt;
    std::optional<PanelUser> const user = UserOf(request);
    if (user && (user->TwoFactor || !Required(*user)))
        return std::nullopt;
    return AdminResponse::Problem(403, "two_factor_required", "This panel requires two-factor sign-in for this account; turn it on to carry on");
}

std::optional<AdminResponse> Panel::StepUpCheck(AdminRequest const& request, std::string_view permission, StepUpWhen when)
{
    std::string const method = Ambrose::ToUpper(request.Method);
    if (when == StepUpWhen::Changing)
    {
        PanelPermission const* const held = PanelPermissions::Find(permission);
        bool const changes = method == "POST" || method == "PUT" || method == "PATCH" || method == "DELETE";
        if (!held || !held->Danger || !changes)
            return std::nullopt;
    }
    std::optional<PanelUser> const user = UserOf(request);
    std::optional<std::string> const secret = _listener.Routes().SessionSecret(request);
    if (!user || !secret)
        return AdminResponse::Problem(403, "step_up_unavailable", "Only a panel user signed in from a browser can confirm this action, and this request carries none");

    std::chrono::minutes const window = TwoFactorSettings().StepUpWindow;
    std::optional<int64> const checked = _sessions.CheckedAt(*secret);
    int64 const now = PanelStore::NowEpochMs();
    bool const fresh = checked && now - *checked <= std::chrono::duration_cast<std::chrono::milliseconds>(window).count();
    std::string const app = PanelAuthorization::AppInPath(request.Path);

    AuditEvent event;
    event.Actor = AuditActor::User;
    event.ActorId = std::to_string(user->Id);
    event.ActorName = user->Username;
    event.Address = request.RemoteAddress;
    event.UserAgent = request.UserAgent;
    event.Node = app;
    nlohmann::json properties;
    properties["permission"] = std::string(permission);
    properties["method"] = method;
    properties["path"] = request.Path;
    properties["request"] = request.Id;
    properties["checked_epoch_ms"] = checked ? nlohmann::json(*checked) : nlohmann::json(nullptr);
    if (!app.empty())
        properties["app"] = app;
    event.Properties = properties.dump();
    event.On("panel_user", std::to_string(user->Id), user->Username).On("permission", std::string(permission));
    std::string failure;

    if (!fresh)
    {
        event.Name = "panel:step_up.required";
        event.Result = AuditResult::Refused;
        event.Reason = checked ? "the last check of who this is is older than Panel.StepUpMinutes" : "this session has no check of who this is";
        if (!Record(event, {}, failure))
            AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A step-up that was asked for could not be recorded: {}", failure);
        nlohmann::json body;
        body["error"] = "step_up_required";
        body["message"] = fmt::format("Confirm it is you before using {}", permission);
        body["permission"] = std::string(permission);
        body["methods"] = MethodsFor(user->TwoFactor);
        body["window_seconds"] = std::chrono::duration_cast<std::chrono::seconds>(window).count();
        return AdminResponse::Json(403, body.dump());
    }

    event.Name = "panel:step_up.used";
    event.Reason = fmt::format("{} {}", method, request.Path);
    if (!Record(event, {}, failure))
    {
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "What a step-up check authorized could not be recorded, so it did not go ahead: {}", failure);
        return AdminResponse::Problem(503, "audit_unavailable", "The panel could not record what your check authorized, so it did not go ahead");
    }
    return std::nullopt;
}

namespace
{
    nlohmann::json UserJson(PanelUser const& user)
    {
        nlohmann::json body;
        body["id"] = user.Id;
        body["username"] = user.Username;
        body["display_name"] = user.DisplayName.empty() ? user.Username : user.DisplayName;
        body["owner"] = user.IsOwner;
        body["role"] = PanelPermissions::NameOf(user.Role);
        body["permissions"] = PanelPermissions::KeysOf(user.Role);
        body["must_change_password"] = user.MustChange;
        body["signed_in"] = user.SignedInEpochMs ? nlohmann::json(*user.SignedInEpochMs) : nlohmann::json(nullptr);
        body["two_factor"] = user.TwoFactor;
        return body;
    }
}

nlohmann::json Panel::UserAnswer(PanelUser const& user)
{
    nlohmann::json body = UserJson(user);
    nlohmann::json grants = nlohmann::json::object();
    std::string error;
    std::vector<PanelGrant> const held = _grants.Of(user.Id, error);
    for (PanelGrant const& grant : held)
        grants[grant.App].push_back(grant.Permission);
    if (!error.empty())
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "The panel could not read {}'s grants: {}", user.Username, error);
    body["grants"] = std::move(grants);
    body["two_factor_required"] = PanelTwoFactor::Covers(TwoFactorSettings().Required, user, held);
    return body;
}

AdminResponse Panel::SignIn(AdminRequest const& request)
{
    nlohmann::json const body = request.Body.empty() ? nlohmann::json() : nlohmann::json::parse(request.Body, nullptr, false);
    if (!body.is_object() || !body.contains("username") || !body["username"].is_string() || !body.contains("password") || !body["password"].is_string())
        return AdminResponse::Invalid("Signing in takes a username and a password", { { "username", "Give the name you sign in with" }, { "password", "Give the password" } });

    std::string const username = body["username"].get<std::string>();
    std::string const password = body["password"].get<std::string>();
    std::string const address = request.RemoteAddress;

    PanelSignInVerdict const verdict = _signIn.Check(username, address);
    if (!verdict.Allowed)
    {
        if (verdict.FirstThisWindow)
        {
            AuditEvent held;
            held.Name = "panel:session.throttled";
            held.Actor = AuditActor::User;
            held.Address = address;
            held.Result = AuditResult::Throttled;
            held.Reason = fmt::format("too many failed sign-ins counted against the {}", verdict.Counted);
            held.On("panel_user", "", username);
            std::string failure;
            if (!Record(held, {}, failure))
                AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A throttled sign-in could not be recorded: {}", failure);
        }
        AdminResponse answer = AdminResponse::Problem(429, "too_many_requests", "Too many sign-in attempts; wait and try again");
        answer.Headers.emplace_back("Retry-After", std::to_string(verdict.RetryAfterSeconds));
        return answer;
    }

    if (std::optional<AdminResponse> gated = CaptchaGate(request, body, username))
        return *gated;

    PanelUser user;
    std::string error;
    PanelUserResult const result = _users.Authenticate(username, password, user, error);
    if (result != PanelUserResult::Ok)
    {
        _signIn.Failed(username, address);
        AuditEvent refused;
        refused.Name = "panel:session.refused";
        refused.Actor = AuditActor::User;
        refused.Address = address;
        refused.UserAgent = request.UserAgent;
        refused.Result = AuditResult::Refused;
        refused.Reason = result == PanelUserResult::StoreFailed ? error : std::string(PanelUsers::Explain(result));
        refused.On("panel_user", "", username);
        std::string failure;
        if (!Record(refused, {}, failure))
            AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A refused sign-in could not be recorded: {}", failure);
        return AdminResponse::Problem(401, "sign_in_refused", std::string(PanelUsers::Explain(PanelUserResult::WrongPassword)));
    }

    if (user.TwoFactor)
        return Challenged(user, request, "a username and password");
    _signIn.Succeeded(username, address);
    return OpenFor(user, request, "a username and password");
}

AdminResponse Panel::Challenged(PanelUser const& user, AdminRequest const& request, std::string_view how, LinkUsed const* link)
{
    PanelTwoFactorSettings const settings = TwoFactorSettings();
    std::array<uint8, 32> const bytes = Ambrose::Crypto::GetRandomArray<32>();
    std::string const secret = Base64::Encode(bytes, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
    auto const now = std::chrono::steady_clock::now();
    Challenge challenge;
    challenge.UserId = user.Id;
    challenge.Username = user.Username;
    challenge.How = std::string(how);
    challenge.Expires = now + settings.ChallengeLifetime;
    if (link)
    {
        challenge.LinkId = link->Id;
        challenge.LinkKind = link->Kind;
    }
    {
        std::lock_guard const lock(_challengeMutex);
        std::erase_if(_challenges, [now](auto const& entry) { return now >= entry.second.Expires; });
        if (_challenges.size() >= MaxChallenges)
        {
            auto const oldest = std::min_element(_challenges.begin(), _challenges.end(), [](auto const& left, auto const& right) { return left.second.Expires < right.second.Expires; });
            _challenges.erase(oldest);
        }
        _challenges.insert_or_assign(ChallengeKey(secret), std::move(challenge));
    }

    AuditEvent challenged;
    challenged.Name = "panel:session.challenged";
    challenged.Actor = AuditActor::User;
    challenged.ActorId = std::to_string(user.Id);
    challenged.ActorName = user.Username;
    challenged.Address = request.RemoteAddress;
    challenged.UserAgent = request.UserAgent;
    challenged.Reason = fmt::format("{} checked out, and a second factor is asked for", how);
    if (link)
    {
        nlohmann::json properties;
        properties["link"] = link->Id;
        properties["link_kind"] = link->Kind;
        challenged.Properties = properties.dump();
    }
    challenged.On("panel_user", std::to_string(user.Id), user.Username);
    if (link)
        challenged.On("panel_link", link->Id, link->Kind);
    std::string failure;
    if (!Record(challenged, {}, failure))
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A second-factor challenge could not be recorded: {}", failure);

    int64 const seconds = std::chrono::duration_cast<std::chrono::seconds>(settings.ChallengeLifetime).count();
    nlohmann::json answer;
    answer["second_factor"] = true;
    answer["methods"] = MethodsFor(true);
    answer["expires_seconds"] = seconds;
    AdminResponse response = AdminResponse::Json(200, answer.dump());
    response.Headers.emplace_back("Set-Cookie", _listener.MakeCookie(ChallengeCookie, secret, seconds));
    return response;
}

PanelSecondFactor Panel::CheckSecondFactor(PanelUser const& user, nlohmann::json const& body, std::string& method, std::string& error)
{
    std::lock_guard const lock(_storeMutex);
    if (HasText(body, "code"))
    {
        method = "totp";
        return _twoFactor.Verify(user.Id, body["code"].get_ref<std::string const&>(), error);
    }
    if (HasText(body, "recovery_code"))
    {
        method = "recovery_code";
        return _twoFactor.UseRecoveryCode(user.Id, body["recovery_code"].get_ref<std::string const&>(), error);
    }
    return PanelSecondFactor::Wrong;
}

AdminResponse Panel::SecondFactor(AdminRequest const& request)
{
    auto const expired = [this](std::string message)
    {
        AdminResponse answer = AdminResponse::Problem(401, "challenge_expired", std::move(message));
        answer.Headers.emplace_back("Set-Cookie", _listener.MakeCookie(ChallengeCookie, "", 0));
        return answer;
    };
    nlohmann::json const body = ParseBody(request);
    if (!body.is_object() || HasText(body, "code") == HasText(body, "recovery_code"))
        return AdminResponse::Invalid("The second step of signing in takes a code from your authenticator app or one recovery code",
            { { "code", "Enter the six-digit code your authenticator app shows, or a recovery code instead" } });
    std::optional<std::string> const secret = _listener.Routes().CookieOf(request, ChallengeCookie);
    if (!secret || secret->empty() || secret->size() > 512)
        return expired("That sign-in has run out; sign in again with your name and password");

    PanelTwoFactorSettings const settings = TwoFactorSettings();
    std::string const key = ChallengeKey(*secret);
    Challenge challenge;
    {
        std::lock_guard const lock(_challengeMutex);
        auto const now = std::chrono::steady_clock::now();
        std::erase_if(_challenges, [now](auto const& entry) { return now >= entry.second.Expires; });
        auto const found = _challenges.find(key);
        if (found == _challenges.end())
            return expired("That sign-in has run out; sign in again with your name and password");
        if (found->second.Attempts >= settings.ChallengeAttempts)
        {
            _challenges.erase(found);
            return expired("Too many codes were tried; sign in again with your name and password");
        }
        ++found->second.Attempts;
        challenge = found->second;
    }

    if (std::optional<AdminResponse> held = HeldBack(_secondFactor, challenge.Username, request.RemoteAddress, "panel:session.second_factor_throttled"))
        return std::move(*held);

    std::string error;
    std::optional<PanelUser> const user = _users.FindById(challenge.UserId, error);
    if (!user || user->Disabled)
    {
        std::lock_guard const lock(_challengeMutex);
        _challenges.erase(key);
        return expired("That sign-in has run out; sign in again with your name and password");
    }

    std::string method;
    PanelSecondFactor const checked = CheckSecondFactor(*user, body, method, error);
    if (checked != PanelSecondFactor::Accepted)
    {
        if (checked == PanelSecondFactor::StoreFailed)
            AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A second factor for {} could not be checked: {}", user->Username, error);
        _secondFactor.Failed(user->Username, request.RemoteAddress);
        RecordRefused("panel:session.second_factor_refused", *user, request, PanelTwoFactor::Explain(checked));
        return AdminResponse::Problem(401, "second_factor_refused", "That code does not sign you in");
    }

    {
        std::lock_guard const lock(_challengeMutex);
        _challenges.erase(key);
    }
    _signIn.Succeeded(user->Username, request.RemoteAddress);
    _secondFactor.Succeeded(user->Username, request.RemoteAddress);
    nlohmann::json properties;
    properties["second_factor"] = method;
    if (method == "recovery_code")
        properties["recovery_codes_left"] = _twoFactor.RecoveryCodesLeft(user->Id, error);
    LinkUsed const used{ challenge.LinkId, challenge.LinkKind };
    AdminResponse response = OpenFor(*user, request, fmt::format("{}, then a {}", challenge.How, method == "totp" ? "TOTP code" : "recovery code"), &properties,
        challenge.LinkId.empty() ? nullptr : &used);
    response.Headers.emplace_back("Set-Cookie", _listener.MakeCookie(ChallengeCookie, "", 0));
    return response;
}

AdminResponse Panel::SignOut(AdminRequest const& request)
{
    std::optional<PanelUser> const user = UserOf(request);
    std::string const secret = _listener.Routes().SessionSecret(request).value_or(std::string());
    if (!secret.empty())
    {
        std::string error;
        _sessions.Close(secret, "the operator signed out", error);
    }
    if (user)
    {
        AuditEvent closed;
        closed.Name = "panel:session.closed";
        closed.Actor = AuditActor::User;
        closed.ActorId = std::to_string(user->Id);
        closed.ActorName = user->Username;
        closed.Address = request.RemoteAddress;
        closed.On("panel_user", std::to_string(user->Id), user->Username);
        std::string failure;
        Record(closed, {}, failure);
    }
    AdminResponse response = AdminResponse::Json(200, "{\"signed_out\":true}");
    response.Headers.emplace_back("Set-Cookie", _listener.MakeSessionCookie("", true));
    return response;
}

AdminResponse Panel::WhoAmI(AdminRequest const& request)
{
    std::optional<PanelUser> const user = UserOf(request);
    if (!user)
        return AdminResponse::Problem(404, "not_a_panel_user", "This request is not carrying a panel user's session");
    nlohmann::json answer = UserAnswer(*user);
    answer["csrf"] = request.SessionCsrf.value_or(std::string());
    return AdminResponse::Json(200, answer.dump());
}

std::string Panel::Issuer() const
{
    std::string issuer = Ambrose::ForLog(_settings.ValueOf("Panel.Name"), 64);
    return issuer.empty() ? std::string("Ambrose") : issuer;
}

AdminResponse Panel::TwoFactorGet(AdminRequest const& request)
{
    std::optional<PanelUser> const user = UserOf(request);
    if (!user)
        return AdminResponse::Problem(404, "not_a_panel_user", "This request is not carrying a panel user's session");
    std::string error;
    PanelTwoFactorState const state = _twoFactor.State(user->Id, error);
    if (!error.empty())
    {
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "The two-factor state of {} could not be read: {}", user->Username, error);
        return AdminResponse::Problem(503, "store_unavailable", "The panel could not read your two-factor sign-in; try again");
    }
    nlohmann::json answer;
    answer["enabled"] = state.Enabled;
    answer["pending"] = state.Pending;
    answer["required"] = Required(*user);
    answer["recovery_codes_left"] = state.RecoveryCodesLeft;
    answer["enabled_epoch_ms"] = state.Enabled && state.EnabledEpochMs != 0 ? nlohmann::json(state.EnabledEpochMs) : nlohmann::json(nullptr);
    answer["window_steps"] = _twoFactor.GetWindow();
    answer["issuer"] = Issuer();
    return AdminResponse::Json(200, answer.dump());
}

AdminResponse Panel::TwoFactorSetup(AdminRequest const& request)
{
    std::optional<PanelUser> const user = UserOf(request);
    if (!user)
        return AdminResponse::Problem(404, "not_a_panel_user", "This request is not carrying a panel user's session");
    nlohmann::json const body = ParseBody(request);
    if (!body.is_object() || (body.contains("replace") && !body["replace"].is_boolean()))
        return AdminResponse::Invalid("Setting up two-factor sign-in takes nothing, or replace as true or false", { { "replace", "Give replace as true or false" } });
    bool const replace = body.value("replace", false);
    std::string const issuer = Issuer();
    std::string error;
    std::optional<PanelTwoFactorSetup> setup;
    {
        std::lock_guard const lock(_storeMutex);
        setup = _twoFactor.Setup(user->Id, issuer, user->Username, replace, error);
    }
    if (!setup)
    {
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "Two-factor sign-in could not be set up for {}: {}", user->Username, error);
        return AdminResponse::Problem(503, "store_unavailable", "The panel could not set up two-factor sign-in; try again");
    }
    nlohmann::json answer;
    answer["secret"] = setup->Secret;
    answer["uri"] = setup->Uri;
    answer["issuer"] = issuer;
    answer["account"] = user->Username;
    answer["algorithm"] = "SHA1";
    answer["digits"] = Totp::Digits;
    answer["period"] = Totp::StepSeconds;
    answer["fresh"] = setup->Fresh;
    AdminResponse response = AdminResponse::Json(200, answer.dump());
    WipeText(setup->Secret);
    WipeText(setup->Uri);
    return response;
}

AdminResponse Panel::TwoFactorEnable(AdminRequest const& request)
{
    std::optional<PanelUser> const user = UserOf(request);
    std::optional<std::string> const secret = _listener.Routes().SessionSecret(request);
    if (!user || !secret)
        return AdminResponse::Problem(404, "not_a_panel_user", "This request is not carrying a panel user's session");
    nlohmann::json const body = ParseBody(request);
    std::vector<std::pair<std::string, std::string>> fields;
    if (!HasText(body, "password"))
        fields.emplace_back("password", "Enter your password");
    if (!HasText(body, "code"))
        fields.emplace_back("code", "Enter the six-digit code your authenticator app shows");
    bool const moving = user->TwoFactor;
    if (moving && HasText(body, "current_code") == HasText(body, "current_recovery_code"))
        fields.emplace_back("current_code", "Enter the code the authenticator app you use now shows, or one recovery code");
    if (!fields.empty() && moving)
        return AdminResponse::Invalid("Moving to another authenticator app takes your password, a code from the app you use now and a code from the new one", std::move(fields));
    if (!fields.empty())
        return AdminResponse::Invalid("Turning on two-factor sign-in takes your password and a code from your authenticator app", std::move(fields));

    if (std::optional<AdminResponse> held = HeldBack(_signIn, user->Username, request.RemoteAddress, "panel:session.throttled"))
        return std::move(*held);
    if (std::optional<AdminResponse> held = HeldBack(_secondFactor, user->Username, request.RemoteAddress, "panel:session.second_factor_throttled"))
        return std::move(*held);

    std::string error;
    std::string password = TextOf(body, "password");
    PanelUserResult const matched = _users.CheckPassword(user->Id, password, error);
    WipeText(password);
    if (matched == PanelUserResult::StoreFailed)
        return AdminResponse::Problem(503, "store_unavailable", "The panel could not check your password; try again");
    if (matched != PanelUserResult::Ok)
    {
        _signIn.Failed(user->Username, request.RemoteAddress);
        RecordRefused("user:two_factor.enabled", *user, request, "the password was wrong");
        return CheckRefused();
    }

    std::string const code = TextOf(body, "code");
    std::optional<PanelFactor> current;
    if (moving && HasText(body, "current_recovery_code"))
        current = PanelFactor{ true, TextOf(body, "current_recovery_code") };
    else if (moving)
        current = PanelFactor{ false, TextOf(body, "current_code") };
    PanelSecondFactor outcome = PanelSecondFactor::Accepted;
    std::vector<std::string> codes;
    AuditEvent enabled;
    enabled.Name = "user:two_factor.enabled";
    enabled.Actor = AuditActor::User;
    enabled.ActorId = std::to_string(user->Id);
    enabled.ActorName = user->Username;
    enabled.Address = request.RemoteAddress;
    enabled.UserAgent = request.UserAgent;
    enabled.Reason = "turned on with the password and a code, with ten new recovery codes; every other session of this operator ended";
    if (current)
        enabled.Reason = fmt::format("moved to another authenticator app with the password, a {} from the one in use and a code from the new one, with ten new recovery codes; "
            "every other session of this operator ended", current->Recovery ? "recovery code" : "code");
    enabled.On("panel_user", std::to_string(user->Id), user->Username);
    std::string failure;
    bool const done = Record(enabled, [&](std::string& why)
    {
        outcome = _twoFactor.Activate(user->Id, code, current ? &*current : nullptr, why);
        if (outcome != PanelSecondFactor::Accepted)
        {
            if (why.empty())
                why = std::string(PanelTwoFactor::Explain(outcome));
            return false;
        }
        std::optional<std::vector<std::string>> issued = _twoFactor.IssueRecoveryCodes(user->Id, why);
        if (!issued)
            return false;
        codes = std::move(*issued);
        std::optional<int64> const generation = _users.BumpGeneration(user->Id, why);
        if (!generation)
            return false;
        if (!_sessions.KeepOnly(user->Id, *secret, *generation, "two-factor sign-in was turned on in another session", why))
            return false;
        if (_sessions.MarkChecked(*secret, why))
            return true;
        if (why.empty())
            why = "this session has ended";
        return false;
    }, failure);
    if (!done)
    {
        for (std::string& issued : codes)
            WipeText(issued);
        if (outcome == PanelSecondFactor::NotPending)
            return AdminResponse::Problem(409, "not_set_up", "Set up two-factor sign-in first, then enter a code from it");
        if (outcome == PanelSecondFactor::Wrong || outcome == PanelSecondFactor::Replayed)
        {
            _secondFactor.Failed(user->Username, request.RemoteAddress);
            RecordRefused("user:two_factor.enabled", *user, request, PanelTwoFactor::Explain(outcome));
            return CheckRefused();
        }
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "Two-factor sign-in could not be turned on for {}: {}", user->Username, failure);
        return AdminResponse::Problem(503, "store_unavailable", "The panel could not turn on two-factor sign-in, so nothing changed; try again");
    }

    _secondFactor.Succeeded(user->Username, request.RemoteAddress);
    AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "{} {}; their other sessions have ended", user->Username,
        current ? "moved their two-factor sign-in to another authenticator app" : "turned on two-factor sign-in");
    std::optional<PanelUser> const fresh = _users.FindById(user->Id, error);
    nlohmann::json answer;
    nlohmann::json grouped = nlohmann::json::array();
    for (std::string& issued : codes)
    {
        grouped.push_back(RecoveryCode::Group(issued));
        WipeText(issued);
    }
    answer["recovery_codes"] = std::move(grouped);
    answer["recovery_codes_left"] = RecoveryCode::Count;
    answer["user"] = fresh ? UserAnswer(*fresh) : nlohmann::json(nullptr);
    return AdminResponse::Json(200, answer.dump());
}

AdminResponse Panel::TwoFactorDisable(AdminRequest const& request)
{
    std::optional<PanelUser> const user = UserOf(request);
    std::optional<std::string> const secret = _listener.Routes().SessionSecret(request);
    if (!user || !secret)
        return AdminResponse::Problem(404, "not_a_panel_user", "This request is not carrying a panel user's session");
    if (!user->TwoFactor)
        return AdminResponse::Problem(409, "two_factor_off", "Two-factor sign-in is not on for this account");
    if (Required(*user))
        return AdminResponse::Problem(409, "two_factor_required", "This panel requires two-factor sign-in for this account, so it stays on");
    nlohmann::json const body = ParseBody(request);
    std::vector<std::pair<std::string, std::string>> fields;
    if (!HasText(body, "password"))
        fields.emplace_back("password", "Enter your password");
    if (HasText(body, "code") == HasText(body, "recovery_code"))
        fields.emplace_back("code", "Enter the six-digit code your authenticator app shows, or one recovery code");
    if (!fields.empty())
        return AdminResponse::Invalid("Turning off two-factor sign-in takes your password and a current code", std::move(fields));

    if (std::optional<AdminResponse> held = HeldBack(_signIn, user->Username, request.RemoteAddress, "panel:session.throttled"))
        return std::move(*held);
    if (std::optional<AdminResponse> held = HeldBack(_secondFactor, user->Username, request.RemoteAddress, "panel:session.second_factor_throttled"))
        return std::move(*held);

    std::string error;
    std::string password = TextOf(body, "password");
    PanelUserResult const matched = _users.CheckPassword(user->Id, password, error);
    WipeText(password);
    if (matched == PanelUserResult::StoreFailed)
        return AdminResponse::Problem(503, "store_unavailable", "The panel could not check your password; try again");
    if (matched != PanelUserResult::Ok)
    {
        _signIn.Failed(user->Username, request.RemoteAddress);
        RecordRefused("user:two_factor.disabled", *user, request, "the password was wrong");
        return CheckRefused();
    }
    std::string method;
    PanelSecondFactor const checked = CheckSecondFactor(*user, body, method, error);
    if (checked != PanelSecondFactor::Accepted)
    {
        if (checked == PanelSecondFactor::StoreFailed)
            AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A second factor for {} could not be checked: {}", user->Username, error);
        _secondFactor.Failed(user->Username, request.RemoteAddress);
        RecordRefused("user:two_factor.disabled", *user, request, PanelTwoFactor::Explain(checked));
        return CheckRefused();
    }

    AuditEvent disabled;
    disabled.Name = "user:two_factor.disabled";
    disabled.Actor = AuditActor::User;
    disabled.ActorId = std::to_string(user->Id);
    disabled.ActorName = user->Username;
    disabled.Address = request.RemoteAddress;
    disabled.UserAgent = request.UserAgent;
    disabled.Reason = fmt::format("turned off with the password and a {}; the secret and every recovery code were deleted and every other session of this operator ended",
        method == "totp" ? "TOTP code" : "recovery code");
    disabled.On("panel_user", std::to_string(user->Id), user->Username);
    std::string failure;
    bool const done = Record(disabled, [&](std::string& why)
    {
        if (!_twoFactor.Disable(user->Id, why))
            return false;
        std::optional<int64> const generation = _users.BumpGeneration(user->Id, why);
        if (!generation)
            return false;
        return _sessions.KeepOnly(user->Id, *secret, *generation, "two-factor sign-in was turned off in another session", why);
    }, failure);
    if (!done)
    {
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "Two-factor sign-in could not be turned off for {}: {}", user->Username, failure);
        return AdminResponse::Problem(503, "store_unavailable", "The panel could not turn off two-factor sign-in, so nothing changed; try again");
    }
    _secondFactor.Succeeded(user->Username, request.RemoteAddress);
    AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "{} turned off two-factor sign-in; their other sessions have ended", user->Username);
    std::optional<PanelUser> const fresh = _users.FindById(user->Id, error);
    nlohmann::json answer;
    answer["user"] = fresh ? UserAnswer(*fresh) : nlohmann::json(nullptr);
    return AdminResponse::Json(200, answer.dump());
}

AdminResponse Panel::RecoveryCodes(AdminRequest const& request)
{
    std::optional<PanelUser> const user = UserOf(request);
    if (!user)
        return AdminResponse::Problem(404, "not_a_panel_user", "This request is not carrying a panel user's session");
    if (!user->TwoFactor)
        return AdminResponse::Problem(409, "two_factor_off", "Two-factor sign-in is not on for this account, so it has no recovery codes");
    nlohmann::json const body = ParseBody(request);
    std::vector<std::pair<std::string, std::string>> fields;
    if (!HasText(body, "password"))
        fields.emplace_back("password", "Enter your password");
    if (HasText(body, "code") == HasText(body, "recovery_code"))
        fields.emplace_back("code", "Enter the six-digit code your authenticator app shows, or one recovery code");
    if (!fields.empty())
        return AdminResponse::Invalid("New recovery codes take your password and a current code", std::move(fields));

    if (std::optional<AdminResponse> held = HeldBack(_signIn, user->Username, request.RemoteAddress, "panel:session.throttled"))
        return std::move(*held);
    if (std::optional<AdminResponse> held = HeldBack(_secondFactor, user->Username, request.RemoteAddress, "panel:session.second_factor_throttled"))
        return std::move(*held);

    std::string error;
    std::string password = TextOf(body, "password");
    PanelUserResult const matched = _users.CheckPassword(user->Id, password, error);
    WipeText(password);
    if (matched == PanelUserResult::StoreFailed)
        return AdminResponse::Problem(503, "store_unavailable", "The panel could not check your password; try again");
    if (matched != PanelUserResult::Ok)
    {
        _signIn.Failed(user->Username, request.RemoteAddress);
        RecordRefused("user:recovery_codes.issued", *user, request, "the password was wrong");
        return CheckRefused();
    }
    std::string method;
    PanelSecondFactor const checked = CheckSecondFactor(*user, body, method, error);
    if (checked != PanelSecondFactor::Accepted)
    {
        if (checked == PanelSecondFactor::StoreFailed)
            AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A second factor for {} could not be checked: {}", user->Username, error);
        _secondFactor.Failed(user->Username, request.RemoteAddress);
        RecordRefused("user:recovery_codes.issued", *user, request, PanelTwoFactor::Explain(checked));
        return CheckRefused();
    }

    std::vector<std::string> codes;
    AuditEvent issued;
    issued.Name = "user:recovery_codes.issued";
    issued.Actor = AuditActor::User;
    issued.ActorId = std::to_string(user->Id);
    issued.ActorName = user->Username;
    issued.Address = request.RemoteAddress;
    issued.UserAgent = request.UserAgent;
    issued.Reason = "ten new recovery codes replaced every earlier one";
    issued.On("panel_user", std::to_string(user->Id), user->Username);
    std::string failure;
    bool const done = Record(issued, [&](std::string& why)
    {
        std::optional<std::vector<std::string>> made = _twoFactor.IssueRecoveryCodes(user->Id, why);
        if (!made)
            return false;
        codes = std::move(*made);
        return true;
    }, failure);
    if (!done)
    {
        for (std::string& code : codes)
            WipeText(code);
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "New recovery codes could not be made for {}: {}", user->Username, failure);
        return AdminResponse::Problem(503, "store_unavailable", "The panel could not make new recovery codes, so the old ones still work; try again");
    }
    _secondFactor.Succeeded(user->Username, request.RemoteAddress);
    nlohmann::json grouped = nlohmann::json::array();
    for (std::string& code : codes)
    {
        grouped.push_back(RecoveryCode::Group(code));
        WipeText(code);
    }
    nlohmann::json answer;
    answer["recovery_codes"] = std::move(grouped);
    answer["recovery_codes_left"] = RecoveryCode::Count;
    return AdminResponse::Json(200, answer.dump());
}

AdminResponse Panel::StepUpRoute(AdminRequest const& request)
{
    std::optional<PanelUser> const user = UserOf(request);
    std::optional<std::string> const secret = _listener.Routes().SessionSecret(request);
    if (!user || !secret)
        return AdminResponse::Problem(403, "step_up_unavailable", "Only a panel user signed in from a browser can confirm it is them");
    nlohmann::json const body = ParseBody(request);
    if (!body.is_object())
        return AdminResponse::Invalid("A check of who you are takes a JSON object", { { "code", "Enter a code, a recovery code or your password" } });
    std::string const purpose = body.contains("for") && body["for"].is_string() ? Ambrose::ForLog(body["for"].get_ref<std::string const&>(), MaxReasonBytes) : std::string();
    bool const twoFactor = user->TwoFactor;
    if (twoFactor && HasText(body, "code") == HasText(body, "recovery_code"))
        return AdminResponse::Invalid("This account confirms it is them with a code", { { "code", "Enter the six-digit code your authenticator app shows, or one recovery code" } });
    if (!twoFactor && !HasText(body, "password"))
        return AdminResponse::Invalid("This account confirms it is them with its password", { { "password", "Enter your password" } });

    PanelSignInThrottle& throttle = twoFactor ? _secondFactor : _signIn;
    if (std::optional<AdminResponse> held = HeldBack(throttle, user->Username, request.RemoteAddress, twoFactor ? "panel:session.second_factor_throttled" : "panel:session.throttled"))
        return std::move(*held);

    std::string error;
    std::string method = "password";
    std::string reason;
    bool accepted = false;
    if (twoFactor)
    {
        PanelSecondFactor const checked = CheckSecondFactor(*user, body, method, error);
        accepted = checked == PanelSecondFactor::Accepted;
        reason = std::string(PanelTwoFactor::Explain(checked));
        if (checked == PanelSecondFactor::StoreFailed)
            AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A second factor for {} could not be checked: {}", user->Username, error);
    }
    else
    {
        std::string password = TextOf(body, "password");
        PanelUserResult const matched = _users.CheckPassword(user->Id, password, error);
        WipeText(password);
        accepted = matched == PanelUserResult::Ok;
        reason = accepted ? std::string("accepted") : std::string("the password was wrong");
    }

    AuditEvent event;
    event.Name = "panel:step_up.checked";
    event.Actor = AuditActor::User;
    event.ActorId = std::to_string(user->Id);
    event.ActorName = user->Username;
    event.Address = request.RemoteAddress;
    event.UserAgent = request.UserAgent;
    nlohmann::json properties;
    properties["method"] = method;
    properties["for"] = purpose;
    event.Properties = properties.dump();
    event.On("panel_user", std::to_string(user->Id), user->Username);
    std::string failure;
    if (!accepted)
    {
        throttle.Failed(user->Username, request.RemoteAddress);
        event.Result = AuditResult::Refused;
        event.Reason = reason;
        if (!Record(event, {}, failure))
            AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A refused check of who {} is could not be recorded: {}", user->Username, failure);
        return AdminResponse::Problem(403, "step_up_refused", "That does not confirm it is you");
    }

    event.Reason = purpose.empty() ? std::string("confirmed it is them") : fmt::format("confirmed it is them to {}", purpose);
    auto const mark = [&](std::string& why)
    {
        if (_sessions.MarkChecked(*secret, why))
            return true;
        if (why.empty())
            why = "this session has ended";
        return false;
    };
    if (!Record(event, mark, failure))
    {
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A check of who {} is could not be recorded, so it counts for nothing: {}", user->Username, failure);
        return AdminResponse::Problem(503, "audit_unavailable", "The panel could not record your check, so it counts for nothing; try again");
    }
    throttle.Succeeded(user->Username, request.RemoteAddress);
    nlohmann::json answer;
    answer["checked_epoch_ms"] = PanelStore::NowEpochMs();
    answer["window_seconds"] = std::chrono::duration_cast<std::chrono::seconds>(TwoFactorSettings().StepUpWindow).count();
    return AdminResponse::Json(200, answer.dump());
}

bool Panel::ResetTwoFactor(PanelUser const& user, std::string_view actor, std::string& error)
{
    AuditEvent reset;
    reset.Name = "panel:user.two_factor_reset";
    reset.Actor = AuditActor::System;
    reset.ActorName = std::string(actor);
    reset.Reason = "two-factor sign-in was turned off for an operator who could not sign in, deleting the secret and every recovery code and ending every session";
    reset.On("panel_user", std::to_string(user.Id), user.Username);
    return Record(reset, [&](std::string& why)
    {
        if (!_twoFactor.Disable(user.Id, why))
            return false;
        if (!_users.BumpGeneration(user.Id, why))
            return false;
        return _sessions.CloseEveryOne(user.Id, "two-factor sign-in was reset from the console", why);
    }, error);
}

void Panel::OfferTheOwnerLink()
{
    std::string error;
    bool empty = false;
    {
        std::lock_guard const lock(_storeMutex);
        empty = _store.IsOpen() && _users.IsEmpty(error);
    }
    if (!empty)
    {
        if (!error.empty())
            AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "The panel could not read its users: {}", error);
        return;
    }

    PanelLinkRefusal refusal;
    std::optional<PanelLinkIssued> const issued = IssueLink({ PanelLinkKind::OwnerClaim, {}, {} }, { AuditActor::System, {}, "supervisor", {} }, refusal);
    if (!issued)
    {
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "The panel has no operator yet, and the link that makes the first one could not be made: {}", refusal.Message);
        return;
    }
    AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "The panel has no operator yet. Open this link from this machine within {} minutes to make the first one: {}",
        std::chrono::duration_cast<std::chrono::minutes>(PanelLinks::LifetimeOf(PanelLinkKind::OwnerClaim)).count(), issued->Url);
}

bool Panel::Transact(std::function<bool(std::string& error)> const& change, std::string& error)
{
    error.clear();
    std::lock_guard const lock(_storeMutex);
    if (!_store.IsOpen())
    {
        error = "the panel store is not open, so nothing can be recorded and nothing is changed";
        return false;
    }
    if (!_store.Begin(error))
        return false;
    if (!change(error))
    {
        _store.Rollback();
        return false;
    }
    if (!PanelAudit::Finalize(_store, error, _auditForwarding))
    {
        _store.Rollback();
        return false;
    }
    if (!_store.Commit(error))
    {
        _store.Rollback();
        return false;
    }
    if (_auditForwarding)
        _auditForwarder.Wake();
    return true;
}

bool Panel::InsertOperator(PanelUserDraft const& draft, PanelLinkIssuer const& by, std::string_view reason, int64& id, PanelUserResult& made, std::string& error)
{
    made = _users.InsertUser(draft, &id, error);
    if (made != PanelUserResult::Ok)
    {
        if (error.empty())
            error = std::string(PanelUsers::Explain(made));
        return false;
    }
    AuditEvent created;
    created.Name = "panel:user.created";
    created.Actor = by.Actor;
    created.ActorId = by.Id;
    created.ActorName = by.Name;
    created.Address = by.RemoteAddress;
    created.Reason = std::string(reason);
    nlohmann::json properties;
    properties["role"] = std::string(PanelPermissions::NameOf(draft.Owner ? PanelRole::Owner : PanelRole::Viewer));
    properties["must_change_password"] = draft.MustChange;
    created.Properties = properties.dump();
    created.On("panel_user", std::to_string(id), draft.Username);
    return PanelAudit::Write(_store, created, error);
}

bool Panel::PlainBeyondLoopback() const
{
    if (_secure)
        return false;
    std::optional<asio::ip::address> const bound = Ambrose::Asio::MakeAddress(_listener.GetBindIp());
    return !bound || !Ambrose::Asio::IsLoopback(*bound);
}

bool Panel::BindsOneRemoteAddress() const
{
    std::optional<asio::ip::address> const bound = Ambrose::Asio::MakeAddress(_listener.GetBindIp());
    return bound && !Ambrose::Asio::IsLoopback(*bound) && !Ambrose::Asio::IsUnspecified(*bound);
}

std::string Panel::LinkFor(std::string_view page, std::string_view token) const
{
    return fmt::format("{}://{}:{}/#{}?token={}", _secure ? "https" : "http", UrlHost(AdminClient::ConnectHost(_listener.GetBindIp())), _listener.GetPort(), page, token);
}

std::string Panel::PairingLine(std::string_view host, uint16 port, std::string_view token, std::string_view fingerprint)
{
    if (fingerprint.empty())
        return fmt::format("http://{}:{}/#link?token={}", UrlHost(host), port, token);
    return fmt::format("https://{}:{}/#link?token={}&sha256={}", UrlHost(host), port, token, fingerprint);
}

std::optional<PanelLinkIssued> Panel::IssueLink(PanelLinkAsk const& ask, PanelLinkIssuer const& issuer, PanelLinkRefusal& refusal)
{
    auto const refuse = [&refusal](int status, std::string code, std::string message, std::string field) -> std::optional<PanelLinkIssued>
    {
        refusal.Status = status;
        refusal.Code = std::move(code);
        refusal.Message = std::move(message);
        refusal.Field = std::move(field);
        return std::nullopt;
    };
    if (!_listener.IsRunning() || !IsStoreOpen())
        return refuse(503, "panel_off", "The panel is off (Panel.Enable = 0), so it has no sign-in to hand out; turn it on and start the supervisor again", {});

    std::string const name(Ambrose::Trim(ask.Username));
    std::string host = AdminClient::ConnectHost(_listener.GetBindIp());
    uint16 port = _listener.GetPort();
    std::string fingerprint;
    if (ask.Kind == PanelLinkKind::Local && BindsOneRemoteAddress())
        return refuse(409, "not_on_loopback", fmt::format("Panel.BindIP = {} is one address beyond this machine, so no program here reaches the panel through loopback; "
            "bind 127.0.0.1 or every address, or pair the program instead", _listener.GetBindIp()), {});
    if (ask.Kind == PanelLinkKind::Pairing)
    {
        if (PlainBeyondLoopback())
            return refuse(409, "plain_http_remote", std::string(PlainRemoteMessage), {});
        if (name.empty())
            return refuse(422, "invalid", "A pairing names the operator it signs in", "username");
        std::string problem;
        if (!ReadAddress(ask.Address, _listener.GetPort(), host, port, problem))
            return refuse(422, "invalid", problem, "address");
        if (!_listener.Routes().HostAllowed(host))
            return refuse(422, "invalid", fmt::format("The panel does not answer for {}; add it to Panel.AllowedHosts first", host), "address");
        if (!_secure && !IsLoopbackHost(host))
            return refuse(409, "plain_http_remote", std::string(PlainLocalMessage), {});
        if (_secure)
        {
            fingerprint = _listener.GetFingerprint();
            if (fingerprint.empty())
                return refuse(503, "no_certificate", "The panel could not say which certificate it serves, so it cannot pin one; try again", {});
        }
    }
    if (ask.Kind == PanelLinkKind::Password && name.empty())
        return refuse(422, "invalid", "A password link names the operator it is for", "username");

    std::string error;
    bool empty = false;
    std::optional<PanelUser> user;
    if (ask.Kind != PanelLinkKind::OwnerClaim)
    {
        std::lock_guard const lock(_storeMutex);
        empty = _users.IsEmpty(error);
        if (error.empty() && !empty)
            user = name.empty() ? _users.FirstOwner(error) : _users.Find(name, error);
    }
    if (!error.empty())
    {
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "The panel could not read its operators to make a link: {}", error);
        return refuse(503, "store_unavailable", "The panel could not read its operators; try again", {});
    }
    bool const makeOwner = empty && (ask.Kind == PanelLinkKind::Local || ask.Kind == PanelLinkKind::Pairing);
    if (ask.Kind != PanelLinkKind::OwnerClaim && !makeOwner)
    {
        if (!user)
            return refuse(404, "no_such_operator", name.empty() ? std::string("The panel has no owner who can sign in") : fmt::format("The panel has no operator named {}", Ambrose::ForLog(name)), {});
        if (user->Disabled)
            return refuse(409, "operator_disabled", fmt::format("{} is disabled, so no link signs them in; enable them first", user->Username), {});
    }

    PanelUserDraft draft;
    if (makeOwner)
    {
        std::string password = PanelUsers::Unguessable();
        PanelUserResult const prepared = _users.PrepareUser(name.empty() ? std::string_view("owner") : std::string_view(name), password, true, true, draft, error);
        WipeText(password);
        if (prepared == PanelUserResult::NameTooShort || prepared == PanelUserResult::NameTooLong || prepared == PanelUserResult::NameInvalid || prepared == PanelUserResult::NameTaken)
            return refuse(422, "invalid", std::string(PanelUsers::Explain(prepared)), "username");
        if (prepared != PanelUserResult::Ok)
        {
            AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "The panel's first owner could not be made for a link: {}", error.empty() ? std::string(PanelUsers::Explain(prepared)) : error);
            return refuse(503, "store_unavailable", "The panel could not make its first owner; try again", {});
        }
    }

    PanelLinkMinted const minted = _links.Mint(ask.Kind);
    std::string const issuerName = issuer.Actor == AuditActor::Token ? std::string("token") : issuer.Name;
    std::string const kindName(PanelLinks::NameOf(ask.Kind));
    std::string const stored = ask.Kind == PanelLinkKind::Pairing ? fmt::format("{}:{}", UrlHost(host), port) : std::string();
    int64 const seconds = std::chrono::duration_cast<std::chrono::seconds>(PanelLinks::LifetimeOf(ask.Kind)).count();
    int64 userId = user ? user->Id : 0;
    std::string const username = user ? user->Username : draft.Username;
    PanelUserResult made = PanelUserResult::Ok;
    bool taken = false;
    bool const kept = Transact([&](std::string& why)
    {
        if (makeOwner)
        {
            bool const nobody = _users.IsEmpty(why);
            if (!why.empty())
                return false;
            if (!nobody)
            {
                taken = true;
                why = "another operator was made first";
                return false;
            }
            if (!InsertOperator(draft, issuer, fmt::format("the first owner was made by a {} link, with a password nobody is told", kindName), userId, made, why))
                return false;
            if (!_links.SpendEvery(PanelLinkKind::OwnerClaim, why))
                return false;
        }
        AuditEvent event;
        event.Name = "panel:link.issued";
        event.Actor = issuer.Actor;
        event.ActorId = issuer.Id;
        event.ActorName = issuer.Name;
        event.Address = issuer.RemoteAddress;
        event.Reason = username.empty() ? fmt::format("a {} link good for {} seconds", kindName, seconds)
                                        : fmt::format("a {} link for {} good for {} seconds", kindName, username, seconds);
        nlohmann::json properties;
        properties["kind"] = kindName;
        properties["user_id"] = userId != 0 ? nlohmann::json(userId) : nlohmann::json(nullptr);
        properties["issuer"] = issuerName;
        properties["expires_epoch_ms"] = minted.ExpiresEpochMs;
        properties["link"] = minted.Id;
        if (ask.Kind == PanelLinkKind::Pairing)
        {
            properties["address"] = stored;
            properties["fingerprint"] = fingerprint.empty() ? nlohmann::json(nullptr) : nlohmann::json(fingerprint);
        }
        event.Properties = properties.dump();
        event.On("panel_link", minted.Id, kindName);
        if (userId != 0)
            event.On("panel_user", std::to_string(userId), username);
        if (!PanelAudit::Write(_store, event, why))
            return false;
        return _links.Keep(minted, ask.Kind, userId != 0 ? std::optional<int64>(userId) : std::nullopt, issuerName, stored, why);
    }, error);
    if (!kept)
    {
        if (taken)
            return refuse(409, "already_claimed", "Another operator was made while this link was being made; ask again and name them", {});
        if (made == PanelUserResult::NameTaken)
            return refuse(422, "invalid", std::string(PanelUsers::Explain(made)), "username");
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A {} link could not be kept, so none was made: {}", kindName, error);
        return refuse(503, "store_unavailable", "The panel could not keep the link, so none was made; try again", {});
    }
    if (makeOwner)
        AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "{} is the panel's owner, made by a {} link, with a password nobody is told until they set their own", username, kindName);

    PanelLinkIssued issued;
    issued.Kind = ask.Kind;
    issued.Token = minted.Token;
    issued.Id = minted.Id;
    issued.UserId = userId;
    issued.Username = username;
    issued.CreatedOwner = makeOwner;
    issued.ExpiresEpochMs = minted.ExpiresEpochMs;
    issued.Host = host;
    issued.Port = port;
    issued.Fingerprint = fingerprint;
    switch (ask.Kind)
    {
        case PanelLinkKind::OwnerClaim: issued.Url = LinkFor("claim", minted.Token); break;
        case PanelLinkKind::Password: issued.Url = LinkFor("password", minted.Token); break;
        case PanelLinkKind::Local: issued.Url = LinkFor("link", minted.Token); break;
        case PanelLinkKind::Pairing: issued.Url = PairingLine(host, port, minted.Token, fingerprint); break;
    }
    return issued;
}

PanelUserResult Panel::MakeOperator(std::string_view username, PanelLinkIssuer const& by, int64& id, std::string& error)
{
    std::string password = PanelUsers::Unguessable();
    PanelUserDraft draft;
    PanelUserResult const prepared = _users.PrepareUser(username, password, false, true, draft, error);
    WipeText(password);
    if (prepared != PanelUserResult::Ok)
        return prepared;
    PanelUserResult made = PanelUserResult::Ok;
    if (Transact([&](std::string& why) { return InsertOperator(draft, by, "an operator was made with a password nobody is told, to set their own from a one-time link", id, made, why); }, error))
        return PanelUserResult::Ok;
    return made != PanelUserResult::Ok ? made : PanelUserResult::StoreFailed;
}

void Panel::RegisterAdminRoutes(AdminRouter& routes)
{
    routes.AddGuarded("POST", std::string(LinksPath), "users.link", [this](AdminRequest const& request) { return MintLinkRoute(request); });
}

AdminResponse Panel::MintLinkRoute(AdminRequest const& request)
{
    nlohmann::json const body = ParseBody(request);
    if (!body.is_object())
        return AdminResponse::Invalid("A link takes a JSON object naming its kind", { { "kind", "Give local or pairing" } });
    std::vector<std::pair<std::string, std::string>> fields;
    for (auto const& [key, value] : body.items())
        if (key != "kind" && key != "username" && key != "address")
            fields.emplace_back(key, "A link takes only kind, username and address");
    std::string const kindText = body.contains("kind") && body["kind"].is_string() ? body["kind"].get<std::string>() : std::string();
    PanelLinkKind kind = PanelLinkKind::Local;
    if (kindText == "pairing")
        kind = PanelLinkKind::Pairing;
    else if (kindText != "local")
        fields.emplace_back("kind", "Give local or pairing");
    for (char const* const key : { "username", "address" })
        if (body.contains(key) && !body[key].is_string())
            fields.emplace_back(key, "Give it as text");
    if (!fields.empty())
        return AdminResponse::Invalid("That is not a link this panel makes", std::move(fields));

    PanelLinkRefusal refusal;
    std::optional<PanelLinkIssued> const issued = IssueLink({ kind, TextOf(body, "username"), TextOf(body, "address") },
        { AuditActor::Token, request.Principal, "token", request.RemoteAddress }, refusal);
    if (!issued)
    {
        if (!refusal.Field.empty())
            return AdminResponse::Invalid(refusal.Message, { { refusal.Field, refusal.Message } });
        return AdminResponse::Problem(refusal.Status, refusal.Code, refusal.Message);
    }
    nlohmann::json answer;
    answer["kind"] = std::string(PanelLinks::NameOf(kind));
    if (kind == PanelLinkKind::Local)
        answer["link"] = issued->Url;
    else
    {
        answer["line"] = issued->Url;
        answer["host"] = issued->Host;
        answer["port"] = issued->Port;
        answer["fingerprint"] = issued->Fingerprint.empty() ? nlohmann::json(nullptr) : nlohmann::json(issued->Fingerprint);
    }
    answer["token"] = issued->Token;
    answer["user_id"] = issued->UserId;
    answer["username"] = issued->Username;
    answer["created_owner"] = issued->CreatedOwner;
    answer["expires_epoch_ms"] = issued->ExpiresEpochMs;
    answer["expires_seconds"] = std::chrono::duration_cast<std::chrono::seconds>(PanelLinks::LifetimeOf(kind)).count();
    return AdminResponse::Json(200, answer.dump());
}

std::optional<AdminResponse> Panel::LinkHeldBack(AdminRequest const& request)
{
    PanelSignInVerdict const verdict = _linkFailures.CheckAddress(request.RemoteAddress);
    if (verdict.Allowed)
        return std::nullopt;
    if (verdict.FirstThisWindow)
    {
        AuditEvent held;
        held.Name = "panel:link.throttled";
        held.Actor = AuditActor::User;
        held.Address = request.RemoteAddress;
        held.UserAgent = request.UserAgent;
        held.Result = AuditResult::Throttled;
        held.Reason = "too many wrong or spent links were tried from this address";
        std::string failure;
        if (!Record(held, {}, failure))
            AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A throttled link could not be recorded: {}", failure);
    }
    AdminResponse answer = AdminResponse::Problem(429, "too_many_requests", "Too many links were tried from here; wait and try again");
    answer.Headers.emplace_back("Retry-After", std::to_string(verdict.RetryAfterSeconds));
    return answer;
}

AdminResponse Panel::LinkRefused(AdminRequest const& request, PanelLink const* link, std::optional<PanelUser> const& user, int status, std::string_view code,
    std::string_view message, std::string_view reason)
{
    _linkFailures.FailedAtAddress(request.RemoteAddress);
    AuditEvent refused;
    refused.Name = "panel:link.refused";
    refused.Actor = AuditActor::User;
    if (user)
    {
        refused.ActorId = std::to_string(user->Id);
        refused.ActorName = user->Username;
    }
    refused.Address = request.RemoteAddress;
    refused.UserAgent = request.UserAgent;
    refused.Result = AuditResult::Refused;
    refused.Reason = std::string(reason);
    nlohmann::json properties;
    properties["answer"] = std::string(code);
    properties["link"] = link ? nlohmann::json(link->Id) : nlohmann::json(nullptr);
    properties["link_kind"] = link ? nlohmann::json(std::string(PanelLinks::NameOf(link->Kind))) : nlohmann::json(nullptr);
    refused.Properties = properties.dump();
    if (link)
    {
        refused.On("panel_link", link->Id, std::string(PanelLinks::NameOf(link->Kind)));
        if (link->UserId)
            refused.On("panel_user", std::to_string(*link->UserId), user ? user->Username : std::string());
    }
    std::string failure;
    if (!Record(refused, {}, failure))
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A refused link could not be recorded: {}", failure);
    return AdminResponse::Problem(status, std::string(code), std::string(message));
}

AdminResponse Panel::TradeLink(AdminRequest const& request)
{
    nlohmann::json const body = ParseBody(request);
    if (!body.is_object())
        return AdminResponse::Invalid("Opening a link takes the token it carries", { { "token", "Give the token from the link" } });
    std::vector<std::pair<std::string, std::string>> fields;
    for (auto const& [key, value] : body.items())
        if (key != "token")
            fields.emplace_back(key, "Opening a link takes only its token");
    if (!HasText(body, "token"))
        fields.emplace_back("token", "Give the token from the link");
    else if (body["token"].get_ref<std::string const&>().size() > PanelLinks::MaxTokenBytes)
        fields.emplace_back("token", "A link's token is far shorter than that");
    if (!fields.empty())
        return AdminResponse::Invalid("That is not a link this panel opens", std::move(fields));

    if (std::optional<AdminResponse> held = LinkHeldBack(request))
        return std::move(*held);

    PanelLink link;
    std::string error;
    PanelLinkState state = PanelLinkState::StoreFailed;
    {
        std::lock_guard const lock(_storeMutex);
        if (_store.IsOpen())
            state = _links.Spend({ PanelLinkKind::Local, PanelLinkKind::Pairing }, body["token"].get_ref<std::string const&>(), request.RemoteAddress, link, error);
        else
            error = "the panel store is not open";
    }
    constexpr std::string_view Refused = "That link does not sign anyone in";
    constexpr std::string_view Gone = "That link has been used or has run out; ask for another";
    switch (state)
    {
        case PanelLinkState::Redeemed:
            break;
        case PanelLinkState::Unknown:
            return LinkRefused(request, nullptr, std::nullopt, 403, "link_refused", Refused, "no link has that token");
        case PanelLinkState::Spent:
            return LinkRefused(request, &link, std::nullopt, 410, "link_expired", Gone, "the link was already used");
        case PanelLinkState::Expired:
            return LinkRefused(request, &link, std::nullopt, 410, "link_expired", Gone, "the link had run out, and it is spent now");
        case PanelLinkState::StoreFailed:
            AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A link could not be read: {}", error);
            return AdminResponse::Problem(503, "store_unavailable", "The panel could not read its links; try again");
    }

    std::optional<asio::ip::address> const from = Ambrose::Asio::MakeAddress(request.RemoteAddress);
    if (link.Kind == PanelLinkKind::Local && (!from || !Ambrose::Asio::IsLoopback(*from)))
        return LinkRefused(request, &link, std::nullopt, 403, "not_this_machine", "A local link opens the panel only on the machine the panel runs on",
            "a local link was used from another machine, so it was burned");
    if (link.Kind == PanelLinkKind::Pairing && PlainBeyondLoopback())
        return LinkRefused(request, &link, std::nullopt, 403, "plain_http_remote", "This panel now serves plain HTTP beyond its own machine, so it takes no pairing link",
            "a pairing link was used while the panel served plain HTTP beyond this machine, so it was burned");
    std::optional<PanelUser> user;
    if (link.UserId)
        user = _users.FindById(*link.UserId, error);
    if (!user || user->Disabled)
        return LinkRefused(request, &link, user, 403, "link_refused", Refused, user ? "the operator is disabled, so the link was burned" : "the operator is gone");

    LinkUsed const used{ link.Id, std::string(PanelLinks::NameOf(link.Kind)) };
    std::string_view const how = link.Kind == PanelLinkKind::Local ? "a local link" : "a pairing link";
    if (user->TwoFactor)
        return Challenged(*user, request, how, &used);
    return OpenFor(*user, request, how, nullptr, &used);
}

AdminResponse Panel::OpenFor(PanelUser const& user, AdminRequest const& request, std::string_view how, nlohmann::json const* properties, LinkUsed const* link)
{
    std::string error;
    std::optional<PanelSessionOpened> opened = _sessions.Open(user.Id, user.Generation, request.RemoteAddress, request.UserAgent, PanelStore::NowEpochMs(), error);
    if (!opened)
    {
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A session could not be opened for {}: {}", user.Username, error);
        return AdminResponse::Problem(503, "store_unavailable", "The panel could not keep your session; try again");
    }

    AuditEvent signedIn;
    signedIn.Name = "panel:session.opened";
    signedIn.Actor = AuditActor::User;
    signedIn.ActorId = std::to_string(user.Id);
    signedIn.ActorName = user.Username;
    signedIn.Address = request.RemoteAddress;
    signedIn.UserAgent = request.UserAgent;
    signedIn.Reason = std::string(how);
    if (properties || link)
    {
        nlohmann::json kept = properties ? *properties : nlohmann::json::object();
        if (link)
        {
            kept["link"] = link->Id;
            kept["link_kind"] = link->Kind;
        }
        signedIn.Properties = kept.dump();
    }
    signedIn.On("panel_session", opened->Id).On("panel_user", std::to_string(user.Id), user.Username);
    if (link)
        signedIn.On("panel_link", link->Id, link->Kind);
    std::string failure;
    if (!Record(signedIn, [&](std::string& why) { return _users.RecordSignIn(user.Id, why); }, failure))
    {
        std::string ignored;
        _sessions.Close(opened->Secret, "the sign-in could not be recorded", ignored);
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A sign-in could not be recorded, so it was refused: {}", failure);
        return AdminResponse::Problem(503, "audit_unavailable", "The panel could not record the sign-in, so it did not sign you in");
    }

    nlohmann::json answer;
    answer["csrf"] = opened->Csrf;
    answer["user"] = UserAnswer(user);
    AdminResponse response = AdminResponse::Json(200, answer.dump());
    response.Headers.emplace_back("Set-Cookie", _listener.MakeSessionCookie(opened->Secret, false));
    return response;
}

AdminResponse Panel::Claim(AdminRequest const& request)
{
    nlohmann::json const body = request.Body.empty() ? nlohmann::json() : nlohmann::json::parse(request.Body, nullptr, false);
    if (!body.is_object() || !body.contains("token") || !body["token"].is_string() || !body.contains("username") || !body["username"].is_string()
        || !body.contains("password") || !body["password"].is_string())
        return AdminResponse::Invalid("Making the first operator takes the link's token, a username and a password",
            { { "token", "Give the token from the link the supervisor printed" }, { "username", "Give the name you want to sign in with" }, { "password", "Give the password" } });

    std::optional<asio::ip::address> const from = Ambrose::Asio::MakeAddress(request.RemoteAddress);
    if (!from || !Ambrose::Asio::IsLoopback(*from))
        return AdminResponse::Problem(403, "not_this_machine", "The first operator is made from the machine the supervisor runs on");

    auto const refused = [] { return AdminResponse::Problem(403, "link_refused", "That link is not the one the supervisor printed"); };
    auto const gone = [] { return AdminResponse::Problem(410, "link_expired", "That link has been used or has run out; restart the supervisor for another"); };
    auto const claimed = [this]
    {
        std::lock_guard const lock(_storeMutex);
        std::string ignored;
        if (_store.IsOpen())
            _links.SpendEvery(PanelLinkKind::OwnerClaim, ignored);
        return AdminResponse::Problem(409, "already_claimed", "The panel already has an operator");
    };

    std::string const token = body["token"].get<std::string>();
    std::string error;
    PanelLink link;
    PanelLinkState state = PanelLinkState::StoreFailed;
    bool empty = false;
    {
        std::lock_guard const lock(_storeMutex);
        if (_store.IsOpen())
        {
            state = _links.Peek({ PanelLinkKind::OwnerClaim }, token, link, error);
            if (state == PanelLinkState::Redeemed)
                empty = _users.IsEmpty(error);
        }
    }
    if (state == PanelLinkState::Unknown)
        return refused();
    if (state == PanelLinkState::Spent || state == PanelLinkState::Expired)
        return gone();
    if (state == PanelLinkState::StoreFailed || !error.empty())
    {
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "The owner link could not be read: {}", error);
        return AdminResponse::Problem(503, "store_unavailable", "The panel could not read its links; try again");
    }
    if (!empty)
        return claimed();

    std::string const username = body["username"].get<std::string>();
    std::string password = body["password"].get<std::string>();
    PanelUserDraft draft;
    PanelUserResult const prepared = _users.PrepareUser(username, password, true, false, draft, error);
    WipeText(password);
    auto const invalid = [](PanelUserResult result)
    {
        bool const named = result == PanelUserResult::NameTaken || result == PanelUserResult::NameInvalid || result == PanelUserResult::NameTooLong || result == PanelUserResult::NameTooShort;
        return AdminResponse::Invalid("The first operator could not be made", { { named ? "username" : "password", std::string(PanelUsers::Explain(result)) } });
    };
    if (prepared != PanelUserResult::Ok)
        return invalid(prepared);

    int64 id = 0;
    PanelUserResult made = PanelUserResult::Ok;
    PanelLinkState spent = PanelLinkState::StoreFailed;
    bool taken = false;
    PanelLinkIssuer const by{ AuditActor::System, {}, "owner link", request.RemoteAddress };
    bool const done = Transact([&](std::string& why)
    {
        spent = _links.Spend({ PanelLinkKind::OwnerClaim }, token, request.RemoteAddress, link, why);
        if (spent != PanelLinkState::Redeemed)
        {
            if (why.empty())
                why = "the owner link could not be used";
            return false;
        }
        bool const nobody = _users.IsEmpty(why);
        if (!why.empty())
            return false;
        if (!nobody)
        {
            taken = true;
            why = "the panel already has an operator";
            return false;
        }
        if (!InsertOperator(draft, by, "the first owner was made from the one-time owner link", id, made, why))
            return false;
        return _links.SpendEvery(PanelLinkKind::OwnerClaim, why);
    }, error);
    if (!done)
    {
        if (spent == PanelLinkState::Unknown)
            return refused();
        if (spent == PanelLinkState::Spent || spent == PanelLinkState::Expired)
            return gone();
        if (taken)
            return claimed();
        if (made != PanelUserResult::Ok && made != PanelUserResult::StoreFailed)
            return invalid(made);
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "The first operator could not be made: {}", error);
        return AdminResponse::Problem(503, "store_unavailable", "The panel could not make its first operator; try again");
    }

    std::optional<PanelUser> const owner = _users.FindById(id, error);
    if (!owner)
        return AdminResponse::Problem(503, "store_unavailable", "The operator was made but could not be read back");
    AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "{} is the panel's owner, made from the one-time link", owner->Username);
    return OpenFor(*owner, request, "the one-time owner link");
}

AdminResponse Panel::Probe(AdminRequest const& request)
{
    std::optional<PanelUser> user;
    std::string csrf;
    if (std::optional<std::string> const secret = _listener.Routes().SessionSecret(request))
    {
        if (std::optional<SessionHolder> const held = _sessions.Hold(*secret))
        {
            csrf = held->Csrf;
            AdminRequest carrying = request;
            carrying.Principal = held->Principal;
            user = UserOf(carrying);
        }
    }
    std::string error;
    nlohmann::json answer;
    answer["app"] = "panel";
    answer["signed_in"] = user.has_value();
    answer["signed_in_with"] = user ? nlohmann::json("session") : nlohmann::json(nullptr);
    answer["csrf"] = user ? nlohmann::json(csrf) : nlohmann::json(nullptr);
    answer["idle_seconds"] = _sessionIdle.count();
    answer["lifetime_seconds"] = _sessionLifetime.count();
    answer["user"] = user ? UserAnswer(*user) : nlohmann::json(nullptr);
    bool needsOwner = false;
    if (!user)
    {
        std::lock_guard const lock(_storeMutex);
        needsOwner = _store.IsOpen() && _users.IsEmpty(error) && _links.AnyOpen(PanelLinkKind::OwnerClaim, error);
    }
    answer["needs_owner"] = needsOwner;
    return AdminResponse::Json(200, answer.dump());
}

std::string Panel::MintPasswordLink(int64 userId)
{
    std::string error;
    std::optional<PanelUser> const user = _users.FindById(userId, error);
    if (!user)
        return {};
    PanelLinkRefusal refusal;
    std::optional<PanelLinkIssued> const issued = IssueLink({ PanelLinkKind::Password, user->Username, {} }, { AuditActor::System, {}, "console", {} }, refusal);
    if (!issued)
    {
        AMBROSE_LOG(_log, LogLevel::Warn, PanelCategory, "No password link was made for {}: {}", user->Username, refusal.Message);
        return {};
    }
    return issued->Token;
}

AdminResponse Panel::Reset(AdminRequest const& request)
{
    nlohmann::json const body = request.Body.empty() ? nlohmann::json() : nlohmann::json::parse(request.Body, nullptr, false);
    if (!body.is_object() || !body.contains("token") || !body["token"].is_string() || !body.contains("password") || !body["password"].is_string())
        return AdminResponse::Invalid("Setting a password takes the link's token and the password",
            { { "token", "Give the token from the link" }, { "password", "Give the password you want" } });

    auto const gone = [] { return AdminResponse::Problem(410, "link_expired", "That link has been used or has run out; ask for another"); };
    std::string const token = body["token"].get<std::string>();
    std::string error;
    PanelLink link;
    PanelLinkState state = PanelLinkState::StoreFailed;
    {
        std::lock_guard const lock(_storeMutex);
        if (_store.IsOpen())
            state = _links.Peek({ PanelLinkKind::Password }, token, link, error);
    }
    if (state == PanelLinkState::StoreFailed)
    {
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A password link could not be read: {}", error);
        return AdminResponse::Problem(503, "store_unavailable", "The panel could not read its links; try again");
    }
    if (state != PanelLinkState::Redeemed || !link.UserId)
        return gone();

    int64 const userId = *link.UserId;
    std::optional<PanelUser> const holder = _users.FindById(userId, error);
    if (!holder)
        return gone();
    std::string password = body["password"].get<std::string>();
    std::string hash;
    PanelUserResult const prepared = _users.PreparePassword(userId, password, hash, error);
    WipeText(password);
    if (prepared == PanelUserResult::UnknownUser)
        return gone();
    if (prepared == PanelUserResult::StoreFailed || prepared == PanelUserResult::HashFailed)
    {
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A password from a one-time link could not be made ready: {}", error);
        return AdminResponse::Problem(503, "store_unavailable", "The panel could not set the password; try again");
    }
    if (prepared != PanelUserResult::Ok)
        return AdminResponse::Invalid("That password was not taken", { { "password", std::string(PanelUsers::Explain(prepared)) } });

    PanelLinkState spent = PanelLinkState::StoreFailed;
    bool const done = Transact([&](std::string& why)
    {
        spent = _links.Spend({ PanelLinkKind::Password }, token, request.RemoteAddress, link, why);
        if (spent != PanelLinkState::Redeemed)
        {
            if (why.empty())
                why = "the password link could not be used";
            return false;
        }
        if (_users.StoreHash(userId, hash, false, why) != PanelUserResult::Ok)
            return false;
        AuditEvent set;
        set.Name = "panel:user.password_set";
        set.Actor = AuditActor::User;
        set.ActorId = std::to_string(userId);
        set.ActorName = holder->Username;
        set.Address = request.RemoteAddress;
        set.UserAgent = request.UserAgent;
        set.Reason = "the password was set from a one-time link, ending every session the operator had";
        nlohmann::json properties;
        properties["link"] = link.Id;
        set.Properties = properties.dump();
        set.On("panel_user", std::to_string(userId), holder->Username).On("panel_link", link.Id, std::string(PanelLinks::NameOf(PanelLinkKind::Password)));
        if (!PanelAudit::Write(_store, set, why))
            return false;
        return _sessions.CloseEveryOne(userId, "the password was set from a one-time link", why);
    }, error);
    if (!done)
    {
        if (spent == PanelLinkState::Unknown || spent == PanelLinkState::Spent || spent == PanelLinkState::Expired)
            return gone();
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "A password from a one-time link could not be set: {}", error);
        return AdminResponse::Problem(503, "store_unavailable", "The panel could not set the password; try again");
    }

    std::optional<PanelUser> const user = _users.FindById(userId, error);
    if (!user)
        return AdminResponse::Problem(503, "store_unavailable", "The password was set but the operator could not be read back");
    AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "{} set a password from a one-time link; every other session that operator had has ended", user->Username);
    if (user->TwoFactor)
        return Challenged(*user, request, "a one-time password link");
    _signIn.Succeeded(user->Username, request.RemoteAddress);
    return OpenFor(*user, request, "a one-time password link");
}
