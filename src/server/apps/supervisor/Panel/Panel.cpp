/*
 * Project Ambrose by Imjustchico
 * Reads the Panel options into a listener of the same shape as an app's admin API, opens the store and then the keyring before the listener so nothing serves without somewhere to write or the keys its secrets need, lends that store under its own lock to the supervisor's live settings and to an owner's protected file patterns, which are saved in the same transaction as the audit row naming who changed them, names the certificate and key in Panel option names when the bind rule refuses them, and starts, reloads and stops the listener beside the supervisor's own; a reload that would leave the bind unsafe or the certificate unservable, or name a two-factor requirement the panel does not know, is refused and the old listener and requirement keep serving. Signing in also says which role the operator holds and every permission that role allows, so the pages a person cannot use are never drawn for them and the panel never has to ask again what somebody is allowed to do. An operator with two-factor sign-in is given no session for a password alone: the password earns a challenge held in memory under the hash of a short-lived cookie, which dies after a few attempts or minutes, and only a code or a recovery code from that operator turns it into a session, the one-time password link included; an operator who already has two-factor sign-in moves to another authenticator only with a current code or recovery code from the one in use as well as the password and a code from the new one, so a session and a password alone cannot swap the factor out. A code or recovery code is checked and spent under the same lock every recorded change holds, so it never lands inside another request's transaction and is never undone with it. Every authenticated route and socket is held to the two-factor requirement except the routes that turn it on, and a danger permission, a secret reveal or a restricted change asks for a check of who the caller is within the last few minutes, records what that check authorized, and changes nothing while it is missing. A route that asks for a permission the catalog does not hold is left out and named in a warning as the panel starts, so a misnamed key costs its page loudly rather than silently. A settings change, reset, batch or reload an app answered through the relay is recorded, a dry run not being a change, with who asked, from where, why and how it ended, refused ones too, and a read that showed a secret is recorded with the keys it showed, never a value; no code, secret or password ever reaches an audit row or a log line. The event socket and its ticket route are registered with the rest, its streams and its sweeper start once the listener is up and stop before it closes.
 */

#include "Panel.h"
#include "AdminConfigView.h"
#include "PanelErrorReport.h"
#include "ConfigMgr.h"
#include "PanelSettingStore.h"
#include "ConstantTime.h"
#include "CryptoRandom.h"
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
}

Panel::Panel(Log& log, std::filesystem::path dataFolder, std::filesystem::path configFolder)
    : _log(log), _dataFolder(std::move(dataFolder)), _store(), _settings(_store), _users(_store), _sessions(_store), _errors(_store), _grants(_store), _keyring(),
      _twoFactor(_store, _keyring), _fileRules(_store), _listener(log, "panel", _dataFolder, std::move(configFolder))
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
    if (!_listener.Start(settings, error))
        return false;
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
    return PanelAudit::Record(_store, event, change, error);
}

