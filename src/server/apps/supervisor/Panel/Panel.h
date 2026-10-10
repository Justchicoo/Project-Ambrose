/*
 * Project Ambrose by Imjustchico
 * The panel's own front door in the supervisor: a second listener with its own Panel options, its own token file, its own store and its own keyring, off unless Panel.Enable is set, holding its operators and their sessions, the single-use links that make the first owner, set a password or open a session from a desktop program, kept only as hashes so they survive a restart, the counts a failed sign-in, a wrong second factor or a wrong link adds to, the cost-weighted limit every costly route is held to and the audit tables every change is recorded in, relayed settings changes, batches, reloads, secret reveals and error report creation among them, the protected path patterns an owner adds to a file root, saved with their audit row, and the supervisor's own live settings with their history, signing an operator with two-factor sign-in in only after a password or a link and a code, holding every route and socket to the two-factor requirement an owner sets while leaving open the routes that meet it, and asking for a fresh check before a danger action, bound to this machine unless a certificate and key are given or the operator opts into plain HTTP, serving the built dashboard at / and the panel's API under /api/panel/, the one event socket every live page runs on at /api/panel/events with the streams it serves and the one-time tickets a script opens it with, issuing local and pairing links through the supervisor's admin API, its console and its command line alike, and reloaded with the rest of the configuration so a bind it would not be allowed to keep, or a two-factor requirement it does not know, is refused while the old one goes on serving. It also sends the mail settings' test mail to the signed-in operator alone and holds sign-in to a captcha after repeated failures when one is configured, failing closed.
 */

#ifndef AMBROSE_PANEL_H
#define AMBROSE_PANEL_H

#include "AdminServer.h"
#include "ListenerSettings.h"
#include "PanelAudit.h"
#include "PanelAuditForwarder.h"
#include "PanelRateLimit.h"
#include "PanelErrors.h"
#include "PanelFileRules.h"
#include "PanelGrants.h"
#include "PanelAuthorization.h"
#include "PanelEventSocket.h"
#include "PanelEventStreams.h"
#include "PanelEventTickets.h"
#include "PanelKeyring.h"
#include "PanelLinks.h"
#include "PanelMaintenance.h"
#include "PanelPublicStatus.h"
#include "PanelSessions.h"
#include "PanelSignIn.h"
#include "PanelTwoFactor.h"
#include "PanelUsers.h"
#include "PanelStore.h"
#include "PanelSettings.h"
#include "Types.h"

#include <nlohmann/json_fwd.hpp>

#include <chrono>
#include <filesystem>
#include <map>
#include <condition_variable>
#include <memory>
#include <thread>
#include <utility>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ConfigMgr;
class EmbeddedPage;
class Log;
class PanelSettingStore;
class SettingStore;

class Panel
{
public:
    static constexpr uint16 DefaultPort = 12080;
    static constexpr std::string_view Prefix = "/api/panel";
    static constexpr std::string_view LinksPath = "/api/panel-links";
    static constexpr std::string_view LinkPath = "/api/panel/link";
    static constexpr std::string_view ChallengeCookie = "_challenge";
    static constexpr std::size_t MaxChallenges = 1024;
    static constexpr uint32 PasswordCheckCost = 10;

    Panel(Log& log, std::filesystem::path dataFolder, std::filesystem::path configFolder = {});

    Panel(Panel const&) = delete;
    Panel& operator=(Panel const&) = delete;

    static ListenerSettings LoadSettings(ConfigMgr const& config, std::vector<std::string>* problems = nullptr);
    static std::filesystem::path StoreFile(ConfigMgr const& config, std::filesystem::path const& dataFolder);
    static std::filesystem::path KeyringFile(ConfigMgr const& config, std::filesystem::path const& dataFolder);

    void SetDashboard(EmbeddedPage const* page);
    bool Start(ConfigMgr const& config, std::string& error);
    bool Reload(ConfigMgr const& config);
    void Stop();

    bool IsRunning() const { return _listener.IsRunning(); }
    uint16 GetPort() const { return _listener.GetPort(); }
    std::string GetBindIp() const { return _listener.GetBindIp(); }
    std::string GetToken() const { return _listener.GetToken(); }
    bool IsSecure() const { return _secure; }
    std::string GetFingerprint() const { return _listener.GetFingerprint(); }
    std::string MintPasswordLink(int64 userId);
    std::string LinkFor(std::string_view page, std::string_view token) const;
    static std::string PairingLine(std::string_view host, uint16 port, std::string_view token, std::string_view fingerprint);
    std::optional<PanelLinkIssued> IssueLink(PanelLinkAsk const& ask, PanelLinkIssuer const& issuer, PanelLinkRefusal& refusal);
    PanelUserResult MakeOperator(std::string_view username, PanelLinkIssuer const& by, int64& id, std::string& error);
    void RegisterAdminRoutes(AdminRouter& routes);

