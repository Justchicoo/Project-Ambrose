/*
 * Project Ambrose by Imjustchico
 * Mutes and unmutes an online wizard by character name or id, persisting the expiry and reason before applying the change to the target session on the world thread.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "Duration.h"
#include "GameSession.h"
#include "ScriptMgr.h"
#include "World.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    struct MuteTarget
    {
        std::shared_ptr<GameSession> Session;
        std::string Name;
    };

    std::optional<MuteTarget> FindMuteTarget(CommandCaller& caller, std::string_view command, std::vector<std::string> const& arguments)
    {
        if (arguments.empty())
        {
            caller.Reply(fmt::format("{} takes a wizard in the world, by character id or name", command));
            return std::nullopt;
        }
        std::string const wanted = fmt::format("{}", fmt::join(arguments, " "));
        std::vector<std::shared_ptr<GameSession>> const found = sWorld.FindInWorld(wanted);
        if (found.empty())
        {
            caller.Reply(fmt::format("No wizard in the world has the character id or name {}", wanted));
            return std::nullopt;
        }
        if (found.size() > 1)
        {
            caller.Reply(fmt::format("{} wizards in the world are named {}; name one by its character id:", found.size(), wanted));
            for (std::shared_ptr<GameSession> const& session : found)
                caller.Reply(fmt::format("  {} on session {}", session->GetCharacterId(), session->GetSessionId()));
            return std::nullopt;
        }
        std::string name = found.front()->GetCharacterName();
        if (name.empty())
            name = fmt::format("wizard {}", found.front()->GetCharacterId());
        return MuteTarget{ found.front(), std::move(name) };
    }

    class MuteCommands final : public CommandScript
    {
    public:
        MuteCommands() : CommandScript("cs_mute") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "mute", .SecurityLevel = SEC_GAMEMASTER, .Help = "mute a wizard in the world for a duration such as 5m", .Run = Mute },
                { .Name = "unmute", .SecurityLevel = SEC_GAMEMASTER, .Help = "remove a wizard's mute", .Run = Unmute },
            };
        }

    private:
        static bool Mute(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            std::size_t durationIndex = 0;
            std::optional<Seconds> duration;
            for (std::size_t index = 1; index < arguments.size(); ++index)
                if (std::optional<Seconds> parsed = Ambrose::ParseDuration(arguments[index]))
                {
                    durationIndex = index;
                    duration = *parsed;
                    break;
                }
            if (!duration || durationIndex == 0 || *duration <= Seconds::zero() || *duration > AccountMgr::MaxMuteDuration)
            {
                caller.Reply("Use mute <wizard name or character id> <duration> [reason], such as mute Aaron Stormblade 5m");
                return false;
            }

            std::vector<std::string> const nameArguments(arguments.begin(), arguments.begin() + static_cast<std::ptrdiff_t>(durationIndex));
            std::optional<MuteTarget> const target = FindMuteTarget(caller, "mute", nameArguments);
            if (!target)
                return false;

            std::string const reason = durationIndex + 1 < arguments.size()
                ? fmt::format("{}", fmt::join(arguments.begin() + static_cast<std::ptrdiff_t>(durationIndex + 1), arguments.end(), " "))
                : "No reason given";
            std::string const mutedBy = caller.GetName();
            uint64 until = 0;
            AccountOpResult const stored = sAccountMgr.MuteAccount(target->Session->GetAccountId(), *duration, mutedBy, reason, &until);
            if (stored != AccountOpResult::Ok)
            {
                caller.Reply(fmt::format("Could not mute {}: {}", target->Name, AccountMgr::Describe(stored)));
                return false;
            }

            bool const applied = sWorld.RunFor(target->Session, [until](GameSession& session) { session.ApplyMute(until); }, World::CommandTimeout);
            if (!applied)
            {
                caller.Reply(fmt::format("Mute for {} was stored, but its online session did not confirm the change; it will also apply on the next login", target->Name));
                return false;
            }
            caller.Reply(fmt::format("Muted {} for {}: {}", target->Name, Ambrose::FormatDuration(*duration), reason));
            return true;
        }

        static bool Unmute(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            std::optional<MuteTarget> const target = FindMuteTarget(caller, "unmute", arguments);
            if (!target)
                return false;

            AccountOpResult const stored = sAccountMgr.UnmuteAccount(target->Session->GetAccountId());
            if (stored != AccountOpResult::Ok)
            {
                caller.Reply(fmt::format("Could not unmute {}: {}", target->Name, AccountMgr::Describe(stored)));
                return false;
            }

            bool const applied = sWorld.RunFor(target->Session, [](GameSession& session) { session.ClearMute(); }, World::CommandTimeout);
            if (!applied)
            {
                caller.Reply(fmt::format("The mute for {} was removed from the login database, but its online session did not confirm the change", target->Name));
                return false;
            }
            caller.Reply(fmt::format("Unmuted {}", target->Name));
            return true;
        }
    };
}

void AddSC_cs_mute()
{
    new MuteCommands();
}