bool Panel::Record(AuditEvent& event, std::function<bool(AuditEvent& event, std::string& error)> const& change, std::string& error)
{
    std::lock_guard const lock(_storeMutex);
    if (!_store.IsOpen())
    {
        error = "the panel store is not open, so nothing can be recorded and nothing is changed";
        return false;
    }
    return PanelAudit::Record(_store, event, change, error);
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
    routes.AddEnrollment("DELETE", "/api/panel/session", [this](AdminRequest const& request) { return SignOut(request); });
    routes.AddEnrollment("GET", "/api/panel/me", [this](AdminRequest const& request) { return WhoAmI(request); });
    routes.AddOpen("GET", "/api/panel/permissions", [](AdminRequest const&) { return AdminResponse::Json(200, PanelPermissions::CatalogJson()); });
    routes.AddGuarded("GET", "/api/panel/settings", "panel.settings", [this](AdminRequest const& request) { return PanelSettingsGet(request); });
    routes.AddGuarded("PATCH", "/api/panel/settings", "panel.settings", [this](AdminRequest const& request) { return PanelSettingsUpdate(request); });
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

AdminResponse Panel::Challenged(PanelUser const& user, AdminRequest const& request, std::string_view how)
{
    PanelTwoFactorSettings const settings = TwoFactorSettings();
    std::array<uint8, 32> const bytes = Ambrose::Crypto::GetRandomArray<32>();
    std::string const secret = Base64::Encode(bytes, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
    auto const now = std::chrono::steady_clock::now();
    {
        std::lock_guard const lock(_challengeMutex);
        std::erase_if(_challenges, [now](auto const& entry) { return now >= entry.second.Expires; });
        if (_challenges.size() >= MaxChallenges)
        {
            auto const oldest = std::min_element(_challenges.begin(), _challenges.end(), [](auto const& left, auto const& right) { return left.second.Expires < right.second.Expires; });
            _challenges.erase(oldest);
        }
        _challenges.insert_or_assign(ChallengeKey(secret), Challenge{ user.Id, user.Username, std::string(how), now + settings.ChallengeLifetime, 0 });
    }

    AuditEvent challenged;
    challenged.Name = "panel:session.challenged";
    challenged.Actor = AuditActor::User;
    challenged.ActorId = std::to_string(user.Id);
    challenged.ActorName = user.Username;
    challenged.Address = request.RemoteAddress;
    challenged.UserAgent = request.UserAgent;
    challenged.Reason = fmt::format("{} checked out, and a second factor is asked for", how);
    challenged.On("panel_user", std::to_string(user.Id), user.Username);
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
    AdminResponse response = OpenFor(*user, request, fmt::format("{}, then a {}", challenge.How, method == "totp" ? "TOTP code" : "recovery code"), &properties);
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
    if (!_users.IsEmpty(error))
    {
        if (!error.empty())
            AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "The panel could not read its users: {}", error);
        return;
    }

    std::array<uint8, 32> const bytes = Ambrose::Crypto::GetRandomArray<32>();
    {
        std::lock_guard const lock(_claimMutex);
        _claimToken = Base64::Encode(bytes, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
        _claimExpires = std::chrono::steady_clock::now() + ClaimLifetime;
    }
    std::string const host = _listener.GetBindIp() == "0.0.0.0" || _listener.GetBindIp() == "::" ? std::string("127.0.0.1") : _listener.GetBindIp();
    std::string const link = fmt::format("{}://{}:{}/#claim?token={}", _secure ? "https" : "http", host, _listener.GetPort(), _claimToken);
    AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "The panel has no operator yet. Open this link from this machine within {} minutes to make the first one: {}",
        ClaimLifetime.count(), link);
}

AdminResponse Panel::OpenFor(PanelUser const& user, AdminRequest const& request, std::string_view how, nlohmann::json const* properties)
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
    if (properties)
        signedIn.Properties = properties->dump();
    signedIn.On("panel_session", opened->Id).On("panel_user", std::to_string(user.Id), user.Username);
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

    std::string token;
    {
        std::lock_guard const lock(_claimMutex);
        if (_claimToken.empty() || std::chrono::steady_clock::now() >= _claimExpires)
            return AdminResponse::Problem(410, "link_expired", "That link has been used or has run out; restart the supervisor for another");
        token = _claimToken;
    }
    if (!Ambrose::Crypto::ConstantTimeEquals(body["token"].get<std::string>(), token))
        return AdminResponse::Problem(403, "link_refused", "That link is not the one the supervisor printed");

    std::string error;
    if (!_users.IsEmpty(error))
    {
        std::lock_guard const lock(_claimMutex);
        _claimToken.clear();
        return AdminResponse::Problem(409, "already_claimed", "The panel already has an operator");
    }

    std::string const username = body["username"].get<std::string>();
    int64 id = 0;
    PanelUserResult const made = _users.Create(username, body["password"].get<std::string>(), true, false, &id, error);
    if (made != PanelUserResult::Ok)
    {
        std::string const why(PanelUsers::Explain(made));
        return AdminResponse::Invalid("The first operator could not be made",
            { { made == PanelUserResult::NameTaken || made == PanelUserResult::NameInvalid || made == PanelUserResult::NameTooLong || made == PanelUserResult::NameTooShort ? "username" : "password", why } });
    }

    {
        std::lock_guard const lock(_claimMutex);
        _claimToken.clear();
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
    {
        std::lock_guard const lock(_claimMutex);
        answer["needs_owner"] = !user && !_claimToken.empty() && std::chrono::steady_clock::now() < _claimExpires;
    }
    return AdminResponse::Json(200, answer.dump());
}

std::string Panel::LinkFor(std::string_view token) const
{
    std::string const host = _listener.GetBindIp() == "0.0.0.0" || _listener.GetBindIp() == "::" ? std::string("127.0.0.1") : _listener.GetBindIp();
    return fmt::format("{}://{}:{}/#password?token={}", _secure ? "https" : "http", host, _listener.GetPort(), token);
}

std::string Panel::MintPasswordLink(int64 userId)
{
    std::array<uint8, 32> const bytes = Ambrose::Crypto::GetRandomArray<32>();
    std::string token = Base64::Encode(bytes, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
    std::lock_guard const lock(_claimMutex);
    auto const now = std::chrono::steady_clock::now();
    std::erase_if(_resets, [now](auto const& entry) { return now >= entry.second.second; });
    _resets.emplace(token, std::pair{ userId, now + ClaimLifetime });
    return token;
}

AdminResponse Panel::Reset(AdminRequest const& request)
{
    nlohmann::json const body = request.Body.empty() ? nlohmann::json() : nlohmann::json::parse(request.Body, nullptr, false);
    if (!body.is_object() || !body.contains("token") || !body["token"].is_string() || !body.contains("password") || !body["password"].is_string())
        return AdminResponse::Invalid("Setting a password takes the link's token and the password",
            { { "token", "Give the token from the link" }, { "password", "Give the password you want" } });

    int64 userId = 0;
    {
        std::lock_guard const lock(_claimMutex);
        auto const now = std::chrono::steady_clock::now();
        std::erase_if(_resets, [now](auto const& entry) { return now >= entry.second.second; });
        auto const found = _resets.find(body["token"].get<std::string>());
        if (found == _resets.end())
            return AdminResponse::Problem(410, "link_expired", "That link has been used or has run out; ask for another");
        userId = found->second.first;
        _resets.erase(found);
    }

    std::string error;
    PanelUserResult const set = _users.SetPassword(userId, body["password"].get<std::string>(), false, error);
    if (set != PanelUserResult::Ok)
    {
        std::string const token = MintPasswordLink(userId);
        AdminResponse answer = AdminResponse::Invalid("That password was not taken", { { "password", std::string(PanelUsers::Explain(set)) } });
        nlohmann::json again = nlohmann::json::parse(answer.Body, nullptr, false);
        if (again.is_object())
        {
            again["token"] = token;
            answer.Body = again.dump();
        }
        return answer;
    }

    std::optional<PanelUser> const user = _users.FindById(userId, error);
    if (!user)
        return AdminResponse::Problem(503, "store_unavailable", "The password was set but the operator could not be read back");
    _sessions.CloseEveryOne(userId, "the password was set from a one-time link", error);
    AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "{} set a password from a one-time link; every other session that operator had has ended", user->Username);
    if (user->TwoFactor)
        return Challenged(*user, request, "a one-time password link");
    _signIn.Succeeded(user->Username, request.RemoteAddress);
    return OpenFor(*user, request, "a one-time password link");
}