    PanelStore& Store() { return _store; }
    PanelSettings& Settings() { return _settings; }
    PanelUsers& Users() { return _users; }
    PanelSessions& Sessions() { return _sessions; }
    PanelErrors& Errors() { return _errors; }
    PanelGrants& Grants() { return _grants; }
    PanelKeyring& Keyring() { return _keyring; }
    PanelTwoFactor& TwoFactor() { return _twoFactor; }
    PanelTwoFactorSettings TwoFactorSettings() const;
    PanelFileRules& FileRules() { return _fileRules; }
    PanelLinks& Links() { return _links; }
    std::shared_ptr<SettingStore> LiveSettingStore();
    bool IsStoreOpen();
    bool ReadFileRules(std::map<std::string, std::vector<std::string>, std::less<>>& rules, std::string& error);
    bool SaveFileRules(AdminRequest const& request, AuditEvent const& event, std::string const& root, std::vector<std::string> const& patterns, std::string& error);
    void SetErrorSource(std::function<std::vector<std::pair<std::string, std::string>>()> source);
    std::size_t GatherErrorsOnce();

    static constexpr std::chrono::seconds GatherInterval{ 30 };
    PanelSignInThrottle& SignInThrottle() { return _signIn; }
    PanelSignInThrottle& SecondFactorThrottle() { return _secondFactor; }
    PanelSignInThrottle& LinkThrottle() { return _linkFailures; }
    PanelRateLimit& Limit() { return _rateLimit; }
    AdminRouter& Routes() { return _listener.Routes(); }
    PanelEventStreams& Events() { return _events; }
    PanelEventTickets& Tickets() { return _tickets; }
    PanelEventSocket& EventSocket() { return *_eventSocket; }
    void SetAppSource(PanelEventSocket::AppSource source);
    void SetAppCall(MaintenanceAppCall call) { _appCall = std::move(call); }
    void SetPublicSource(PublicStatusSource source) { _publicSource = std::move(source); }
    void AddSocket(AdminSocketRoute route) { _listener.AddSocket(std::move(route)); }

    bool Record(AuditEvent const& event, std::function<bool(std::string& error)> const& change, std::string& error);
    bool Record(AuditEvent& event, std::function<bool(AuditEvent& event, std::string& error)> const& change, std::string& error);
    bool VerifyAuditChain(AuditChainVerification& verification, std::string& error);
    AdminResponse AuditRequest(AdminRequest const& request, std::string_view app, std::string_view action, std::function<AdminResponse()> operation);
    uint8 CommandLevel(AdminRequest const& request);
    std::string CommandActorName(AdminRequest const& request);
    bool StoreCommandHistoryWithinAudit(AdminRequest const& request, std::string_view app, std::string_view command, std::string& error);
    AdminResponse CommandHistoryGet(AdminRequest const& request);
    std::string NameOf(AdminRequest const& request);
    void RecordRelayed(AdminRequest const& request, std::string_view app, std::string_view method, std::string_view path, int status, std::string const& body);
    void RecordReveal(AdminRequest const& request, std::string_view app, std::vector<std::string> const& keys);
    bool ResetTwoFactor(PanelUser const& user, std::string_view actor, std::string& error);

    std::optional<AdminResponse> Admit(AdminRequest const& request);
    std::optional<AdminResponse> StepUpCheck(AdminRequest const& request, std::string_view permission, StepUpWhen when);

private:
    struct Challenge
    {
        int64 UserId = 0;
        std::string Username = {};
        std::string How = {};
        std::chrono::steady_clock::time_point Expires = {};
        uint32 Attempts = 0;
        std::string LinkId = {};
        std::string LinkKind = {};
    };

    struct LinkUsed
    {
        std::string Id = {};
        std::string Kind = {};
    };

