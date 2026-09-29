/*
 * Project Ambrose by Imjustchico
 * Reads where everything lives: the install as the executable's folder or its parent when that is bin, unless the parent is a drive or file system root or a prefix other software shares such as /usr, /usr/local or /opt, the supervisor's config folder, its LogsDir, Backups.Dir or backups in the data folder, the custom SQL tree, each app's config read without the environment for its LogsDir, admin token file and TLS key, and the supervisor's own token files and keys; then builds the roots with their policies and built-in protected paths, the most protective first so that of two roots sharing one folder the client installs, then the data folder, then backups keep it and the other is dropped with a note, before putting them in the order the page shows them, carves a root nested in another out of it with a rule naming where it is served instead, keeps configuration files out of an install that shares their folder, adds each root's operator patterns, and knows the store with its journal files, the keyring, every token file and every TLS key by identity and by path.
 */

#include "FileRoots.h"
#include "AdminToken.h"
#include "AppDefinition.h"
#include "ConfigMgr.h"
#include "ListenerSettings.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <cstddef>
#include <system_error>
#include <utility>

namespace
{
    using Ambrose::FileOperation;
    using Ambrose::RuleEffect;
    using Ambrose::RuleOrigin;

    constexpr std::string_view ConfigOnly = "The config root carries only configuration files: *.conf, *.conf.dist and conf.d";
    constexpr std::string_view AuditFiles = "The apps' command audit files are their own record and are never read or changed here";
    constexpr std::string_view PanelKeeps = "The trash and version store the panel keeps for this root";
    constexpr std::string_view ClientBuilt = "Built from your own Wizard101 install, so it is listed but never handed to a browser";

    struct Draft
    {
        std::string Id = {};
        std::string Label = {};
        FileRootKind Kind = FileRootKind::Data;
        std::filesystem::path Path = {};
        bool Filtered = false;
        std::vector<std::string> Apps = {};
    };

    std::filesystem::path Resolve(std::filesystem::path const& value, std::filesystem::path const& base)
    {
        if (value.empty())
            return {};
        return (value.is_absolute() || base.empty() ? value : base / value).lexically_normal();
    }

    std::filesystem::path OptionPath(ConfigMgr const& config, std::string const& key)
    {
        return ConfigMgr::PathFromUtf8(Ambrose::Trim(config.GetOption<std::string>(key, "", true)));
    }

    std::filesystem::path TokenFileOf(ListenerSettings const& settings, std::string const& name, std::filesystem::path const& dataFolder, std::filesystem::path const& configFolder,
        std::filesystem::path const& workingFolder)
    {
        if (!settings.TokenFile.empty())
            return Resolve(settings.TokenFile, workingFolder);
        return AdminToken::DefaultFile(name, dataFolder, configFolder);
    }

    std::vector<std::string> Parts(std::filesystem::path const& path)
    {
        std::vector<std::string> parts;
        for (std::filesystem::path const& part : path)
        {
            std::string text = ConfigMgr::PathToUtf8(part);
            if (!text.empty())
                parts.push_back(std::move(text));
        }
        return parts;
    }

    bool SameName(std::string_view left, std::string_view right)
    {
        return Ambrose::PathRules::PlatformCase() == Ambrose::CaseMode::Insensitive ? Ambrose::EqualsIgnoreCase(left, right) : left == right;
    }

    std::string Anchored(std::vector<std::string> const& relative)
    {
        std::string pattern;
        for (std::string const& part : relative)
            pattern += "/" + Ambrose::PathRules::Escape(part);
        return pattern;
    }

    void Rule(FileRoot& root, std::string const& pattern, RuleEffect effect, std::string_view why)
    {
        std::string error;
        root.Rules.Add(pattern, effect, RuleOrigin::BuiltIn, std::string(why), error);
    }

