/*
 * Project Ambrose by Imjustchico
 * The ban group: 'ban account', 'ban ip' and 'ban machine' take what to ban, a duration such as 1h, 2d12h or perm, and a reason, and 'unban' ends one early. A banned account's wizards in the world here are disconnected at once with MSG_FORCE_DISCONNECT's AccountBanned and the ban's end, so the client says the account is banned and until when rather than that its connection dropped, and its next login is refused by the login server, which reads the same rows. A caller in game bans only an account below its own level. 'baninfo' says whether an account is banned, until when, by whom and why.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "DisconnectReason.h"
#include "Duration.h"
#include "GameSession.h"
#include "ScriptMgr.h"
#include "StringUtil.h"
#include "World.h"

#include <fmt/format.h>

#include <optional>
#include <string>
#include <vector>

namespace
{
    std::string JoinFrom(std::vector<std::string> const& arguments, std::size_t first)
    {
        std::string text;
        for (std::size_t i = first; i < arguments.size(); ++i)
            text += (text.empty() ? "" : " ") + arguments[i];
        return text;
    }

    std::optional<std::chrono::seconds> ReadDuration(CommandCaller& caller, std::string const& text)
    {
        if (Ambrose::ToLower(text) == "perm")
            return std::chrono::seconds(0);
        std::optional<std::chrono::seconds> const duration = Ambrose::ParseDuration(text);
        if (!duration || duration->count() <= 0)
        {
            caller.Reply(fmt::format("{} is not a duration; write one such as 30m, 1h, 2d12h or perm", text));
            return std::nullopt;
        }
        return duration;
    }

    std::string Until(std::chrono::seconds duration)
    {
        return duration.count() == 0 ? std::string("for good") : fmt::format("for {}", Ambrose::FormatDuration(duration));
    }

    std::size_t DisconnectAccount(uint64 accountId, std::string const& reason, uint64 unbanDate)
    {
        std::size_t kicked = 0;
        for (std::shared_ptr<GameSession> const& session : sWorld.GetSessions())
        {
            if (session->GetAccountId() != accountId)
                continue;
            session->KickPlayer(DisconnectReason::AccountBanned, reason, unbanDate);
            ++kicked;
        }
        return kicked;
    }

    class BanCommands : public CommandScript
    {
    public:
        BanCommands() : CommandScript("cs_ban") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "ban", .SecurityLevel = SEC_GAMEMASTER, .Help = "ban an account, an address or a machine", .Children = {
                    { .Name = "account", .SecurityLevel = SEC_GAMEMASTER, .Help = "ban an account and disconnect it: <username> <duration|perm> <reason>", .Run = BanAccount },
                    { .Name = "ip", .SecurityLevel = SEC_GAMEMASTER, .Help = "ban an address: <address> <duration|perm> <reason>", .Run = BanAddress },
                    { .Name = "machine", .SecurityLevel = SEC_GAMEMASTER, .Help = "ban a machine by its MachineID in hex: <machine> <duration|perm> <reason>", .Run = BanMachine },
                } },
                { .Name = "unban", .SecurityLevel = SEC_GAMEMASTER, .Help = "end a ban early", .Children = {
                    { .Name = "account", .SecurityLevel = SEC_GAMEMASTER, .Help = "end an account's ban: <username>", .Run = UnbanAccount },
                    { .Name = "ip", .SecurityLevel = SEC_GAMEMASTER, .Help = "end an address's ban: <address>", .Run = UnbanAddress },
                    { .Name = "machine", .SecurityLevel = SEC_GAMEMASTER, .Help = "end a machine's ban: <machine>", .Run = UnbanMachine },
                } },
                { .Name = "baninfo", .SecurityLevel = SEC_GAMEMASTER, .Help = "say whether an account is banned, until when, by whom and why: <username>", .Run = BanInfo },
            };
        }

    private:
        static std::optional<AccountInfo> FindAccount(CommandCaller& caller, std::string_view username)
        {
            AccountLookup const lookup = sAccountMgr.GetAccountByName(username);
            if (lookup.Result != AccountOpResult::Ok)
                caller.Reply(fmt::format("Cannot look up account {}: {}", username, AccountMgr::Describe(lookup.Result)));
            else if (!lookup.Account)
                caller.Reply(fmt::format("Account {} does not exist", username));
            return lookup.Account;
        }

        static bool Done(CommandCaller& caller, AccountOpResult result, std::string const& success)
        {
            caller.Reply(result == AccountOpResult::Ok ? success : std::string(AccountMgr::Describe(result)));
            return result == AccountOpResult::Ok;
        }

        static bool BanAccount(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.size() < 3)
            {
                caller.Reply("Give the username, a duration such as 1h or perm, and the reason");
                return false;
            }
            std::optional<std::chrono::seconds> const duration = ReadDuration(caller, arguments[1]);
            if (!duration)
                return false;
            std::optional<AccountInfo> const account = FindAccount(caller, arguments[0]);
            if (!account)
                return false;
            if (!caller.IsConsole() && caller.GetSecurityLevel() <= account->SecurityLevel)
            {
                caller.Reply(fmt::format("Account {} holds security level {}, which is not below yours", account->Username, account->SecurityLevel));
                return false;
            }
            std::string const reason = JoinFrom(arguments, 2);
            AccountOpResult const result = sAccountMgr.Ban(account->Id, *duration, caller.GetName(), reason);
            if (result != AccountOpResult::Ok)
                return Done(caller, result, {});
            std::optional<AccountBan> const ban = sAccountMgr.GetActiveBan(account->Id);
            std::size_t const kicked = DisconnectAccount(account->Id, fmt::format("This account is banned: {}", reason), ban ? ban->UnbanDate : 0);
            caller.Reply(fmt::format("Banned account {} {}: {}; disconnected {} wizard(s) in the world", account->Username, Until(*duration), reason, kicked));
            return true;
        }

        static bool BanAddress(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.size() < 3)
            {
                caller.Reply("Give the address, a duration such as 1h or perm, and the reason");
                return false;
            }
            std::optional<std::chrono::seconds> const duration = ReadDuration(caller, arguments[1]);
            if (!duration)
                return false;
            std::string const reason = JoinFrom(arguments, 2);
            return Done(caller, sAccountMgr.BanAddress(arguments[0], *duration, caller.GetName(), reason), fmt::format("Banned address {} {}: {}", arguments[0], Until(*duration), reason));
        }

        static bool BanMachine(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.size() < 3)
            {
                caller.Reply("Give the MachineID in hex, a duration such as 1h or perm, and the reason");
                return false;
            }
            std::optional<uint64> const machine = Ambrose::StringTo<uint64>(arguments[0], 16);
            if (!machine)
            {
                caller.Reply(fmt::format("{} is not a MachineID in hex", arguments[0]));
                return false;
            }
            std::optional<std::chrono::seconds> const duration = ReadDuration(caller, arguments[1]);
            if (!duration)
                return false;
            std::string const reason = JoinFrom(arguments, 2);
            return Done(caller, sAccountMgr.BanMachine(*machine, *duration, caller.GetName(), reason), fmt::format("Banned machine {:016X} {}: {}", *machine, Until(*duration), reason));
        }

        static bool UnbanAccount(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.size() != 1)
            {
                caller.Reply("Give the username whose ban to end");
                return false;
            }
            std::optional<AccountInfo> const account = FindAccount(caller, arguments[0]);
            if (!account)
                return false;
            return Done(caller, sAccountMgr.Unban(account->Id), fmt::format("Ended the ban of account {}", account->Username));
        }

        static bool UnbanAddress(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.size() != 1)
            {
                caller.Reply("Give the address whose ban to end");
                return false;
            }
            return Done(caller, sAccountMgr.UnbanAddress(arguments[0]), fmt::format("Ended the ban of address {}", arguments[0]));
        }

        static bool UnbanMachine(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            std::optional<uint64> const machine = arguments.size() == 1 ? Ambrose::StringTo<uint64>(arguments[0], 16) : std::nullopt;
            if (!machine)
            {
                caller.Reply("Give the MachineID in hex whose ban to end");
                return false;
            }
            return Done(caller, sAccountMgr.UnbanMachine(*machine), fmt::format("Ended the ban of machine {:016X}", *machine));
        }

        static bool BanInfo(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.size() != 1)
            {
                caller.Reply("Give the username to look up");
                return false;
            }
            std::optional<AccountInfo> const account = FindAccount(caller, arguments[0]);
            if (!account)
                return false;
            AccountOpResult result = AccountOpResult::Ok;
            std::optional<AccountBan> const ban = sAccountMgr.GetActiveBan(account->Id, &result);
            if (result != AccountOpResult::Ok)
                return Done(caller, result, {});
            if (!ban)
            {
                caller.Reply(fmt::format("Account {} is not banned", account->Username));
                return true;
            }
            uint64 const now = AccountMgr::Now();
            caller.Reply(fmt::format("Account {} is banned {} by {}: {}", account->Username,
                ban->UnbanDate == 0 ? std::string("for good") : fmt::format("for {} more", Ambrose::FormatDuration(std::chrono::seconds(ban->UnbanDate > now ? ban->UnbanDate - now : 0))),
                ban->BannedBy, ban->Reason));
            return true;
        }
    };
}

void AddSC_cs_ban()
{
    new BanCommands();
}
