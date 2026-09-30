/*
 * Project Ambrose by Imjustchico
 * The network group: 'network sessions' counts the sockets open on this server and the sessions the world holds by the status each has reached, 'network session <id>' shows one session's status, keepalive round trip, queued messages and strikes, and 'network packetlog on|off|filter <names>|suppress <names>' sets the packet log's live settings as '.settings set' would, recorded under the caller's name, so it applies from the next message without a restart.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "GameSession.h"
#include "NetworkHooks.h"
#include "ScriptMgr.h"
#include "SessionStatus.h"
#include "Settings.h"
#include "World.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <charconv>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    class NetworkCommands : public CommandScript
    {
    public:
        NetworkCommands() : CommandScript("cs_network") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "network", .SecurityLevel = SEC_GAMEMASTER, .Help = "the sessions this server holds and the packet log", .Children = {
                    { .Name = "sessions", .SecurityLevel = SEC_GAMEMASTER, .Help = "how many sessions are open, by status", .Run = Sessions },
                    { .Name = "session", .SecurityLevel = SEC_GAMEMASTER, .Help = "one session's status, round trip, queue and strikes", .Run = Session },
                    { .Name = "packetlog", .SecurityLevel = SEC_ADMINISTRATOR, .Help = "on, off, filter <names> or suppress <names>", .Run = PacketLogCommand },
                } },
            };
        }

    private:
        static bool Sessions(CommandCaller& caller, std::vector<std::string> const&)
        {
            std::vector<std::shared_ptr<GameSession>> const sessions = sWorld.GetSessions();
            std::map<std::string_view, std::size_t> byStatus;
            for (std::shared_ptr<GameSession> const& session : sessions)
                ++byStatus[SessionStatuses::GetName(session->GetStatus())];
            std::vector<std::string> parts;
            for (auto const& [status, count] : byStatus)
                parts.push_back(fmt::format("{} {}", count, status));
            caller.Reply(fmt::format("{} socket(s) open on this server; the world holds {} session(s){}{}", NetworkHooks::OpenSessions(), sessions.size(),
                parts.empty() ? "" : ": ", fmt::join(parts, ", ")));
            return true;
        }

        static bool Session(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            uint32 id = 0;
            std::string_view const text = arguments.empty() ? std::string_view() : std::string_view(arguments.front());
            auto const [end, error] = std::from_chars(text.data(), text.data() + text.size(), id);
            if (text.empty() || error != std::errc() || end != text.data() + text.size() || id == 0 || id > 65535)
            {
                caller.Reply("Give the session id, a number from 1 to 65535");
                return false;
            }
            for (std::shared_ptr<GameSession> const& session : sWorld.GetSessions())
            {
                if (session->GetSessionId() != id)
                    continue;
                caller.Reply(fmt::format("Session {} is {}, keepalive round trip {} ms, {} queued message(s), {} strike(s), {} rate limit violation(s)", id,
                    SessionStatuses::GetName(session->GetStatus()), session->GetKeepAliveRoundTrip().count(), session->GetQueuedMessageCount(), session->GetStrikes(),
                    session->GetRateLimitViolations()));
                return true;
            }
            caller.Reply(fmt::format("The world holds no session {}", id));
            return false;
        }

        static bool Apply(CommandCaller& caller, std::string_view key, std::string_view value)
        {
            SettingAuthor const author{ caller.GetName(), 0, caller.IsConsole() ? std::string("console") : std::string("game") };
            SettingOutcome const outcome = sSettings.Set(key, value, author, "network packetlog");
            if (outcome.Result != SettingResult::Ok && outcome.Result != SettingResult::Unchanged)
            {
                caller.Reply(fmt::format("{} was not changed: {}", key, outcome.Message));
                return false;
            }
            return true;
        }

        static bool PacketLogCommand(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            std::string_view const action = arguments.empty() ? std::string_view() : std::string_view(arguments.front());
            std::string const names = arguments.size() > 1 ? fmt::format("{}", fmt::join(arguments.begin() + 1, arguments.end(), ",")) : std::string();
            if (action == "on" || action == "off")
            {
                if (!Apply(caller, "Network.PacketLog.Enable", action == "on" ? "true" : "false"))
                    return false;
                caller.Reply(fmt::format("The packet log is {} from the next message", action));
                return true;
            }
            if (action == "filter" || action == "suppress")
            {
                std::string_view const key = action == "filter" ? "Network.PacketLog.Filter" : "Network.PacketLog.Suppress";
                if (!Apply(caller, key, names))
                    return false;
                caller.Reply(names.empty() ? fmt::format("{} is cleared from the next message", key) : fmt::format("{} is {} from the next message", key, names));
                return true;
            }
            caller.Reply("Say on, off, filter <names> or suppress <names>; filter with no names keeps every message");
            return false;
        }
    };
}

void AddSC_cs_network()
{
    new NetworkCommands();
}
