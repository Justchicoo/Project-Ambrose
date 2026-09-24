/*
 * Project Ambrose by Imjustchico
 * The browsers a panel user is signed in on, kept in the supervisor's own store rather than in memory, so restarting the supervisor does not sign every operator out: each row holds only the SHA-256 of the cookie's secret and of its CSRF token, the user it belongs to and the generation that user had when it opened, its idle and absolute expiry, and where it was opened from; a session stops being believed the moment its user's generation moves, which is how a password change or a disable ends every other session that user has.
 */

#ifndef AMBROSE_PANELSESSIONS_H
#define AMBROSE_PANELSESSIONS_H

#include "AdminSessions.h"
#include "PanelStore.h"
#include "Types.h"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct PanelSessionOpened
{
    std::string Secret;
    std::string Csrf;
    std::string Id;
};

struct PanelSessionInfo
{
    std::string Id;
    int64 UserId = 0;
    int64 Generation = 0;
    int64 CreatedEpochMs = 0;
    int64 SeenEpochMs = 0;
    int64 IdleExpiresEpochMs = 0;
    int64 AbsoluteExpiresEpochMs = 0;
    std::string Address;
    std::string UserAgent;
};

class PanelSessions : public SessionSource
{
public:
    static constexpr std::size_t SecretBytes = 32;
    static constexpr std::chrono::minutes DefaultIdle{ 720 };
    static constexpr std::chrono::hours DefaultLifetime{ 168 };

    explicit PanelSessions(PanelStore& store);

    PanelSessions(PanelSessions const&) = delete;
    PanelSessions& operator=(PanelSessions const&) = delete;

    void SetLifetimes(std::chrono::seconds idle, std::chrono::seconds absolute);

    std::optional<PanelSessionOpened> Open(int64 userId, int64 generation, std::string_view address, std::string_view userAgent, std::string& error);
    std::optional<SessionHolder> Hold(std::string_view secret) override;

    bool Close(std::string_view secret, std::string_view reason, std::string& error);
    bool CloseEveryOne(int64 userId, std::string_view reason, std::string& error);
    bool DropExpired(std::string& error);

    std::vector<PanelSessionInfo> List(int64 userId, std::string& error);
    std::size_t Count(std::string& error);

private:
    static std::string Digest(std::string_view text);

    PanelStore& _store;
    std::mutex _mutex;
    std::chrono::seconds _idle{ DefaultIdle };
    std::chrono::seconds _absolute{ DefaultLifetime };
};

#endif