    Ambrose::FilePolicy Allowing(std::initializer_list<FileOperation> allowed, std::string const& reason, std::string summary, std::string const& code = "refused_by_policy")
    {
        Ambrose::FilePolicy policy;
        std::vector<FileOperation> const kept(allowed);
        policy.RefuseAllBut(kept, reason, code);
        policy.SetSummary(std::move(summary));
        return policy;
    }

    Ambrose::FilePolicy Refusing(std::initializer_list<std::pair<FileOperation, std::string_view>> refused, std::string summary)
    {
        Ambrose::FilePolicy policy;
        for (auto const& [operation, reason] : refused)
            policy.Refuse(operation, std::string(reason));
        policy.SetSummary(std::move(summary));
        return policy;
    }

    Ambrose::FilePolicy PolicyFor(FileRootKind kind)
    {
        switch (kind)
        {
            case FileRootKind::Install:
                return Allowing({ FileOperation::List, FileOperation::Read, FileOperation::Preview, FileOperation::Download, FileOperation::Archive, FileOperation::Share, FileOperation::Sftp },
                    "The install root is the running build; it changes only through updates", "Read-only: the running build, which changes only through updates");
            case FileRootKind::Config:
            {
                constexpr std::string_view Secrets = "A configuration file holds secrets, so it is read here with them masked rather than handed out whole";
                constexpr std::string_view Only = "Only configuration files belong in the config root, so nothing is unpacked or fetched into it";
                return Refusing({ { FileOperation::Download, Secrets }, { FileOperation::Archive, Secrets }, { FileOperation::Share, Secrets }, { FileOperation::Extract, Only },
                                    { FileOperation::Pull, Only }, { FileOperation::Truncate, "A configuration file is edited, never truncated" },
                                    { FileOperation::Permissions, "Configuration files keep the permissions the supervisor gives them, since they hold secrets" } },
                    "Writable configuration files: *.conf, *.conf.dist and conf.d, with secret values masked");
            }
            case FileRootKind::Logs:
                return Allowing({ FileOperation::List, FileOperation::Read, FileOperation::Preview, FileOperation::Download, FileOperation::Archive, FileOperation::Share,
                                    FileOperation::Delete, FileOperation::Truncate, FileOperation::Sftp },
                    "Log files are written by the apps that keep them; truncate, rotate or trash them instead", "Logs: read, download, truncate or trash them; the apps write them");
            case FileRootKind::Data:
                return Allowing({ FileOperation::List, FileOperation::Read, FileOperation::Preview, FileOperation::Download, FileOperation::Archive, FileOperation::Share, FileOperation::Sftp },
                    "The data folder is written by the servers and tools that keep it; the panel reads it",
                    "Read-only: what the extractors built from your install is listed but never handed out");
            case FileRootKind::SqlCustom:
            {
                constexpr std::string_view OneByOne = "SQL files are added here one at a time, so nothing is unpacked or fetched into this folder";
                return Refusing({ { FileOperation::Extract, OneByOne }, { FileOperation::Pull, OneByOne } }, "Writable: local SQL that is never upstreamed, where exported world edits appear");
            }
            case FileRootKind::Backups:
                return Allowing({ FileOperation::List, FileOperation::Read, FileOperation::Preview }, "Downloads, deletes and restores of backups go through the backups page",
                    "Read-only here: the backups page downloads, deletes and restores them");
            case FileRootKind::Client:
                break;
        }
        Ambrose::FilePolicy policy = Allowing({ FileOperation::List }, "Your own Wizard101 install is listed but never handed to a browser",
            "Listing only: your own Wizard101 install, never handed to a browser", "client_derived");
        policy.SetClientDerived(true);
        return policy;
    }

    bool SharedPrefix(std::filesystem::path const& folder)
    {
        if (!folder.has_relative_path())
            return true;
        std::string const text = ConfigMgr::PathToUtf8(folder.lexically_normal());
        return text == "/usr" || text == "/usr/local" || text == "/opt";
    }

    std::string Owner(std::string_view id)
    {
        return fmt::format("This folder is the {} root; open it there", id);
    }

