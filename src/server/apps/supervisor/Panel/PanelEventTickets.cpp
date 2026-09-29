/*
 * Project Ambrose by Imjustchico
 * Mints, spends and burns the event socket's tickets under one lock: expired ones are dropped whenever the table is touched, a caller at the cap loses its oldest before a new one is kept, and redeeming removes the ticket before anything about it is checked, so a second attempt, a late one and one from another address all find nothing.
 */

#include "PanelEventTickets.h"
#include "Base64.h"
#include "CryptoRandom.h"

#include <array>
#include <utility>

PanelEventTickets::PanelEventTickets(TimeSource timeSource) : _timeSource(std::move(timeSource))
{
}

void PanelEventTickets::DropExpired(Clock::time_point now)
{
    std::erase_if(_tickets, [now](auto const& entry) { return now >= entry.second.Expires; });
}

std::string PanelEventTickets::Mint(std::string_view principal, std::string_view address, std::vector<std::string> apps)
{
    std::array<uint8, TicketBytes> const bytes = Ambrose::Crypto::GetRandomArray<TicketBytes>();
    std::string ticket = Base64::Encode(bytes, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
    std::lock_guard const lock(_mutex);
    Clock::time_point const now = _timeSource();
    DropExpired(now);
    std::size_t held = 0;
    auto oldest = _tickets.end();
    for (auto entry = _tickets.begin(); entry != _tickets.end(); ++entry)
    {
        if (entry->second.Ticket.Principal != principal)
            continue;
        ++held;
        if (oldest == _tickets.end() || entry->second.Minted < oldest->second.Minted)
            oldest = entry;
    }
    if (held >= MaxPerPrincipal && oldest != _tickets.end())
        _tickets.erase(oldest);
    Entry entry;
    entry.Ticket = PanelTicket{ std::string(principal), std::string(address), std::move(apps) };
    entry.Minted = now;
    entry.Expires = now + Lifetime;
    _tickets.insert_or_assign(SHA256::GetDigestOf(std::string_view(ticket)), std::move(entry));
    return ticket;
}

std::optional<PanelTicket> PanelEventTickets::Redeem(std::string_view ticket, std::string_view address)
{
    SHA256::Digest const digest = SHA256::GetDigestOf(ticket);
    std::lock_guard const lock(_mutex);
    Clock::time_point const now = _timeSource();
    auto const found = _tickets.find(digest);
    if (found == _tickets.end())
    {
        DropExpired(now);
        return std::nullopt;
    }
    Entry const entry = std::move(found->second);
    _tickets.erase(found);
    DropExpired(now);
    if (now >= entry.Expires || entry.Ticket.Address != address)
        return std::nullopt;
    return entry.Ticket;
}

void PanelEventTickets::Burn(std::string_view ticket)
{
    SHA256::Digest const digest = SHA256::GetDigestOf(ticket);
    std::lock_guard const lock(_mutex);
    _tickets.erase(digest);
}

std::size_t PanelEventTickets::Outstanding(std::string_view principal) const
{
    std::lock_guard const lock(_mutex);
    Clock::time_point const now = _timeSource();
    std::size_t count = 0;
    for (auto const& entry : _tickets)
        if (entry.second.Ticket.Principal == principal && now < entry.second.Expires)
            ++count;
    return count;
}

void PanelEventTickets::Clear()
{
    std::lock_guard const lock(_mutex);
    _tickets.clear();
}
