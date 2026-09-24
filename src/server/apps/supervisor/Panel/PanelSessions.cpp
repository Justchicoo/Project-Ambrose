/*
 * Project Ambrose by Imjustchico
 * Opens a session by handing the browser a random secret and keeping only its SHA-256, so a stolen store file signs nobody in; the CSRF token is derived from that same secret rather than stored, so it cannot leak from the store either and a caller who cannot read the cookie cannot compute it; and holding a session checks in one statement that it has not ended, has not passed either expiry, and still carries the generation its user has now, so a password change or a disable stops every session that user had without hunting for them.
 */

#include "PanelSessions.h"
#include "Base64.h"
#include "CryptoRandom.h"
#include "SHA256.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <utility>

namespace
{
    std::string Secret()
    {
        std::array<uint8, PanelSessions::SecretBytes> const bytes = Ambrose::Crypto::GetRandomArray<PanelSessions::SecretBytes>();
        return Base64::Encode(bytes, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
    }

    std::string CsrfOf(std::string_view secret)
    {
        return Base64::Encode(SHA256::GetDigestOf(std::string(secret) + ":csrf"), Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
    }
}

PanelSessions::PanelSessions(PanelStore& store) : _store(store)
{
}

std::string PanelSessions::Digest(std::string_view text)
{
    return Base64::Encode(SHA256::GetDigestOf(text), Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
}

void PanelSessions::SetLifetimes(std::chrono::seconds idle, std::chrono::seconds absolute)
{
    std::lock_guard const lock(_mutex);
    _idle = idle;
    _absolute = absolute;
}

std::optional<PanelSessionOpened> PanelSessions::Open(int64 userId, int64 generation, std::string_view address, std::string_view userAgent, std::string& error)
{
    std::lock_guard const lock(_mutex);
    PanelSessionOpened opened;
    opened.Secret = Secret();
    opened.Csrf = CsrfOf(opened.Secret);
    opened.Id = Digest(opened.Secret).substr(0, 16);

    std::optional<PanelStore::Statement> insert = _store.Prepare(
        "INSERT INTO panel_session (id, token_hash, user_id, generation, created_epoch_ms, seen_epoch_ms, idle_expires_epoch_ms, absolute_expires_epoch_ms, address, user_agent)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", error);
    if (!insert)
        return std::nullopt;
    int64 const now = PanelStore::NowEpochMs();
    insert->Bind(1, opened.Id);
    insert->Bind(2, Digest(opened.Secret));
    insert->Bind(3, userId);
    insert->Bind(4, generation);
    insert->Bind(5, now);
    insert->Bind(6, now);
    insert->Bind(7, now + std::chrono::milliseconds(_idle).count());
    insert->Bind(8, now + std::chrono::milliseconds(_absolute).count());
    if (address.empty())
        insert->BindNull(9);
    else
        insert->Bind(9, address);
    if (userAgent.empty())
        insert->BindNull(10);
    else
        insert->Bind(10, userAgent);
    if (!insert->Run(error))
        return std::nullopt;
    return opened;
}

std::optional<SessionHolder> PanelSessions::Hold(std::string_view secret)
{
    if (secret.empty() || secret.size() > 512)
        return std::nullopt;

    std::lock_guard const lock(_mutex);
    std::string error;
    std::optional<PanelStore::Statement> rows = _store.Prepare(
        "SELECT s.id, s.user_id, s.absolute_expires_epoch_ms FROM panel_session s JOIN panel_user u ON u.id = s.user_id"
        " WHERE s.token_hash = ? AND s.ended_epoch_ms IS NULL AND s.idle_expires_epoch_ms > ? AND s.absolute_expires_epoch_ms > ?"
        " AND s.generation = u.generation AND u.disabled = 0", error);
    if (!rows)
        return std::nullopt;
    int64 const now = PanelStore::NowEpochMs();
    rows->Bind(1, Digest(secret));
    rows->Bind(2, now);
    rows->Bind(3, now);
    if (!rows->Step(error))
        return std::nullopt;
    std::string const id = rows->Text(0);
    int64 const userId = rows->Int64(1);
    int64 const absolute = rows->Int64(2);
    rows.reset();

    std::optional<PanelStore::Statement> touch = _store.Prepare("UPDATE panel_session SET seen_epoch_ms = ?, idle_expires_epoch_ms = ? WHERE id = ?", error);
    if (touch)
    {
        int64 const idle = now + std::chrono::milliseconds(_idle).count();
        touch->Bind(1, now);
        touch->Bind(2, std::min(idle, absolute));
        touch->Bind(3, id);
        touch->Run(error);
    }
    return SessionHolder{ CsrfOf(secret), fmt::format("user:{}", userId) };
}

bool PanelSessions::Close(std::string_view secret, std::string_view reason, std::string& error)
{
    std::lock_guard const lock(_mutex);
    std::optional<PanelStore::Statement> update = _store.Prepare(
        "UPDATE panel_session SET ended_epoch_ms = ?, ended_reason = ? WHERE token_hash = ? AND ended_epoch_ms IS NULL", error);
    if (!update)
        return false;
    update->Bind(1, PanelStore::NowEpochMs());
    update->Bind(2, reason);
    update->Bind(3, Digest(secret));
    if (!update->Run(error))
        return false;
    return _store.Changed() > 0;
}

bool PanelSessions::CloseEveryOne(int64 userId, std::string_view reason, std::string& error)
{
    std::lock_guard const lock(_mutex);
    std::optional<PanelStore::Statement> update = _store.Prepare(
        "UPDATE panel_session SET ended_epoch_ms = ?, ended_reason = ? WHERE user_id = ? AND ended_epoch_ms IS NULL", error);
    if (!update)
        return false;
    update->Bind(1, PanelStore::NowEpochMs());
    update->Bind(2, reason);
    update->Bind(3, userId);
    return update->Run(error);
}

bool PanelSessions::DropExpired(std::string& error)
{
    std::lock_guard const lock(_mutex);
    std::optional<PanelStore::Statement> drop = _store.Prepare(
        "DELETE FROM panel_session WHERE ended_epoch_ms IS NOT NULL OR absolute_expires_epoch_ms <= ? OR idle_expires_epoch_ms <= ?", error);
    if (!drop)
        return false;
    int64 const now = PanelStore::NowEpochMs();
    drop->Bind(1, now);
    drop->Bind(2, now);
    return drop->Run(error);
}

std::vector<PanelSessionInfo> PanelSessions::List(int64 userId, std::string& error)
{
    std::lock_guard const lock(_mutex);
    std::vector<PanelSessionInfo> sessions;
    std::optional<PanelStore::Statement> rows = _store.Prepare(
        "SELECT id, user_id, generation, created_epoch_ms, seen_epoch_ms, idle_expires_epoch_ms, absolute_expires_epoch_ms, COALESCE(address, ''), COALESCE(user_agent, '')"
        " FROM panel_session WHERE ended_epoch_ms IS NULL AND (? = 0 OR user_id = ?) ORDER BY seen_epoch_ms DESC", error);
    if (!rows)
        return sessions;
    rows->Bind(1, userId);
    rows->Bind(2, userId);
    while (rows->Step(error))
    {
        PanelSessionInfo session;
        session.Id = rows->Text(0);
        session.UserId = rows->Int64(1);
        session.Generation = rows->Int64(2);
        session.CreatedEpochMs = rows->Int64(3);
        session.SeenEpochMs = rows->Int64(4);
        session.IdleExpiresEpochMs = rows->Int64(5);
        session.AbsoluteExpiresEpochMs = rows->Int64(6);
        session.Address = rows->Text(7);
        session.UserAgent = rows->Text(8);
        sessions.push_back(std::move(session));
    }
    return sessions;
}

std::size_t PanelSessions::Count(std::string& error)
{
    std::lock_guard const lock(_mutex);
    std::optional<PanelStore::Statement> rows = _store.Prepare("SELECT COUNT(*) FROM panel_session WHERE ended_epoch_ms IS NULL", error);
    if (!rows || !rows->Step(error))
        return 0;
    return static_cast<std::size_t>(rows->Int64(0));
}