    void AddApp(std::vector<std::string>& apps, std::string const& name)
    {
        if (std::find(apps.begin(), apps.end(), name) == apps.end())
            apps.push_back(name);
    }

    void Place(std::vector<Draft>& drafts, Draft draft, std::vector<std::string>& notes)
    {
        if (draft.Path.empty())
            return;
        for (Draft& existing : drafts)
        {
            if (!FileRootSet::SamePath(existing.Path, draft.Path) || existing.Filtered != draft.Filtered)
                continue;
            if (existing.Kind == draft.Kind)
            {
                for (std::string const& app : draft.Apps)
                    AddApp(existing.Apps, app);
                return;
            }
            notes.push_back(fmt::format("The {} root shares its folder with the {} root, so its files are served there under that root's policy", draft.Id, existing.Id));
            return;
        }
        drafts.push_back(std::move(draft));
    }
}

std::filesystem::path FileRootSet::Normal(std::filesystem::path const& path)
{
    if (path.empty())
        return {};
    std::error_code code;
    std::filesystem::path absolute = std::filesystem::absolute(path, code);
    if (code)
        absolute = path;
    std::filesystem::path normal = std::filesystem::weakly_canonical(absolute, code);
    if (code)
        normal = absolute;
    normal = normal.lexically_normal();
    while (!normal.has_filename() && normal.has_relative_path())
        normal = normal.parent_path();
    return normal;
}

std::optional<std::vector<std::string>> FileRootSet::Within(std::filesystem::path const& outer, std::filesystem::path const& inner)
{
    std::vector<std::string> const outside = Parts(outer);
    std::vector<std::string> const inside = Parts(inner);
    if (outside.empty() || inside.size() < outside.size())
        return std::nullopt;
    for (std::size_t index = 0; index < outside.size(); ++index)
        if (!SameName(outside[index], inside[index]))
            return std::nullopt;
    return std::vector<std::string>(inside.begin() + static_cast<std::ptrdiff_t>(outside.size()), inside.end());
}

bool FileRootSet::SamePath(std::filesystem::path const& left, std::filesystem::path const& right)
{
    std::optional<std::vector<std::string>> const rest = Within(left, right);
    return rest && rest->empty();
}

std::string_view FileRootSet::KindName(FileRootKind kind) noexcept
{
    switch (kind)
    {
        case FileRootKind::Install: return "install";
        case FileRootKind::Config: return "config";
        case FileRootKind::Logs: return "logs";
        case FileRootKind::Data: return "data";
        case FileRootKind::SqlCustom: return "sql-custom";
        case FileRootKind::Backups: return "backups";
        case FileRootKind::Client: break;
    }
    return "client";
}

FileRoot const* FileRootSet::Find(std::string_view id) const
{
    auto const found = std::find_if(Roots.begin(), Roots.end(), [id](FileRoot const& root) { return root.Id == id; });
    return found == Roots.end() ? nullptr : &*found;
}

bool FileRootSet::IsSecret(Ambrose::FileIdentity const& identity, std::filesystem::path const& host) const
{
    if (std::any_of(SecretIdentities.begin(), SecretIdentities.end(), [&identity](Ambrose::FileIdentity const& secret) { return secret.Same(identity); }))
        return true;
    if (host.empty())
        return false;
    return std::any_of(SecretPaths.begin(), SecretPaths.end(), [&host](std::filesystem::path const& secret) { return SamePath(secret, host); });
}

std::filesystem::path FileRootInputs::InstallFolderFor(std::filesystem::path const& executableFolder)
{
    std::filesystem::path const folder = FileRootSet::Normal(executableFolder);
    if (folder.empty())
        return {};
    if (!SameName(ConfigMgr::PathToUtf8(folder.filename()), "bin") || !folder.has_parent_path())
        return folder;
    std::filesystem::path const parent = folder.parent_path();
    return SharedPrefix(parent) ? folder : parent;
}

