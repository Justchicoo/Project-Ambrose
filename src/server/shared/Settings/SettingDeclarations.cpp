/*
 * Project Ambrose by Imjustchico
 * The table of live settings with the default, bounds and unit each reader already used, grouped by category, and the page doc/config/settings.md is: one table per category, with the apps that read each setting and when a change takes hold.
 */

#include "SettingDeclarations.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <map>

namespace
{
    SettingDeclaration Unsigned(std::string key, std::string defaultValue, std::string min, std::string max, std::string unit, std::string category, uint8 apps, SettingApply apply,
        std::string description)
    {
        return { std::move(key), SettingType::Unsigned, std::move(defaultValue), std::move(min), std::move(max), std::move(unit), std::move(category), std::move(description), apply, {}, apps };
    }

    SettingDeclaration Integer(std::string key, std::string defaultValue, std::string min, std::string max, std::string unit, std::string category, uint8 apps, SettingApply apply,
        std::string description)
    {
        return { std::move(key), SettingType::Integer, std::move(defaultValue), std::move(min), std::move(max), std::move(unit), std::move(category), std::move(description), apply, {}, apps };
    }

    SettingDeclaration Float(std::string key, std::string defaultValue, std::string min, std::string max, std::string unit, std::string category, uint8 apps, SettingApply apply,
        std::string description)
    {
        return { std::move(key), SettingType::Float, std::move(defaultValue), std::move(min), std::move(max), std::move(unit), std::move(category), std::move(description), apply, {}, apps };
    }

    SettingDeclaration Flag(std::string key, std::string defaultValue, std::string category, uint8 apps, SettingApply apply, std::string description)
    {
        return { std::move(key), SettingType::Bool, std::move(defaultValue), {}, {}, {}, std::move(category), std::move(description), apply, {}, apps };
    }

    SettingDeclaration Text(std::string key, std::string defaultValue, std::string maxBytes, std::string category, uint8 apps, SettingApply apply, std::string description)
    {
        return { std::move(key), SettingType::String, std::move(defaultValue), {}, std::move(maxBytes), {}, std::move(category), std::move(description), apply, {}, apps };
    }

