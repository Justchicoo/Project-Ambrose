/*
 * Project Ambrose by Imjustchico
 * Commands that belong to no group. 'kick' disconnects a wizard in the world, found by its character id or by its name as its client shows it, with MSG_FORCE_DISCONNECT giving the reason CSR, so the client says a game master disconnected it rather than that its connection was lost; a name two wizards share names neither, and the ids to choose between are listed instead.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "DisconnectReason.h"
#include "GameSession.h"
#include "ScriptMgr.h"
#include "World.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <memory>
#include <string>
#include <vector>

namespace
{
    class MiscCommands : public CommandScript
    {
    public:
        MiscCommands() : CommandScript("cs_misc") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "kick", .SecurityLevel = SEC_GAMEMASTER, .Help = "disconnect a wizard in the world, named by character id or by name", .Run = Kick },
            };
        }

    private:
        static bool Kick(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.empty())
            {
                caller.Reply("Give the character id or the name of the wizard to disconnect");
                return false;
            }
            std::string const target = fmt::format("{}", fmt::join(arguments, " "));
            std::vector<std::shared_ptr<GameSession>> const found = sWorld.FindInWorld(target);
            if (found.empty())
            {
                caller.Reply(fmt::format("No wizard in the world has the character id or name {}", target));
                return false;
            }
            if (found.size() > 1)
            {
                caller.Reply(fmt::format("{} wizards in the world are named {}; kick one by its character id:", found.size(), target));
                for (std::shared_ptr<GameSession> const& session : found)
                    caller.Reply(fmt::format("  {} on session {}", session->GetCharacterId(), session->GetSessionId()));
                return false;
            }
            GameSession& session = *found.front();
            session.KickPlayer(DisconnectReason::Csr, fmt::format("Disconnected by {}", caller.GetName()));
            caller.Reply(fmt::format("Disconnected {} (character {}) on session {}", session.GetCharacterName().empty() ? std::string("a wizard") : session.GetCharacterName(),
                session.GetCharacterId(), session.GetSessionId()));
            return true;
        }
    };
}

void AddSC_cs_misc()
{
    new MiscCommands();
}
