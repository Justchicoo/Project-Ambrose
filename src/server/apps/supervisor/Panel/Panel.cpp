/*
 * Project Ambrose by Imjustchico
 * Reads the Panel options into a listener of the same shape as an app's admin API, opens the store before the listener so nothing serves without somewhere to write, names the certificate and key in Panel option names when the bind rule refuses them, and starts, reloads and stops the listener beside the supervisor's own; a reload that would leave the bind unsafe or the certificate unservable is refused and the old listener keeps serving. Signing in also says which role the operator holds and every permission that role allows, so the pages a person cannot use are never drawn for them and the panel never has to ask again what somebody is allowed to do.
 */

#include "Panel.h"
#include "ConfigMgr.h"
#include "ConstantTime.h"
#include "CryptoRandom.h"
#include "Base64.h"
#include "IpAddress.h"
#include "Log.h"
#include "SourceFolder.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <array>
#include <mutex>
#include <utility>

namespace
{
    constexpr char const* PanelCategory = "server.panel";

    std::filesystem::path Configured(ConfigMgr const& config, std::string const& key, std::filesystem::path const& fallback)
    {
        std::filesystem::path const value = ConfigMgr::PathFromUtf8(Ambrose::Trim(config.GetOption<std::string>(key, "", true)));
        return value.empty() ? fallback : value.lexically_normal();
    }
}

Panel::Panel(Log& log, std::filesystem::path dataFolder, std::filesystem::path configFolder)
    : _log(log), _dataFolder(std::move(dataFolder)), _users(_store), _sessions(_store), _errors(_store), _grants(_store), _listener(log, "panel", _dataFolder, std::move(configFolder))
{
    _listener.Routes().SetThrottle([this](AdminRequest const& request, uint32 cost) { return Throttle(request, cost); });
    _listener.SetSessionSource(&_sessions);
    _authorization = std::make_unique<PanelAuthorization>(_grants,
        [this](AdminRequest const& request) { return UserOf(request); },
        [this](AdminRequest const& request, std::string_view permission, std::string_view app, bool allowed)
        {
            AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "{} {} {}{} (request {})", request.Principal.empty() ? std::string("somebody") : request.Principal,
                allowed ? "used" : "was refused", permission, app.empty() ? std::string() : " on " + std::string(app), request.Id);
        });
    _listener.Routes().SetPermissionCheck([this](AdminRequest const& request, std::string_view permission)
    {
        return _authorization->Decide(request, permission);
    });
    _listener.Routes().SetPermissionKnown([](std::string_view permission) { return PanelPermissions::Holds(permission); });
    RegisterSignIn();
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

bool Panel::OpenStore(ConfigMgr const& config, std::string& error)
{
    std::lock_guard const lock(_storeMutex);
    if (_store.IsOpen())
        return true;
    std::vector<std::string> warnings;
    std::filesystem::path const file = StoreFile(config, _dataFolder);
    if (!_store.Open(file, Ambrose::FindSourceFolder(), warnings, error))
        return false;
    for (std::string const& warning : warnings)
        AMBROSE_LOG(_log, LogLevel::Warn, PanelCategory, "{}", warning);
    for (std::string const& applied : _store.GetApplied())
        AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "The panel store applied {}", applied);
    AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "The panel store is open in {}", ConfigMgr::PathToUtf8(file));
    return true;
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
    if (!OpenStore(config, error))
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
    if (!_listener.Start(settings, error))
        return false;
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
    if (settings.Enable && !_store.IsOpen())
    {
        std::string error;
        if (!OpenStore(config, error))
        {
            AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "The panel store could not be opened, so the panel stays as it is: {}", error);
            return false;
        }
    }
    _rateLimit.SetLimits(settings.RateLimitBurst, settings.RateLimitPerSecond);
    if (!_listener.Reload(settings))
        return false;
    _secure = settings.Enable && settings.HasTls();
    return true;
}

void Panel::Stop()
{
    StopGathering();
    _listener.Stop();
    _rateLimit.Clear();
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

void Panel::RegisterSignIn()
{
    AdminRouter& routes = _listener.Routes();
    routes.AddPublic("POST", "/api/panel/session", [this](AdminRequest const& request) { return SignIn(request); });
    routes.AddPublic("POST", "/api/panel/claim", [this](AdminRequest const& request) { return Claim(request); });
    routes.AddPublic("GET", "/api/panel/session", [this](AdminRequest const& request) { return Probe(request); });
    routes.AddPublic("GET", "/api/session", [this](AdminRequest const& request) { return Probe(request); });
    routes.AddPublic("POST", "/api/panel/reset", [this](AdminRequest const& request) { return Reset(request); });
    routes.AddOpen("DELETE", "/api/panel/session", [this](AdminRequest const& request) { return SignOut(request); });
    routes.AddOpen("GET", "/api/panel/me", [this](AdminRequest const& request) { return WhoAmI(request); });
    routes.AddOpen("GET", "/api/panel/permissions", [](AdminRequest const&) { return AdminResponse::Json(200, PanelPermissions::CatalogJson()); });
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
        return body;
    }
}

nlohmann::json Panel::UserAnswer(PanelUser const& user)
{
    nlohmann::json body = UserJson(user);
    nlohmann::json grants = nlohmann::json::object();
    std::string error;
    for (PanelGrant const& grant : _grants.Of(user.Id, error))
        grants[grant.App].push_back(grant.Permission);
    if (!error.empty())
        AMBROSE_LOG(_log, LogLevel::Error, PanelCategory, "The panel could not read {}'s grants: {}", user.Username, error);
    body["grants"] = std::move(grants);
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

    _signIn.Succeeded(username, address);
    return OpenFor(user, request, "a username and password");
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

AdminResponse Panel::OpenFor(PanelUser const& user, AdminRequest const& request, std::string_view how)
{
    std::string error;
    std::optional<PanelSessionOpened> opened = _sessions.Open(user.Id, user.Generation, request.RemoteAddress, request.UserAgent, error);
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
    _signIn.Succeeded(user->Username, request.RemoteAddress);
    AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "{} set a password from a one-time link; every other session that operator had has ended", user->Username);
    return OpenFor(*user, request, "a one-time password link");
}