    std::vector<SettingDeclaration> Build()
    {
        using namespace SettingApps;
        constexpr SettingApply Live = SettingApply::Live;
        constexpr SettingApply NextUse = SettingApply::NextUse;
        std::vector<SettingDeclaration> table{
            Unsigned("World.UpdateInterval", "50", "1", "10000", "ms", "World", Game, Live, "How long the world waits between ticks."),
            Unsigned("World.Heartbeat", "60", "0", "86400", "s", "World", Game, Live, "How often the world logs that it is still ticking; 0 turns the line off."),
            Unsigned("LoginComplete.Permissions", "47", "0", "4294967295", "", "World", Game, NextUse,
                "The permission bits MSG_LOGINCOMPLETE gives a wizard: 0x1 and 0x4 chat level, 0x2 and 0x8 show chat, 0x20 gifting, 0x40 test features, 0x400 paying, 0x1000 earning crowns."),
            Unsigned("LoginComplete.CSRSecurityLevel", "2", "0", "4", "", "World", Game, NextUse, "The account security level from which MSG_LOGINCOMPLETE opens the client's game master tools."),
            Flag("LoginComplete.TestServer", "false", "World", Game, NextUse, "Whether MSG_LOGINCOMPLETE tells the client it is on a test server."),

            Unsigned("Templates.CacheSize", "256", "1", "65536", "MiB", "World", Game, Live,
                "How much memory the object templates decoded from the install may hold before the least recently used is dropped; a smaller budget drops them at once."),

            Unsigned("Zone.UnloadDelay", "60", "0", "86400", "s", "Zones", Game, NextUse, "How long an empty zone instance stays loaded, read when its last wizard leaves."),
            Unsigned("Zone.MobileIdReleaseDelay", "2000", "0", "60000", "ms", "Zones", Game, NextUse, "How long a mobile id rests after its wizard leaves before another wizard may take it."),

            Text("Realm.Name", "Ambrose", "64", "Realms", Game, NextUse, "The realm's name, announced to the login server with each heartbeat and sent in MSG_LOGINCOMPLETE."),
            Text("Realm.Address", "", "255", "Realms", Game, NextUse, "The address the login server sends players to for this realm; empty uses PublicAddress, then BindIP."),
            Text("PublicAddress", "", "255", "Realms", Game, NextUse, "The address players reach this game server at, used when Realm.Address is empty."),
            Unsigned("Realm.HeartbeatInterval", "30", "1", "3600", "s", "Realms", Game | Login, Live,
                "How often a game server tells the login server it is up, and the beat the login server counts missed heartbeats by."),
            Unsigned("Realm.OfflineAfterIntervals", "3", "1", "1000", "", "Realms", Login, Live, "How many heartbeats a realm may miss before no player is sent to it."),
            Unsigned("Realm.RefreshInterval", "10", "1", "3600", "s", "Realms", Login, Live, "How often the login server rereads the realmlist table."),
            Text("Realm.DefaultRealm", "", "64", "Realms", Login, Live, "The realm a player is sent to when their client names none; a name no realm online has falls through to the least-full realm."),

            Text("GM.CommandPrefix", ".", "8", "Commands", Game, Live, "What a chat line starts with to be read as a command."),
            Flag("GM.LogCommands", "true", "Commands", Game, Live, "Whether every command run is written to the log."),

            Text("Locale.Default", "en-US", "16", "Locale", Game | Login, Live, "The locale names and texts are read in when a client names none."),

            Unsigned("Network.SessionAcceptTimeout", "15", "1", "3600", "s", "Network", Game | Login, NextUse, "How long a new connection may take to finish its handshake."),
            Unsigned("Network.KeepAliveInterval", "60", "0", "3600", "s", "Network", Game | Login, NextUse, "How often an idle connection is asked whether it is still there; 0 never asks."),
            Unsigned("Network.KeepAliveTimeout", "15", "1", "3600", "s", "Network", Game | Login, NextUse, "How long a keepalive may go unanswered before the connection is closed."),
            Unsigned("Network.MaxStrikes", "10", "1", "1000", "", "Network", Game | Login, NextUse, "How many refused or malformed messages a connection may send before it is closed."),
            Unsigned("Network.DroppedMessageBurst", "64", "1", "100000", "", "Network", Game | Login, NextUse,
                "How many messages a connection may send that are dropped unread before a drop counts as a strike."),
            Unsigned("Network.DroppedMessagesPerSecond", "16", "1", "100000", "", "Network", Game | Login, NextUse, "How fast that allowance of dropped messages refills, per second."),
            Unsigned("Network.PingBurst", "16", "1", "100000", "", "Network", Game | Login, NextUse, "How many pings a connection may send at once before a ping counts as a strike."),
            Unsigned("Network.PingsPerSecond", "4", "1", "100000", "", "Network", Game | Login, NextUse, "How fast that allowance of pings refills, per second."),
            Unsigned("Network.HandoffGrace", "30", "1", "3600", "s", "Network", Login, NextUse, "How long a client sent to a game server may keep its login connection open."),
            Unsigned("Attach.Timeout", "30", "1", "3600", "s", "Network", Game, NextUse, "How long a new game connection may go without MSG_ATTACH before it is closed."),

            Text("Login.Name", "Ambrose", "64", "Login", Login, Live, "The login server's name, sent in MSG_STARTCHARACTERLIST."),
            Text("Login.AllowedRevision", "", "1024", "Login", Login, Live, "The client revisions let in while Login.EnforceRevision is on, separated by commas."),
            Flag("Login.EnforceRevision", "false", "Login", Login, Live, "Whether a client whose revision Login.AllowedRevision does not list is refused."),
            Unsigned("Login.MaxAuthAttempts", "5", "0", "1000", "", "Login", Login, Live, "Wrong passwords from one address before it is locked out; 0 never locks it out."),
            Unsigned("Login.LockoutSeconds", "900", "1", "2592000", "s", "Login", Login, Live, "How long a locked-out address is refused, and how long a failure is remembered."),
            Unsigned("Login.DuplicateLoginPolicy", "1", "0", "1", "", "Login", Login, Live, "What a login to an account already logged in does: 0 refuses it, 1 closes the earlier session."),
            Unsigned("Login.SessionKeyLifetime", "108000", "60", "2592000", "s", "Login", Login, NextUse, "How long the session key a successful login issues stays valid."),
            Unsigned("Login.KeyTTL", "60", "5", "2592000", "s", "Login", Login, NextUse, "How long the key a client carries to a game server stays good for."),
            Unsigned("Login.AfkTimeout", "360", "0", "86400", "s", "Login", Login, NextUse, "How long a client may idle before choosing a wizard before it is closed; 0 never closes it."),
            Integer("Login.AfkWarning", "1", "-128", "127", "", "Login", Login, NextUse, "The Warning byte MSG_DISCONNECT_LOGIN_AFK carries."),
            Unsigned("Login.ShutdownGrace", "5", "0", "60", "s", "Login", Login, Live, "How long a stopping login server waits for its shutdown notices to be written."),

            Unsigned("Character.MaxPerAccount", "6", "0", "250", "", "Characters", Login, Live, "How many wizards an account may hold."),
            Flag("Character.AllowChosenNames", "false", "Characters", Login, Live, "Whether any account may name a wizard freely rather than from the client's name tables."),

            Float("Rate.XP.Quest", "1", "0", "100", "times", "Rates", Game, Live, "Multiplies the experience a quest gives."),
            Float("Rate.XP.Kill", "1", "0", "100", "times", "Rates", Game, Live, "Multiplies the experience a defeated creature gives."),
            Float("Rate.Gold.Quest", "1", "0", "100", "times", "Rates", Game, Live, "Multiplies the gold a quest gives."),
            Float("Rate.Gold.Kill", "1", "0", "100", "times", "Rates", Game, Live, "Multiplies the gold a defeated creature drops."),
            Float("Rate.Drop.Item", "1", "0", "100", "times", "Rates", Game, Live, "Multiplies the chance of each item a defeated creature may drop."),
            Float("Rate.Respawn", "1", "0.1", "100", "times", "Rates", Game, Live, "Multiplies how long a defeated creature takes to return."),
        };
        std::sort(table.begin(), table.end(), [](SettingDeclaration const& left, SettingDeclaration const& right) { return left.Key < right.Key; });
        return table;
    }

