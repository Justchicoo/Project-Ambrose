/*
 * Project Ambrose by Imjustchico
 * Creates accounts, sets passwords and security levels, locks, bans, unbans and describes accounts from the console, replying with AccountMgr's result text and parsing ban durations such as 1d12h or perm.
 */

#include "AccountCommands.h"
#include "AccountMgr.h"
#include "Duration.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <chrono>

namespace
{
    std::string const CommandNames[] = { "account create", "account set password", "account set gmlevel", "account lock", "account unlock", "account ban", "account unban", "account info" };

    std::optional<AccountInfo> FindAccount(std::string_view username, ConsoleCommandTable::Reply const& reply)
    {
        AccountLookup const lookup = sAccountMgr.GetAccountByName(username);
        if (lookup.Result != AccountOpResult::Ok)
            reply(fmt::format("Cannot look up account {}: {}", username, AccountMgr::Describe(lookup.Result)));
        else if (!lookup.Account)
            reply(fmt::format("Account {} does not exist", username));
        return lookup.Account;
    }

    std::string FormatTime(uint64 seconds)
    {
        if (seconds == 0)
            return "never";
        std::chrono::sys_seconds const time(std::chrono::seconds(static_cast<std::chrono::seconds::rep>(seconds)));
        std::chrono::sys_days const day = std::chrono::floor<std::chrono::days>(time);
        std::chrono::year_month_day const date(day);
        std::chrono::hh_mm_ss<std::chrono::seconds> const clock(time - day);
        return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02} UTC", static_cast<int>(date.year()), static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()),
            clock.hours().count(), clock.minutes().count(), clock.seconds().count());
    }

    std::string JoinFrom(std::vector<std::string> const& arguments, std::size_t first)
    {
        std::string text;
        for (std::size_t i = first; i < arguments.size(); ++i)
            text += (text.empty() ? "" : " ") + arguments[i];
        return text;
    }
}

