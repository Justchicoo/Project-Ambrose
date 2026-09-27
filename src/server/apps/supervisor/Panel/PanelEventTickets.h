/*
 * Project Ambrose by Imjustchico
 * The one-time tickets a script trades its API credential for before it opens the panel's event socket, so the credential never travels in a socket's first frame and a ticket never in an address: 32 random bytes written as base64url, kept only as their SHA-256, good for 30 seconds and one use, and bound to the caller that asked, the address it asked from and the apps it named. Any attempt spends a ticket, the right one or not, a ticket seen in an address is burned without being used, and at most 16 wait per caller, the oldest going first, so a caller cannot fill the supervisor's memory with them. The time source is injected so a test can stand at the edge of the window.
 */

#ifndef AMBROSE_PANELEVENTTICKETS_H
#define AMBROSE_PANELEVENTTICKETS_H

#include "SHA256.h"
#include "Types.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct PanelTicket
{
    std::string Principal = {};
    std::string Address = {};
    std::vector<std::string> Apps = {};
};

class PanelEventTickets
{
public:
    using Clock = std::chrono::steady_clock;
    using TimeSource = std::function<Clock::time_point()>;

    static constexpr std::chrono::seconds Lifetime{ 30 };
    static constexpr std::size_t MaxPerPrincipal = 16;
    static constexpr std::size_t TicketBytes = 32;

    explicit PanelEventTickets(TimeSource timeSource = [] { return Clock::now(); });

    PanelEventTickets(PanelEventTickets const&) = delete;
    PanelEventTickets& operator=(PanelEventTickets const&) = delete;

    std::string Mint(std::string_view principal, std::string_view address, std::vector<std::string> apps);
    std::optional<PanelTicket> Redeem(std::string_view ticket, std::string_view address);
    void Burn(std::string_view ticket);
    std::size_t Outstanding(std::string_view principal) const;
    void Clear();

private:
    struct Entry
    {
        PanelTicket Ticket = {};
        Clock::time_point Minted = {};
        Clock::time_point Expires = {};
    };

    void DropExpired(Clock::time_point now);

    mutable std::mutex _mutex;
    TimeSource _timeSource;
    std::map<SHA256::Digest, Entry> _tickets;
};

#endif
