/*
 * Project Ambrose by Imjustchico
 * Registers the panel user commands on a console table: each finds its operator by name and answers in one or two lines; making an operator, resetting a password and handing out a local or pairing link go through the panel's own issuing path with the console named as the issuer, print the link only to the console that asked, and are marked console-only so no browser reaches them through the command route.
 */

#include "PanelCommands.h"
#include "ConsoleCommandTable.h"
#include "Panel.h"
#include "PanelLinks.h"
#include "PanelUsers.h"

#include <fmt/format.h>

#include <chrono>
#include <ctime>
#include <optional>
#include <string>
#include <vector>

namespace
{
    PanelLinkIssuer Console()
    {
        return { AuditActor::System, {}, "console", {} };
    }

    int64 SecondsOf(PanelLinkKind kind)
    {
        return std::chrono::duration_cast<std::chrono::seconds>(PanelLinks::LifetimeOf(kind)).count();
    }
}

std::string PanelCommands::WhenText(int64 epochMs)
{
    std::time_t const value = static_cast<std::time_t>(epochMs / 1000);
    std::tm parts = {};
#ifdef _WIN32
    localtime_s(&parts, &value);
#else
    localtime_r(&value, &parts);
#endif
    return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec);
}

void PanelCommands::Register(ConsoleCommandTable& commands, Panel& panel)
{
    commands.Register({ "panel user list", "", "list the panel's operators", false,
        [&panel](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
        {
            if (!arguments.empty())
                return false;
            std::string error;
            std::vector<PanelUser> const users = panel.Users().List(error);
            if (!error.empty())
            {
                reply(error);
                return true;
            }
            if (users.empty())
                reply("The panel has no operator yet; its console printed a one-time link when it started");
            for (PanelUser const& user : users)
                reply(fmt::format("{}{}{}{} last signed in {}", user.Username, user.IsOwner ? " (owner)" : "", user.Disabled ? " (disabled)" : "",
                    user.TwoFactor ? " (two-factor)" : "", user.SignedInEpochMs ? WhenText(*user.SignedInEpochMs) : std::string("never")));
            return true;
        } });
    commands.Register({ "panel audit verify", "", "verify the panel's audit chain and report the first broken row", false,
        [&panel](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
        {
            if (!arguments.empty())
                return false;
            AuditChainVerification verification;
            std::string error;
            if (!panel.VerifyAuditChain(verification, error))
            {
                reply(fmt::format("The audit chain could not be verified: {}", error));
                return true;
            }
            if (verification.Valid)
                reply(fmt::format("The audit chain is valid; {} row(s) checked", verification.RowsChecked));
            else
                reply(fmt::format("The audit chain is broken at row {}: {}", verification.FirstInvalidId, verification.Problem));
            return true;
        } });
    commands.Register({ "panel user reset-two-factor", "<name>", "turn off an operator's two-factor sign-in and end their sessions, for one who lost their authenticator and codes", false,
        [&panel](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
        {
            if (arguments.size() != 1)
                return false;
            std::string error;
            std::optional<PanelUser> const user = panel.Users().Find(arguments[0], error);
            if (!user)
            {
                reply(error.empty() ? fmt::format("The panel has no operator named {}", arguments[0]) : error);
                return true;
            }
            if (!user->TwoFactor)
            {
                reply(fmt::format("{} has no two-factor sign-in to reset", user->Username));
                return true;
            }
            if (!panel.ResetTwoFactor(*user, "console", error))
            {
                reply(fmt::format("Two-factor sign-in was not reset for {}: {}", user->Username, error));
                return true;
            }
            reply(fmt::format("{} signs in with their password alone until they turn two-factor sign-in on again, and the sessions they had have ended", user->Username));
            return true;
        } });
    commands.Register({ "panel user create", "<name>", "make an operator and print a one-time link they set their password from", false,
        [&panel](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
        {
            if (arguments.size() != 1)
                return false;
            std::string error;
            int64 id = 0;
            PanelUserResult const made = panel.MakeOperator(arguments[0], Console(), id, error);
            if (made != PanelUserResult::Ok)
            {
                reply(fmt::format("{} was not made: {}", arguments[0], made == PanelUserResult::StoreFailed ? error : std::string(PanelUsers::Explain(made))));
                return true;
            }
            PanelLinkRefusal refusal;
            std::optional<PanelLinkIssued> const issued = panel.IssueLink({ PanelLinkKind::Password, arguments[0], {} }, Console(), refusal);
            if (!issued)
            {
                reply(fmt::format("{} was made, but no link to set a password was: {}", arguments[0], refusal.Message));
                return true;
            }
            reply(fmt::format("{} was made. They set their own password once, within {} minutes: {}", issued->Username, SecondsOf(PanelLinkKind::Password) / 60, issued->Url));
            return true;
        }, true });
    commands.Register({ "panel user reset-password", "<name>", "print a one-time link that operator sets a new password from", false,
        [&panel](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
        {
            if (arguments.size() != 1)
                return false;
            PanelLinkRefusal refusal;
            std::optional<PanelLinkIssued> const issued = panel.IssueLink({ PanelLinkKind::Password, arguments[0], {} }, Console(), refusal);
            if (!issued)
            {
                reply(refusal.Message);
                return true;
            }
            reply(fmt::format("{} sets a new password once, within {} minutes: {}", issued->Username, SecondsOf(PanelLinkKind::Password) / 60, issued->Url));
            return true;
        }, true });
    commands.Register({ "panel user link", "[name]",
        "print a local link that opens the panel signed in as that operator, or as the first owner, once, from this machine only, within 60 seconds; on a panel with no operator it makes the owner", false,
        [&panel](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
        {
            if (arguments.size() > 1)
                return false;
            PanelLinkRefusal refusal;
            std::optional<PanelLinkIssued> const issued = panel.IssueLink({ PanelLinkKind::Local, arguments.empty() ? std::string() : arguments[0], {} }, Console(), refusal);
            if (!issued)
            {
                reply(refusal.Message);
                return true;
            }
            reply(issued->Url);
            reply(fmt::format("It signs {} in once, from this machine only, within {} seconds{}", issued->Username, SecondsOf(PanelLinkKind::Local),
                issued->CreatedOwner ? "; the panel had no operator, so they are its owner now, with a password nobody is told until they set their own" : ""));
            return true;
        }, true });
    commands.Register({ "panel user pair", "<name> <host[:port]>",
        "print one line a desktop program on another machine pairs with: the panel's address, the fingerprint of the certificate it serves and a token that signs that operator in once within 10 minutes", false,
        [&panel](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
        {
            if (arguments.size() != 2)
                return false;
            PanelLinkRefusal refusal;
            std::optional<PanelLinkIssued> const issued = panel.IssueLink({ PanelLinkKind::Pairing, arguments[0], arguments[1] }, Console(), refusal);
            if (!issued)
            {
                reply(refusal.Message);
                return true;
            }
            reply(issued->Url);
            std::string const made = issued->CreatedOwner ? "; the panel had no operator, so they are its owner now, with a password nobody is told until they set their own" : "";
            if (issued->Fingerprint.empty())
                reply(fmt::format("It pins no certificate, since the panel serves plain HTTP on this machine only; it signs {} in once within {} minutes{}", issued->Username,
                    SecondsOf(PanelLinkKind::Pairing) / 60, made));
            else
                reply(fmt::format("It pins the certificate the panel serves, SHA-256 {}, and signs {} in once within {} minutes{}", issued->Fingerprint, issued->Username,
                    SecondsOf(PanelLinkKind::Pairing) / 60, made));
            return true;
        }, true });
    commands.Register({ "panel user disable", "<name>", "stop an operator signing in and end the sessions they have", false,
        [&panel](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
        {
            if (arguments.size() != 1)
                return false;
            std::string error;
            std::optional<PanelUser> const user = panel.Users().Find(arguments[0], error);
            if (!user)
            {
                reply(error.empty() ? fmt::format("The panel has no operator named {}", arguments[0]) : error);
                return true;
            }
            if (!panel.Users().SetDisabled(user->Id, true, error))
            {
                reply(error.empty() ? fmt::format("{} was not disabled", user->Username) : error);
                return true;
            }
            panel.Sessions().CloseEveryOne(user->Id, "the operator was disabled", error);
            reply(fmt::format("{} can no longer sign in, and the sessions they had have ended", user->Username));
            return true;
        } });
    commands.Register({ "panel user enable", "<name>", "let a disabled operator sign in again", false,
        [&panel](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
        {
            if (arguments.size() != 1)
                return false;
            std::string error;
            std::optional<PanelUser> const user = panel.Users().Find(arguments[0], error);
            if (!user)
            {
                reply(error.empty() ? fmt::format("The panel has no operator named {}", arguments[0]) : error);
                return true;
            }
            if (!panel.Users().SetDisabled(user->Id, false, error))
            {
                reply(error.empty() ? fmt::format("{} was not enabled", user->Username) : error);
                return true;
            }
            reply(fmt::format("{} can sign in again", user->Username));
            return true;
        } });
}