void AccountCommands::Register(ConsoleCommandTable& commands)
{
    commands.Register({ "account create", "<username> <password> [email]", "create an account", true,
        [](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
        {
            if (arguments.size() < 2 || arguments.size() > 3)
                return false;
            uint64 id = 0;
            AccountOpResult const result = sAccountMgr.CreateAccount(arguments[0], arguments[1], arguments.size() == 3 ? std::string_view(arguments[2]) : std::string_view(), &id);
            if (result == AccountOpResult::Ok)
                reply(fmt::format("Account {} created with id {}", arguments[0], id));
            else if (result == AccountOpResult::NameAlreadyExists)
                reply(fmt::format("Account {} already exists", arguments[0]));
            else
                reply(fmt::format("Account {} was not created: {}", arguments[0], AccountMgr::Describe(result)));
            return true;
        } });

    commands.Register({ "account set password", "<username> <password>", "replace an account's password", true,
        [](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
        {
            if (arguments.size() != 2)
                return false;
            if (std::optional<AccountInfo> const account = FindAccount(arguments[0], reply))
            {
                AccountOpResult const result = sAccountMgr.ChangePassword(account->Id, arguments[1]);
                reply(result == AccountOpResult::Ok ? fmt::format("Password of {} changed", account->Username) : fmt::format("Password of {} not changed: {}", account->Username, AccountMgr::Describe(result)));
            }
            return true;
        } });

    commands.Register({ "account set gmlevel", "<username> <0-4>", "set the security level: 0 player, 1 moderator, 2 game master, 3 administrator, 4 console", false,
        [](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
        {
            if (arguments.size() != 2)
                return false;
            std::optional<uint32> const level = Ambrose::StringTo<uint32>(arguments[1]);
            if (!level || *level > SEC_CONSOLE)
                return false;
            if (std::optional<AccountInfo> const account = FindAccount(arguments[0], reply))
            {
                AccountOpResult const result = sAccountMgr.SetSecurityLevel(account->Id, static_cast<uint8>(*level));
                reply(result == AccountOpResult::Ok ? fmt::format("Security level of {} set to {}", account->Username, *level) : fmt::format("Security level of {} not changed: {}", account->Username, AccountMgr::Describe(result)));
            }
            return true;
        } });

    for (bool const lock : { true, false })
    {
        commands.Register({ lock ? "account lock" : "account unlock", "<username>", lock ? "refuse logins to an account" : "allow logins to a locked account", false,
            [lock](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
            {
                if (arguments.size() != 1)
                    return false;
                if (std::optional<AccountInfo> const account = FindAccount(arguments[0], reply))
                {
                    AccountOpResult const result = sAccountMgr.SetLocked(account->Id, lock);
                    reply(result == AccountOpResult::Ok ? fmt::format("Account {} {}", account->Username, lock ? "locked" : "unlocked") : fmt::format("Account {} unchanged: {}", account->Username, AccountMgr::Describe(result)));
                }
                return true;
            } });
    }

    commands.Register({ "account ban", "<username> <duration|perm> <reason>", "ban an account for a duration such as 30m, 12h, 7d or 1d12h, or permanently, replacing any ban it already has", false,
        [](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
        {
            if (arguments.size() < 3)
                return false;
            bool const permanent = Ambrose::EqualsIgnoreCase(arguments[1], "perm") || Ambrose::EqualsIgnoreCase(arguments[1], "permanent");
            std::optional<Seconds> const duration = permanent ? Seconds(0) : Ambrose::ParseDuration(arguments[1]);
            if (!duration)
                return false;
            if (std::optional<AccountInfo> const account = FindAccount(arguments[0], reply))
            {
                AccountOpResult const result = sAccountMgr.Ban(account->Id, *duration, "Console", JoinFrom(arguments, 2));
                if (result != AccountOpResult::Ok)
                    reply(fmt::format("Account {} not banned: {}", account->Username, AccountMgr::Describe(result)));
                else if (duration->count() == 0)
                    reply(fmt::format("Account {} banned permanently", account->Username));
                else
                    reply(fmt::format("Account {} banned until {}", account->Username, FormatTime(AccountMgr::Now() + static_cast<uint64>(duration->count()))));
            }
            return true;
        } });

    commands.Register({ "account unban", "<username>", "lift every active ban on an account", false,
        [](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
        {
            if (arguments.size() != 1)
                return false;
            if (std::optional<AccountInfo> const account = FindAccount(arguments[0], reply))
            {
                AccountOpResult const result = sAccountMgr.Unban(account->Id);
                reply(result == AccountOpResult::Ok ? fmt::format("Account {} unbanned", account->Username) : fmt::format("Account {} not unbanned: {}", account->Username, AccountMgr::Describe(result)));
            }
            return true;
        } });

    commands.Register({ "account info", "<username>", "show an account's id, security level, lock, ban and last login", false,
        [](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
        {
            if (arguments.size() != 1)
                return false;
            std::optional<AccountInfo> const account = FindAccount(arguments[0], reply);
            if (!account)
                return true;
            reply(fmt::format("Account {} (id {}): security level {}, {}, verifier key {}", account->Username, account->Id, account->SecurityLevel, account->Locked ? "locked" : "not locked", account->VerifierKeyId));
            reply(fmt::format("Joined {}, last login {} from {} (machine {})", FormatTime(account->JoinDate), FormatTime(account->LastLogin), account->LastIp.empty() ? "nowhere" : account->LastIp, account->LastMachineId));
            AccountOpResult banResult = AccountOpResult::Ok;
            std::optional<AccountBan> const ban = sAccountMgr.GetActiveBan(account->Id, &banResult);
            if (banResult != AccountOpResult::Ok)
                reply(fmt::format("Bans: {}", AccountMgr::Describe(banResult)));
            else if (!ban)
                reply("Not banned");
            else
                reply(fmt::format("Banned {} by {}: {}", ban->IsPermanent() ? std::string("permanently") : "until " + FormatTime(ban->UnbanDate), ban->BannedBy, ban->Reason));
            return true;
        } });
}

void AccountCommands::Unregister(ConsoleCommandTable& commands)
{
    for (std::string const& name : CommandNames)
        commands.Unregister(name);
}