FileRootInputs FileRootInputs::Read(ConfigMgr const& config, FileRootPlaces const& places, std::vector<std::string>& problems)
{
    FileRootInputs inputs;
    inputs.DataFolder = places.DataFolder;
    inputs.InstallFolder = InstallFolderFor(places.ExecutableFolder);
    if (!places.SourceFolder.empty())
        inputs.SqlCustomFolder = places.SourceFolder / "data" / "sql" / "custom";
    inputs.ConfigFile = config.GetFilename();
    std::filesystem::path const configFolder = inputs.ConfigFile.parent_path();
    inputs.LogsFolder = Resolve(OptionPath(config, "LogsDir"), places.WorkingFolder);
    std::filesystem::path const backups = OptionPath(config, "Backups.Dir");
    if (!backups.empty())
        inputs.BackupsFolder = Resolve(backups, places.WorkingFolder);
    else if (!places.DataFolder.empty())
        inputs.BackupsFolder = places.DataFolder / "backups";
    inputs.StoreFile = places.StoreFile;
    if (!places.DataFolder.empty())
        inputs.KeyringFile = places.DataFolder / "keyring";

    for (std::string_view const prefix : { std::string_view("Admin"), std::string_view("Panel") })
    {
        ListenerSettings const listener = ListenerSettings::Load(config, prefix, 0);
        inputs.SecretFiles.push_back(TokenFileOf(listener, prefix == "Admin" ? "supervisor" : "panel", places.DataFolder, configFolder, places.WorkingFolder));
        if (!listener.PrivateKeyFile.empty())
            inputs.SecretFiles.push_back(Resolve(listener.PrivateKeyFile, places.WorkingFolder));
    }

    for (AppDefinition const& app : AppDefinition::Load(config, places.ExecutableFolder, places.WorkingFolder, problems))
    {
        FileRootApp entry;
        entry.Name = app.Name;
        entry.Program = app.ProgramName;
        entry.Config = app.Config;
        ConfigMgr own([](std::string const&) { return std::optional<std::string>(); });
        ConfigLoadResult const loaded = own.LoadInitial(app.Config);
        if (!loaded.Succeeded())
        {
            problems.push_back(fmt::format("The config of {} at {} could not be read, so the panel does not know its logs folder or its secret files", app.Name, ConfigMgr::PathToUtf8(app.Config)));
            inputs.Apps.push_back(std::move(entry));
            continue;
        }
        entry.Logs = Resolve(OptionPath(own, "LogsDir"), app.WorkingDirectory);
        ListenerSettings const admin = ListenerSettings::Load(own, "Admin", 0);
        entry.Secrets.push_back(TokenFileOf(admin, app.ProgramName, places.DataFolder, app.Config.parent_path(), app.WorkingDirectory));
        if (!admin.PrivateKeyFile.empty())
            entry.Secrets.push_back(Resolve(admin.PrivateKeyFile, app.WorkingDirectory));
        inputs.Apps.push_back(std::move(entry));
    }
    inputs.ClientInstalls = places.ClientInstalls;
    return inputs;
}

