/*
 * Project Ambrose by Imjustchico
 * The server group: what an operator asks a running server about itself, and what the server says to everyone in it. 'server info' is the one a console reaches for first, saying which build is running, how long it has been up, how many times the world has ticked and how many sessions it holds, and it is open to any level because none of it is a secret to somebody already signed in. 'server announce' shows a line of text to every wizard in the world through MSG_SERVERMESSAGE, which the client shows as a server message, and says how many it reached.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "Duration.h"
#include "GameSession.h"
#include "GitRevision.h"
#include "ScriptMgr.h"
#include "Utf.h"
#include "World.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>

namespace
{
    class ServerCommands : public CommandScript
    {
    public:
        ServerCommands() : CommandScript("cs_server") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "server", .SecurityLevel = SEC_PLAYER, .Help = "what this server is and what it is doing", .Children = {
                    { .Name = "info", .SecurityLevel = SEC_PLAYER, .Help = "the build, the uptime, the ticks and the sessions", .Run = Info },
                    { .Name = "announce", .SecurityLevel = SEC_GAMEMASTER, .Help = "show a message to every wizard in the world", .Run = Announce },
                } },
            };
        }

    private:
        static bool Info(CommandCaller& caller, std::vector<std::string> const&)
        {
            caller.Reply(fmt::format("Project Ambrose {} on {}", GitRevision::GetHash(), GitRevision::GetBranch()));
            caller.Reply(fmt::format("The world has ticked {} time(s) and holds {} session(s)", sWorld.GetTickCount(), sWorld.GetSessionCount()));
            return true;
        }

        static bool Announce(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.empty())
            {
                caller.Reply("Give the message to show");
                return false;
            }
            std::string const text = fmt::format("{}", fmt::join(arguments, " "));
            std::optional<std::u16string> const wide = Utf::Utf8ToUtf16(text, Utf::InvalidPolicy::Reject);
            if (!wide)
            {
                caller.Reply("The message is not valid UTF-8");
                return false;
            }
            std::size_t shown = 0;
            for (std::shared_ptr<GameSession> const& session : sWorld.GetSessions())
            {
                SessionStatus const status = session->GetStatus();
                if ((status == SessionStatus::LoggedIn || status == SessionStatus::InWorld) && session->SendServerMessage(*wide))
                    ++shown;
            }
            caller.Reply(fmt::format("Shown to {} wizard(s) in the world", shown));
            return true;
        }
    };
}

void AddSC_cs_server()
{
    new ServerCommands();
}
