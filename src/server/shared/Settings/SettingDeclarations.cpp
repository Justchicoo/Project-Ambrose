/*
 * Project Ambrose by Imjustchico
 * The table of live settings with the default, bounds and unit each reader already used, grouped by category, and the page doc/config/settings.md is: one table per category, with the apps that read each setting, the supervisor among them, when a change takes hold and who may see and change it; the secrets are the table's secret settings and the options only config holds, the admin and panel tokens and the database connection strings.
 */

#include "SettingDeclarations.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "StringUtil.h"

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

    SettingDeclaration Secret(SettingDeclaration declaration)
    {
        declaration.Visibility = SettingVisibility::Secret;
        return declaration;
    }

    SettingDeclaration Restricted(SettingDeclaration declaration)
    {
        declaration.Edit = SettingEditClass::Restricted;
        return declaration;
    }

    bool EndsWithIgnoreCase(std::string_view text, std::string_view suffix)
    {
        return text.size() >= suffix.size() && Ambrose::EqualsIgnoreCase(text.substr(text.size() - suffix.size()), suffix);
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

            Unsigned("Inventory.Slots", "100", "0", "10000", "items", "Inventory", Game, NextUse,
                "How many items every wizard's backpack holds, read when the wizard enters the world and sent to its client as the m_numItemsAllowed of its inventory behavior, since neither the player template nor any other file of the install gives one."),
            Unsigned("Inventory.ExtraSlots", "0", "0", "10000", "items", "Inventory", Game, Live,
                "How many items every wizard's backpack holds beyond Inventory.Slots, read at each add, so raising it lets the next add to a full backpack succeed; the client is told the new total when the wizard next enters the world."),

            Unsigned("Templates.CacheSize", "256", "1", "65536", "MiB", "World", Game, Live,
                "How much memory the object templates decoded from the install may hold before the least recently used is dropped; a smaller budget drops them at once."),

            Unsigned("Zone.UnloadDelay", "60", "0", "86400", "s", "Zones", Game, NextUse, "How long an empty zone instance stays loaded, read when its last wizard leaves."),
            Unsigned("Zone.MobileIdReleaseDelay", "2000", "0", "60000", "ms", "Zones", Game, NextUse, "How long a mobile id rests after its wizard leaves before another wizard may take it."),
            Flag("Zone.DoorsIgnoreRequirements", "false", "Zones", Game, Live,
                "Whether a door, a trigger holding a ResTeleport, fires for a wizard who does not meet its requirements, which otherwise fail closed until requirements are checked; for exploring a test server."),
            Unsigned("Zone.MoveFlushInterval", "250", "50", "5000", "ms", "Zones", Game, NextUse, "How often the moves and movement states of the wizards in an instance are sent to the others in it, read at each flush."),
            Float("Visibility.Distance", "0", "0", "100000", "world units", "Zones", Game, Live,
                "How near an object must come to a wizard to be shown to it, read at each visibility update; 0 takes the zone's own far clip, and a zone with none shows everything."),
            Float("Visibility.Hysteresis", "20", "0", "10000", "world units", "Zones", Game, Live,
                "How far past the visibility distance an object already shown may go before it is taken away, read at each visibility update, so one standing at the edge is not shown and taken away over and over."),
            Unsigned("Zone.MoveIdleIntervals", "2", "1", "100", "", "Zones", Game, NextUse,
                "How many flushes a wizard said to be moving may pass without a new move before the others are told it is standing, read at each flush."),
            Flag("Patch.Enabled", "false", "Patching", Game, Live,
                "Whether the realm may send package-download messages; read at each send, so a live change applies to the next package message. The gate sits on GameSession's own send, so download messages must only ever be sent through GameSession."),
            Unsigned("Player.LinkDeadTime", "60", "0", "86400", "s", "Player", Game, Live,
                "How long a disconnected wizard remains visible and may reattach before being removed from the world."),
            Unsigned("Player.AfkWarnTime", "900", "0", "86400", "s", "Player", Game, Live,
                "How long an in-world wizard may be idle before the client receives MSG_DISCONNECT_AFK."),
            Unsigned("Player.AfkTime", "1800", "0", "86400", "s", "Player", Game, Live,
                "How long an in-world wizard may be idle before its session is disconnected; 0 disables the AFK timer."),
            Float("Potion.RestoreFraction", "1", "0", "1", "fraction", "Player", Game, Live,
                "The share of maximum health and mana each potion restores, read whenever a wizard uses a potion."),
            Unsigned("Potion.RefillInterval", "0", "0", "86400", "s", "Player", Game, Live,
                "How long after a potion is used before one charge refills on its own; 0, the default, leaves refills to buying them and to minigames, as the live game does, and changes apply after the next charge refills."),

            Unsigned("Queue.BypassSecurityLevel", "2", "0", "4", "", "Realms", Login, Live,
                "The minimum account security level that bypasses full-realm admission queues, read for each character selection."),
            Unsigned("Queue.PositionUpdateInterval", "5", "1", "60", "s", "Realms", Login, Live,
                "How often authenticated clients waiting for a realm receive their current queue position."),

            Text("Realm.Name", "Ambrose", "64", "Realms", Game, NextUse, "The realm's name, announced to the login server with each heartbeat and sent in MSG_LOGINCOMPLETE."),
            Text("Realm.Address", "", "255", "Realms", Game, NextUse, "The address the login server sends players to for this realm; empty uses PublicAddress, then BindIP."),
            Text("PublicAddress", "", "255", "Realms", Game, NextUse, "The address players reach this game server at, used when Realm.Address is empty."),
            Unsigned("Realm.HeartbeatInterval", "30", "1", "3600", "s", "Realms", Game | Login, Live,
                "How often a game server tells the login server it is up, and the beat the login server counts missed heartbeats by."),
            Unsigned("Realm.OfflineAfterIntervals", "3", "1", "1000", "", "Realms", Login, Live, "How many heartbeats a realm may miss before no player is sent to it."),
            Unsigned("Realm.RefreshInterval", "10", "1", "3600", "s", "Realms", Login, Live, "How often the login server rereads the realmlist table."),
            Text("Realm.DefaultRealm", "", "64", "Realms", Login, Live, "The realm a player is sent to when their client names none; a name no realm online has falls through to the least-full realm."),

            Text("LoginComplete.SaveDataTo", "", "1024", "Diagnostics", Game, Live,
                "A folder the game server writes each MSG_LOGINCOMPLETE Data it sends into, as the enveloped bytes the client receives, for reading with client core; empty writes nothing."),
            Text("GM.CommandPrefix", ".", "8", "Commands", Game, Live, "What a chat line starts with to be read as a command."),
            Flag("GM.PlayerCommandsAsChat", "true", "Commands", Game, Live,
                "Whether a player's chat line that starts with the command prefix is said as an ordinary line; when off it is refused and the player told so. An account above player level runs such a line as a command."),
            Float("Chat.SayRange", "0", "0", "100000", "world units", "Chat", Game, Live,
                "How far a wizard's typed chat, quick chat and emotes reach the other wizards in its instance, read at each tick; 0 reaches the whole instance."),
            Flag("GM.LogCommands", "true", "Commands", Game, Live, "Whether every command run is written to the log."),

            Unsigned("Social.MaxFriends", "100", "0", "10000", "", "Social", Game, Live,
                "How many friends one wizard may have; a change applies to the next friend request and max-friends reply."),

            Text("Locale.Default", "en-US", "16", "Locale", Game | Login, Live, "The locale names and texts are read in when a client names none."),

            Flag("Network.PacketLog.Enable", "false", "Network", Game | Login, Live,
                "Whether every DML message a session sends or receives is written to the network.packets log by name with its fields, read at each message; a message carrying credentials is written without its field values."),
            Text("Network.PacketLog.Filter", "", "2048", "Network", Game | Login, Live,
                "The message tags the packet log keeps, separated by commas or spaces, read at each message; empty keeps every message not suppressed."),
            Text("Network.PacketLog.Suppress", "MSG_CLIENTMOVE,MSG_SERVERMOVE,MSG_NEWOBJECT,MSG_REMOVEOBJECT,MSG_LOGIN_NOT_AFK", "2048", "Network", Game | Login, Live,
                "The message tags the packet log leaves out, separated by commas or spaces, read at each message; keepalives are control frames and are never written."),
            Unsigned("Network.SessionAcceptTimeout", "15", "1", "3600", "s", "Network", Game | Login, NextUse, "How long a new connection may take to finish its handshake."),
            Unsigned("Network.KeepAliveInterval", "60", "0", "3600", "s", "Network", Game | Login, NextUse, "How often an idle connection is asked whether it is still there; 0 never asks."),
            Unsigned("Network.KeepAliveTimeout", "15", "1", "3600", "s", "Network", Game | Login, NextUse, "How long a keepalive may go unanswered before the connection is closed."),
            Unsigned("Network.MaxStrikes", "10", "1", "1000", "", "Network", Game | Login, NextUse, "How many refused or malformed messages a connection may send before it is closed."),
            Unsigned("Network.DroppedMessageBurst", "64", "1", "100000", "", "Network", Game | Login, NextUse,
                "How many messages a connection may send that are dropped unread before a drop counts as a strike."),
            Unsigned("Network.DroppedMessagesPerSecond", "16", "1", "100000", "", "Network", Game | Login, NextUse, "How fast that allowance of dropped messages refills, per second."),
            Unsigned("Network.PingBurst", "16", "1", "100000", "", "Network", Game | Login, NextUse, "How many pings a connection may send at once before a ping counts as a strike."),
            Unsigned("Network.PingsPerSecond", "4", "1", "100000", "", "Network", Game | Login, NextUse, "How fast that allowance of pings refills, per second."),
            Unsigned("Network.RateLimit.Burst", "150", "1", "100000", "", "Network", Game | Login, Live, "How many inbound frames a session may receive in a burst before frames count against its per-second rate."),
            Unsigned("Network.RateLimit.PerSecond", "50", "1", "100000", "", "Network", Game | Login, Live, "How fast a session's inbound frame allowance refills, per second."),
            Unsigned("Network.MaxConnectionsPerIP", "100", "1", "100000", "", "Network", Game | Login, Live, "How many simultaneous client connections one IP address may hold."),
            Unsigned("Network.AcceptRatePerSecond", "50", "1", "100000", "", "Network", Game | Login, Live, "How many new client connections one IP address may establish per second."),
            Unsigned("Network.SendQueueHighWater", "16777216", "1048576", "1073741824", "bytes", "Network", Game | Login, Live, "How many bytes one connection may have waiting to be sent before it is closed; applies to existing connections immediately."),
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
            Flag("Login.Maintenance", "false", "Login", Login, Live, "Whether the login server is closed for maintenance: accounts below Login.MaintenanceBypassLevel are refused at sign-in with the maintenance reason, and players already in the world stay connected."),
            Text("Login.MaintenanceReason", "", "255", "Login", Login, Live, "The reason a player refused during maintenance is sent; empty sends Maintenance."),
            Unsigned("Login.MaintenanceBypassLevel", "2", "0", "4", "", "Login", Login, Live, "The lowest security level that still signs in during maintenance: 0 player, 1 moderator, 2 game master, 3 administrator, 4 console."),
            Unsigned("Login.ShutdownGrace", "5", "0", "60", "s", "Login", Login, Live, "How long a stopping login server waits for its shutdown notices to be written."),

            Unsigned("Character.MaxPerAccount", "6", "0", "250", "", "Characters", Login, Live, "How many wizards an account may hold."),
            Text("Character.DeleteMode", "soft", "8", "Characters", Login, Live,
                "What deleting a wizard from character select does, read on each delete: soft keeps its row, with the time and the account it was taken from, so a game master can restore it; hard removes it and everything it holds."),
            Unsigned("Character.KeepDeletedDays", "0", "0", "36500", "days", "Characters", Login, Live,
                "How long a soft-deleted wizard is kept before the next delete removes it for good, read on each delete; 0 keeps them all."),
            Flag("Character.AllowChosenNames", "false", "Characters", Login, Live, "Whether any account may name a wizard freely rather than from the client's name tables."),

            Float("Rate.XP.Quest", "1", "0", "100", "times", "Rates", Game, Live, "Multiplies the experience a quest gives."),
            Float("Rate.XP.Kill", "1", "0", "100", "times", "Rates", Game, Live, "Multiplies the experience a defeated creature gives."),
            Float("Rate.Gold.Quest", "1", "0", "100", "times", "Rates", Game, Live, "Multiplies the gold a quest gives."),
            Float("Rate.Gold.Kill", "1", "0", "100", "times", "Rates", Game, Live, "Multiplies the gold a defeated creature drops."),
            Float("Rate.Drop.Item", "1", "0", "100", "times", "Rates", Game, Live, "Multiplies the chance of each item a defeated creature may drop."),
            Float("Rate.Respawn", "1", "0.1", "100", "times", "Rates", Game, Live, "Multiplies how long a defeated creature takes to return."),

            Unsigned("Account.UsernameMinLength", "3", "1", "32", "characters", "Accounts", Login, NextUse, "The shortest username a new account may use."),
            Unsigned("Account.PasswordMinLength", "4", "1", "128", "characters", "Accounts", Login, NextUse, "The fewest characters a new or changed password may have."),
            Restricted(Secret(Text("Account.VerifierKeys", "", "65535", "Accounts", Game | Login, NextUse,
                "The AES-256 keys that seal stored password verifiers, written id:hex with ids 1 to 255 and 64 hex digits each, separated by commas; keep every key that still seals a stored verifier."))),
            Restricted(Unsigned("Account.VerifierActiveKey", "0", "0", "255", "", "Accounts", Game | Login, NextUse,
                "The key id that seals new and changed verifiers, which Account.VerifierKeys must list; 0 stores them unencrypted and is refused while keys are listed.")),
            Restricted(Flag("Account.AllowPlainVerifiers", "true", "Accounts", Login, NextUse,
                "Whether an account whose verifier is still unencrypted may log in while a verifier key is active.")),

            Unsigned("Files.MinFreeBytes", "1073741824", "0", "1125899906842624", "bytes", "Files", Supervisor, Live,
                "The least free space a volume must keep after any write the panel makes; the larger of this and Files.MinFreePercent holds."),
            Unsigned("Files.MinFreePercent", "5", "0", "90", "%", "Files", Supervisor, Live,
                "The least free space a volume must keep after any write the panel makes, as a share of the volume; the larger of this and Files.MinFreeBytes holds."),
            Unsigned("Files.DownloadMaxBytes", "67108864", "65536", "1073741824", "bytes", "Files", Supervisor, Live,
                "The most of a file one download hands out, whole or as a byte range; a larger file is downloaded in ranges."),
            Unsigned("Files.UploadMaxBytes", "16777216", "1", "1073741824", "bytes", "Files", Supervisor, Live,
                "The largest file body the panel accepts for one upload or copy."),
            Unsigned("Files.ReadMaxBytes", "4194304", "65536", "67108864", "bytes", "Files", Supervisor, Live,
                "The most of a file one read hands the panel; a file this size or smaller also carries its content hash, and a configuration file larger than this is not shown."),
            Unsigned("Files.ListMaxEntries", "100000", "1000", "10000000", "entries", "Files", Supervisor, Live,
                "The most entries a folder listing reads before it stops and says the folder held more."),
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
        if (apps & SettingApps::Supervisor)
            names.push_back("supervisor");
        return fmt::format("{}", fmt::join(names, ", "));
    }

    std::string Access(SettingDeclaration const& declaration)
    {
        bool const secret = declaration.Visibility == SettingVisibility::Secret;
        bool const restricted = declaration.Edit == SettingEditClass::Restricted;
        if (secret && restricted)
            return "secret, restricted";
        return secret ? "secret" : restricted ? "restricted" : "normal";
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

bool SettingDeclarations::IsSecret(std::string_view key)
{
    static std::vector<std::string> const secrets = []
    {
        std::vector<std::string> keys;
        for (SettingDeclaration const& declaration : All())
            if (declaration.Visibility == SettingVisibility::Secret)
                keys.push_back(declaration.Key);
        return keys;
    }();
    if (Ambrose::EqualsIgnoreCase(key, "Admin.Token") || Ambrose::EqualsIgnoreCase(key, "Panel.Token") || EndsWithIgnoreCase(key, "DatabaseInfo"))
        return true;
    return std::any_of(secrets.begin(), secrets.end(), [key](std::string const& secret) { return Ambrose::EqualsIgnoreCase(secret, key); });
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
            "(`characters` for the game server, `login` for the login server, and the panel store for the supervisor), and written to `setting_audit` with who made it and why. "
            "A setting also set by an `AMBROSE_` environment variable or a command-line override is locked and cannot be changed live. The layers are described in [README.md](README.md).\n\n";
    page += "Applies says when a change takes hold: live at once, or from the next connection or operation that reads it.\n\n";
    page += "Access says who may see and change a setting over the admin API and the panel. A secret's value is shown masked, in `setting_audit` too, unless the caller asks "
            "for it with the right to see secrets, and every such reveal is audited. The admin and panel tokens and the password in each database connection string are secrets "
            "too, although only config holds them, and are masked the same way wherever they are shown, a configuration file read through the panel included. A restricted "
            "setting is one whose wrong value stops the app or locks players out, so changing it takes its own right besides the right to change settings.\n";
    for (auto const& [category, declarations] : byCategory)
    {
        page += fmt::format("\n## {}\n\n", category);
        page += "| Key | Type | Default | Bounds | Applies | Apps | Access | What it does |\n";
        page += "|---|---|---|---|---|---|---|---|\n";
        for (SettingDeclaration const* declaration : declarations)
        {
            std::string const bounds = Settings::DescribeBounds(*declaration);
            std::string const unitDefault = declaration->Unit.empty() || declaration->Default.empty() ? declaration->Default : fmt::format("{} {}", declaration->Default, declaration->Unit);
            page += fmt::format("| `{}` | {} | {} | {} | {} | {} | {} | {} |\n", declaration->Key, Settings::TypeName(declaration->Type), Cell(unitDefault),
                bounds.empty() ? std::string("none") : Cell(bounds), Settings::ApplyName(declaration->Apply), AppsOf(declaration->Apps), Access(*declaration), Cell(declaration->Description));
        }
    }
    return page;
}