FileRootSet FileRootSet::Build(FileRootInputs const& inputs, std::vector<std::string>& errors)
{
    FileRootSet set;
    set.Apps = inputs.Apps;

    std::vector<std::filesystem::path> secrets;
    if (!inputs.StoreFile.empty())
    {
        secrets.push_back(inputs.StoreFile);
        for (std::string_view const suffix : { "-wal", "-shm", "-journal" })
        {
            std::filesystem::path journal = inputs.StoreFile;
            journal += ConfigMgr::PathFromUtf8(suffix);
            secrets.push_back(std::move(journal));
        }
    }
    secrets.push_back(inputs.KeyringFile);
    secrets.insert(secrets.end(), inputs.SecretFiles.begin(), inputs.SecretFiles.end());
    for (FileRootApp const& app : inputs.Apps)
        secrets.insert(secrets.end(), app.Secrets.begin(), app.Secrets.end());
    for (std::filesystem::path const& secret : secrets)
    {
        if (secret.empty())
            continue;
        std::filesystem::path const normal = Normal(secret);
        if (std::none_of(set.SecretPaths.begin(), set.SecretPaths.end(), [&normal](std::filesystem::path const& known) { return SamePath(known, normal); }))
            set.SecretPaths.push_back(normal);
        std::error_code code;
        if (!std::filesystem::exists(normal, code))
            continue;
        Ambrose::FileIdentity identity;
        std::string why;
        if (Ambrose::FileJail::IdentityOf(normal, identity, why))
            set.SecretIdentities.push_back(identity);
    }

    std::vector<Draft> drafts;
    for (std::size_t index = 0; index < inputs.ClientInstalls.size(); ++index)
    {
        std::string const id = index == 0 ? std::string("client") : fmt::format("client-{}", index + 1);
        Place(drafts, { id, index == 0 ? std::string("Client install") : fmt::format("Client install {}", index + 1), FileRootKind::Client, Normal(inputs.ClientInstalls[index]), false, {} },
            set.Notes);
    }
    Place(drafts, { "data", "Data", FileRootKind::Data, Normal(inputs.DataFolder), false, {} }, set.Notes);
    Place(drafts, { "backups", "Backups", FileRootKind::Backups, Normal(inputs.BackupsFolder), false, {} }, set.Notes);
    Place(drafts, { "install", "Install", FileRootKind::Install, Normal(inputs.InstallFolder), false, {} }, set.Notes);

    std::filesystem::path const configFolder = Normal(inputs.ConfigFile.parent_path());
    Draft config{ "config", "Configuration", FileRootKind::Config, configFolder, true, { "supervisor" } };
    for (FileRootApp const& app : inputs.Apps)
        if (SamePath(Normal(app.Config.parent_path()), configFolder))
            AddApp(config.Apps, app.Name);
    Place(drafts, std::move(config), set.Notes);
    for (FileRootApp const& app : inputs.Apps)
    {
        std::filesystem::path const folder = Normal(app.Config.parent_path());
        if (folder.empty() || SamePath(folder, configFolder))
            continue;
        Place(drafts, { "config-" + app.Name, fmt::format("{} configuration", app.Name), FileRootKind::Config, folder, true, { app.Name } }, set.Notes);
    }

    std::filesystem::path const logsFolder = Normal(inputs.LogsFolder);
    if (!logsFolder.empty())
    {
        Draft logs{ "logs", "Logs", FileRootKind::Logs, logsFolder, false, { "supervisor" } };
        for (FileRootApp const& app : inputs.Apps)
            if (!app.Logs.empty() && SamePath(Normal(app.Logs), logsFolder))
                AddApp(logs.Apps, app.Name);
        Place(drafts, std::move(logs), set.Notes);
    }
    for (FileRootApp const& app : inputs.Apps)
    {
        std::filesystem::path const folder = Normal(app.Logs);
        if (folder.empty() || (!logsFolder.empty() && SamePath(folder, logsFolder)))
            continue;
        Place(drafts, { "logs-" + app.Name, fmt::format("{} logs", app.Name), FileRootKind::Logs, folder, false, { app.Name } }, set.Notes);
    }
    Place(drafts, { "sql-custom", "Custom SQL", FileRootKind::SqlCustom, Normal(inputs.SqlCustomFolder), false, {} }, set.Notes);
    std::stable_sort(drafts.begin(), drafts.end(), [](Draft const& left, Draft const& right) { return left.Kind < right.Kind; });

    for (Draft const& draft : drafts)
    {
        FileRoot root;
        root.Id = draft.Id;
        root.Label = draft.Label;
        root.Kind = draft.Kind;
        root.Apps = draft.Apps;
        root.Path = draft.Path;
        root.Policy = PolicyFor(draft.Kind);
        std::string problem;
        root.Jail = Ambrose::JailRoot::Open(draft.Path, problem);
        if (!root.Jail)
            root.Problem = fmt::format("The {} folder cannot be opened: {}", draft.Id, problem);
        Rule(root, "/.ambrose/", RuleEffect::Hide, PanelKeeps);
        if (draft.Filtered)
        {
            Rule(root, "*", RuleEffect::Hide, ConfigOnly);
            for (std::string_view const kept : { "!*.conf", "!*.conf.dist", "!/conf.d/", "!/conf.d/**" })
                Rule(root, std::string(kept), RuleEffect::Hide, ConfigOnly);
            Rule(root, "/audit/", RuleEffect::Hide, AuditFiles);
        }
        if (draft.Kind == FileRootKind::Data)
        {
            Rule(root, "/admin/", RuleEffect::Hide, "The admin API token files, which no browser is ever handed");
            Rule(root, "/panel/", RuleEffect::Hide, "The panel's own store, which no browser is ever handed");
            Rule(root, "/keyring", RuleEffect::Hide, "The keys that open the panel's secrets");
            Rule(root, "*.lock", RuleEffect::Hide, "Lock files belong to the process holding them");
            Rule(root, "*", RuleEffect::ClientDerived, ClientBuilt);
            for (std::string_view const kept : { "!/supervisor/", "!/supervisor/**", "!/launcher-window.json" })
                Rule(root, std::string(kept), RuleEffect::ClientDerived, ClientBuilt);
        }
        set.Roots.push_back(std::move(root));
    }

    for (FileRoot& outer : set.Roots)
    {
        auto const placed = std::find_if(drafts.begin(), drafts.end(), [&outer](Draft const& draft) { return draft.Id == outer.Id; });
        Draft const& outerDraft = *placed;
        for (Draft const& inner : drafts)
        {
            if (inner.Id == outer.Id)
                continue;
            std::optional<std::vector<std::string>> const relative = Within(outerDraft.Path, inner.Path);
            if (!relative)
                continue;
            if (relative->empty())
            {
                if (!inner.Filtered || outerDraft.Filtered)
                    continue;
                for (std::string_view const kept : { "/*.conf", "/*.conf.dist", "/conf.d/" })
                    Rule(outer, std::string(kept), RuleEffect::Elsewhere, fmt::format("Configuration files here are served by the {} root, with their secrets masked", inner.Id));
                continue;
            }
            std::string const base = Anchored(*relative);
            if (inner.Filtered && !outerDraft.Filtered)
            {
                for (std::string_view const kept : { "/*.conf", "/*.conf.dist", "/conf.d/" })
                    Rule(outer, base + std::string(kept), RuleEffect::Elsewhere, fmt::format("Configuration files here are served by the {} root, with their secrets masked", inner.Id));
                continue;
            }
            if (!inner.Filtered)
                Rule(outer, base + "/", RuleEffect::Elsewhere, Owner(inner.Id));
        }
    }

    for (auto const& [id, patterns] : inputs.OperatorRules)
    {
        auto const root = std::find_if(set.Roots.begin(), set.Roots.end(), [&id](FileRoot const& candidate) { return candidate.Id == id; });
        if (root == set.Roots.end())
        {
            if (!patterns.empty())
                set.Notes.push_back(fmt::format("{} protected pattern(s) are kept for a root named {}, which this machine does not have", patterns.size(), id));
            continue;
        }
        for (std::size_t index = 0; index < patterns.size(); ++index)
        {
            std::string error;
            if (!root->Rules.Add(patterns[index], RuleEffect::Hide, RuleOrigin::Operator, "An operator protected this path", error))
                errors.push_back(fmt::format("The {} root's protected pattern {} ({}) cannot be used: {}", id, index + 1, Ambrose::ForLog(patterns[index], 128), error));
        }
    }
    return set;
}

bool FileRoots::Rebuild(FileRootInputs const& inputs, std::vector<std::string>& errors)
{
    std::lock_guard const lock(_building);
    std::vector<std::string> found;
    FileRootSet next = FileRootSet::Build(inputs, found);
    if (!found.empty())
    {
        errors.insert(errors.end(), found.begin(), found.end());
        return false;
    }
    _set.Replace(std::move(next));
    return true;
}
