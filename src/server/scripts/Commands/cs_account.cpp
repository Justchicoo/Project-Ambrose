/*
 * Project Ambrose by Imjustchico
 * The account group: what an administrator or a game master changes about an account without touching SQL. Accounts are created, given a new password, which revokes the session key the client would return to character select with, given a security level or permission bits of its own, which its wizards enter the world with in place of LoginComplete.Permissions from their next entry, and locked or unlocked; one is deleted only once it holds no wizard and is not in the world, and 'onlinelist' lists who is in the world here. A caller in game acts only on an account below its own level and never gives a level as high as its own, so no game master can raise another to its rank or lock out its peers; the console may do all of it.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "DatabaseEnv.h"
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
    std::optional<AccountInfo> FindAccount(CommandCaller& caller, std::string_view username)
    {
        AccountLookup const lookup = sAccountMgr.GetAccountByName(username);
        if (lookup.Result != AccountOpResult::Ok)
            caller.Reply(fmt::format("Cannot look up account {}: {}", username, AccountMgr::Describe(lookup.Result)));
        else if (!lookup.Account)
            caller.Reply(fmt::format("Account {} does not exist", username));
        return lookup.Account;
    }

    bool Outranks(CommandCaller& caller, AccountInfo const& account)
    {
        if (caller.IsConsole() || caller.GetSecurityLevel() > account.SecurityLevel)
            return true;
        caller.Reply(fmt::format("Account {} holds security level {}, which is not below yours", account.Username, account.SecurityLevel));
        return false;
    }

    bool Done(CommandCaller& caller, AccountOpResult result, std::string const& success)
    {
        caller.Reply(result == AccountOpResult::Ok ? success : std::string(AccountMgr::Describe(result)));
        return result == AccountOpResult::Ok;
    }

    std::optional<uint64> CountWizards(uint64 accountId)
    {
        if (!CharacterDatabase.IsOpen())
            return std::nullopt;
        QueryResult rows;
        if (!CharacterDatabase.TryQuery(fmt::format("SELECT COUNT(*) FROM `characters` WHERE `account` = {} AND `deleted_at` IS NULL", accountId), rows) || !rows)
            return std::nullopt;
        return rows->Fetch()[0].Get<uint64>();
    }

    class AccountCommands : public CommandScript
    {
    public:
        AccountCommands() : CommandScript("cs_account") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "account", .SecurityLevel = SEC_GAMEMASTER, .Help = "manage accounts", .Children = {
                    { .Name = "create", .SecurityLevel = SEC_ADMINISTRATOR, .Help = "create an account: <username> <password> [email]", .Sensitive = true, .Run = Create },
                    { .Name = "delete", .SecurityLevel = SEC_ADMINISTRATOR, .Help = "delete an account that holds no wizard: <username>", .Run = Delete },
                    { .Name = "set", .SecurityLevel = SEC_ADMINISTRATOR, .Help = "change an account", .Children = {
                        { .Name = "password", .SecurityLevel = SEC_ADMINISTRATOR, .Help = "set an account's password and revoke its session key: <username> <password>", .Sensitive = true,
                            .Run = SetPassword },
                        { .Name = "gmlevel", .SecurityLevel = SEC_ADMINISTRATOR, .Help = "set an account's security level, 0 to 3: <username> <level>", .Run = SetLevel },
                        { .Name = "permissions", .SecurityLevel = SEC_ADMINISTRATOR, .Help = "set the permission bits an account's wizards enter the world with, or default: <username> <bits|default>",
                            .Run = SetPermissions },
                    } },
                    { .Name = "lock", .SecurityLevel = SEC_GAMEMASTER, .Help = "lock an account so it cannot log in: <username>", .Run = Lock },
                    { .Name = "unlock", .SecurityLevel = SEC_GAMEMASTER, .Help = "unlock an account: <username>", .Run = Unlock },
                    { .Name = "onlinelist", .SecurityLevel = SEC_GAMEMASTER, .Help = "list the accounts with a wizard in the world here", .Run = OnlineList },
                } },
            };
        }

    private:
        static bool Create(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.size() < 2 || arguments.size() > 3)
            {
                caller.Reply("Give a username, a password and, if you like, an email address");
                return false;
            }
            uint64 id = 0;
            AccountOpResult const result = sAccountMgr.CreateAccount(arguments[0], arguments[1], arguments.size() == 3 ? std::string_view(arguments[2]) : std::string_view(), &id);
            return Done(caller, result, fmt::format("Account {} created with id {}", arguments[0], id));
        }

        static bool Delete(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.size() != 1)
            {
                caller.Reply("Give the username of the account to delete");
                return false;
            }
            std::optional<AccountInfo> const account = FindAccount(caller, arguments[0]);
            if (!account || !Outranks(caller, *account))
                return false;
            if (account->Online)
            {
                caller.Reply(fmt::format("Account {} is in the world; kick it first", account->Username));
                return false;
            }
            std::optional<uint64> const wizards = CountWizards(account->Id);
            if (!wizards)
            {
                caller.Reply("The characters database cannot say whether the account holds wizards, so nothing was deleted");
                return false;
            }
            if (*wizards != 0)
            {
                caller.Reply(fmt::format("Account {} holds {} wizard(s); delete them first", account->Username, *wizards));
                return false;
            }
            return Done(caller, sAccountMgr.DeleteAccount(account->Id), fmt::format("Deleted account {} (id {})", account->Username, account->Id));
        }

        static bool SetPassword(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.size() != 2)
            {
                caller.Reply("Give the username and the new password");
                return false;
            }
            std::optional<AccountInfo> const account = FindAccount(caller, arguments[0]);
            if (!account || !Outranks(caller, *account))
                return false;
            return Done(caller, sAccountMgr.ChangePassword(account->Id, arguments[1]), fmt::format("Changed the password of {} and revoked its session key", account->Username));
        }

        static bool SetLevel(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            std::optional<uint8> const level = arguments.size() == 2 ? Ambrose::StringTo<uint8>(arguments[1]) : std::nullopt;
            if (!level || *level > SEC_ADMINISTRATOR)
            {
                caller.Reply("Give the username and a security level from 0 to 3");
                return false;
            }
            std::optional<AccountInfo> const account = FindAccount(caller, arguments[0]);
            if (!account || !Outranks(caller, *account))
                return false;
            if (!caller.IsConsole() && *level >= caller.GetSecurityLevel())
            {
                caller.Reply(fmt::format("You can give a security level below your own {} only", caller.GetSecurityLevel()));
                return false;
            }
            return Done(caller, sAccountMgr.SetSecurityLevel(account->Id, *level), fmt::format("Account {} now holds security level {}", account->Username, *level));
        }

        static bool SetPermissions(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.size() != 2)
            {
                caller.Reply("Give the username and the permission bits, such as 0x2f, or default");
                return false;
            }
            std::optional<uint32> permissions;
            if (Ambrose::ToLower(arguments[1]) != "default")
            {
                std::string_view text = arguments[1];
                bool const hex = text.starts_with("0x") || text.starts_with("0X");
                permissions = Ambrose::StringTo<uint32>(hex ? text.substr(2) : text, hex ? 16 : 10);
                if (!permissions)
                {
                    caller.Reply(fmt::format("{} is not a number of permission bits", arguments[1]));
                    return false;
                }
            }
            std::optional<AccountInfo> const account = FindAccount(caller, arguments[0]);
            if (!account || !Outranks(caller, *account))
                return false;
            return Done(caller, sAccountMgr.SetPermissions(account->Id, permissions),
                permissions ? fmt::format("Account {}'s wizards enter the world with permissions {:#x} from their next entry", account->Username, *permissions)
                            : fmt::format("Account {}'s wizards enter the world with LoginComplete.Permissions from their next entry", account->Username));
        }

        static bool SetLocked(CommandCaller& caller, std::vector<std::string> const& arguments, bool locked)
        {
            if (arguments.size() != 1)
            {
                caller.Reply(fmt::format("Give the username of the account to {}", locked ? "lock" : "unlock"));
                return false;
            }
            std::optional<AccountInfo> const account = FindAccount(caller, arguments[0]);
            if (!account || !Outranks(caller, *account))
                return false;
            return Done(caller, sAccountMgr.SetLocked(account->Id, locked), fmt::format("{} account {}", locked ? "Locked" : "Unlocked", account->Username));
        }

        static bool Lock(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            return SetLocked(caller, arguments, true);
        }

        static bool Unlock(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            return SetLocked(caller, arguments, false);
        }

        static bool OnlineList(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (!arguments.empty())
            {
                caller.Reply("onlinelist takes nothing after it");
                return false;
            }
            std::size_t listed = 0;
            for (std::shared_ptr<GameSession> const& session : sWorld.GetSessions())
            {
                if (session->GetCharacterId() == 0)
                    continue;
                caller.Reply(fmt::format("account {}: {} (character {}) on session {}", session->GetAccountId(),
                    session->GetCharacterName().empty() ? std::string("a wizard") : session->GetCharacterName(), session->GetCharacterId(), session->GetSessionId()));
                ++listed;
            }
            caller.Reply(fmt::format("{} wizard(s) in the world", listed));
            return true;
        }
    };
}

void AddSC_cs_account()
{
    new AccountCommands();
}