    std::string AppsOf(uint8 apps)
    {
        std::vector<std::string_view> names;
        if (apps & SettingApps::Game)
            names.push_back("gameserver");
        if (apps & SettingApps::Login)
            names.push_back("loginserver");
        if (apps & SettingApps::Patch)
            names.push_back("patchserver");
        return fmt::format("{}", fmt::join(names, ", "));
    }

    std::string Cell(std::string_view text)
    {
        std::string cell;
        cell.reserve(text.size());
        for (char const c : text)
            cell += c == '|' ? std::string("\\|") : std::string(1, c);
        return cell.empty() ? std::string("empty") : cell;
    }
}

std::vector<SettingDeclaration> const& SettingDeclarations::All()
{
    static std::vector<SettingDeclaration> const table = Build();
    return table;
}

SettingDeclaration const* SettingDeclarations::Find(std::string_view key)
{
    std::vector<SettingDeclaration> const& table = All();
    auto const found = std::lower_bound(table.begin(), table.end(), key, [](SettingDeclaration const& declaration, std::string_view wanted) { return declaration.Key < wanted; });
    return found != table.end() && found->Key == key ? &*found : nullptr;
}

std::string SettingDeclarations::RenderDocument()
{
    std::map<std::string, std::vector<SettingDeclaration const*>> byCategory;
    for (SettingDeclaration const& declaration : All())
        byCategory[declaration.Category].push_back(&declaration);

    std::string page = "<!-- Project Ambrose by Imjustchico: Every live setting, written from the declarations in src/server/shared/Settings. -->\n";
    page += "# Live settings\n\n";
    page += "Every setting here can be changed while its app runs with `.settings set <key> <value> [reason]` in game or `settings set` on the app's console, and returned to its "
            "config value with `settings reset`. A change is checked against the type and bounds below, persisted in the `settings` table of the database the app owns "
            "(`characters` for the game server, `login` for the login server), and written to `setting_audit` with who made it and why. A setting also set by an "
            "`AMBROSE_` environment variable or a command-line override is locked and cannot be changed live. The layers are described in [README.md](README.md).\n\n";
    page += "Applies says when a change takes hold: live at once, or from the next connection or operation that reads it.\n";
    for (auto const& [category, declarations] : byCategory)
    {
        page += fmt::format("\n## {}\n\n", category);
        page += "| Key | Type | Default | Bounds | Applies | Apps | What it does |\n";
        page += "|---|---|---|---|---|---|---|\n";
        for (SettingDeclaration const* declaration : declarations)
        {
            std::string const bounds = Settings::DescribeBounds(*declaration);
            std::string const unitDefault = declaration->Unit.empty() || declaration->Default.empty() ? declaration->Default : fmt::format("{} {}", declaration->Default, declaration->Unit);
            page += fmt::format("| `{}` | {} | {} | {} | {} | {} | {} |\n", declaration->Key, Settings::TypeName(declaration->Type), Cell(unitDefault),
                bounds.empty() ? std::string("none") : Cell(bounds), Settings::ApplyName(declaration->Apply), AppsOf(declaration->Apps), Cell(declaration->Description));
        }
    }
    return page;
}