    bool OpenStore(ConfigMgr const& config, std::string& error);
    bool OpenKeyring(ConfigMgr const& config, std::string& error);
    std::optional<PanelTwoFactorSettings> LoadTwoFactorSettings(ConfigMgr const& config, std::string& error);
    void ApplyTwoFactorSettings(ConfigMgr const& config, PanelTwoFactorSettings const& loaded);
    void RegisterSignIn();
    void RegisterTwoFactor();
    void RegisterCommandHistory();
    void RegisterMaintenance();
    AdminResponse MaintenanceGet(AdminRequest const& request);
    AdminResponse MaintenanceEnter(AdminRequest const& request);
    AdminResponse MaintenanceExit(AdminRequest const& request);
    void RegisterPublicStatus();
    AdminResponse PublicStatusGet(AdminRequest const& request);
    AdminResponse IncidentPost(AdminRequest const& request);
    AdminResponse IncidentClear(AdminRequest const& request);
    void OfferTheOwnerLink();
    AdminResponse Claim(AdminRequest const& request);
    AdminResponse Probe(AdminRequest const& request);
    AdminResponse Reset(AdminRequest const& request);
    AdminResponse TradeLink(AdminRequest const& request);
    AdminResponse MintLinkRoute(AdminRequest const& request);
    AdminResponse OpenFor(PanelUser const& user, AdminRequest const& request, std::string_view how, nlohmann::json const* properties = nullptr, LinkUsed const* link = nullptr);
    AdminResponse Challenged(PanelUser const& user, AdminRequest const& request, std::string_view how, LinkUsed const* link = nullptr);
    bool Transact(std::function<bool(std::string& error)> const& change, std::string& error);
    bool InsertOperator(PanelUserDraft const& draft, PanelLinkIssuer const& by, std::string_view reason, int64& id, PanelUserResult& made, std::string& error);
    bool PlainBeyondLoopback() const;
    bool BindsOneRemoteAddress() const;
    std::optional<AdminResponse> LinkHeldBack(AdminRequest const& request);
    AdminResponse LinkRefused(AdminRequest const& request, PanelLink const* link, std::optional<PanelUser> const& user, int status, std::string_view code, std::string_view message, std::string_view reason);
    AdminResponse SignIn(AdminRequest const& request);
    AdminResponse SecondFactor(AdminRequest const& request);
    AdminResponse SignOut(AdminRequest const& request);
    AdminResponse WhoAmI(AdminRequest const& request);
    AdminResponse TwoFactorGet(AdminRequest const& request);
    AdminResponse TwoFactorSetup(AdminRequest const& request);
    AdminResponse TwoFactorEnable(AdminRequest const& request);
    AdminResponse TwoFactorDisable(AdminRequest const& request);
    AdminResponse RecoveryCodes(AdminRequest const& request);
    AdminResponse StepUpRoute(AdminRequest const& request);
    AdminResponse PanelSettingsGet(AdminRequest const& request);
    AdminResponse PanelSettingsUpdate(AdminRequest const& request);
    AdminResponse MailTest(AdminRequest const& request);
    std::optional<AdminResponse> CaptchaGate(AdminRequest const& request, nlohmann::json const& body, std::string_view username);
    AdminResponse ClearError(AdminRequest const& request);
    AdminResponse ErrorReport(AdminRequest const& request, bool create);
    std::optional<PanelUser> UserOf(AdminRequest const& request);
    nlohmann::json UserAnswer(PanelUser const& user);
    std::string Issuer() const;
    bool Required(PanelUser const& user);
    std::optional<AdminResponse> Throttle(AdminRequest const& request, uint32 cost);
    std::optional<AdminResponse> HeldBack(PanelSignInThrottle& throttle, std::string_view username, std::string_view address, std::string_view counted);
    PanelSecondFactor CheckSecondFactor(PanelUser const& user, nlohmann::json const& body, std::string& method, std::string& error);
    void RecordRefused(std::string_view name, PanelUser const& user, AdminRequest const& request, std::string_view reason);

    Log& _log;
    std::filesystem::path _dataFolder;
    PanelStore _store;
    PanelSettings _settings;
    PanelUsers _users;
    PanelSessions _sessions;
    PanelErrors _errors;
    PanelGrants _grants;
    PanelKeyring _keyring;
    PanelTwoFactor _twoFactor;
    PanelFileRules _fileRules;
    std::unique_ptr<PanelAuthorization> _authorization;
    PanelSignInThrottle _signIn;
    PanelSignInThrottle _secondFactor;
    PanelSignInThrottle _linkFailures;
    PanelLinks _links;
    std::chrono::seconds _sessionIdle{ 0 };
    std::chrono::seconds _sessionLifetime{ 0 };
    std::mutex _challengeMutex;
    std::map<std::string, Challenge, std::less<>> _challenges;
    mutable std::mutex _twoFactorMutex;
    PanelTwoFactorSettings _twoFactorSettings;
    PanelRateLimit _rateLimit;
    std::mutex _storeMutex;
    PanelAuditForwarder _auditForwarder;
    bool _auditForwarding = false;
    std::shared_ptr<PanelSettingStore> _settingStore;
    void StartGathering();
    void StopGathering();

    std::function<std::vector<std::pair<std::string, std::string>>()> _errorSource;
    std::thread _gatherThread;
    std::mutex _gatherMutex;
    std::condition_variable _gatherWake;
    bool _gathering = false;
    PanelEventStreams _events;
    PanelEventTickets _tickets;
    std::unique_ptr<PanelEventSocket> _eventSocket;
    MaintenanceAppCall _appCall;
    PublicStatusSource _publicSource;
    PublicStatusCache _publicStatusCache;
    bool _publicStatusEnabled = false;
    AdminServer _listener;
    bool _secure = false;
};

#endif
