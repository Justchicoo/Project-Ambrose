/*
 * Project Ambrose by Imjustchico
 * Tests the file routes through the admin router with a real panel store and signed-in owner and viewer sessions: a traversal, an absolute path, an encoded traversal, a device name and a link leaving the root, as the path or as a folder on it, are each refused with 403 and recorded with the host path they would have reached, which the answer never carries; a client install and the extracted client data hand no byte to an owner or a viewer whatever the path; a configuration file, or a copy of one, read without the right to see secrets shows every secret masked and names the keys, while an owner who asks sees them and the reveal is recorded without a value; a protected path, built in or an owner's, is refused for listing, reading and every write; a listing carries its policy, paging, sorting and filter; a read is a window cut on a character boundary with an entity tag when the file fits whole; a token the supervisor relays is held to the rights it was forwarded; and every route asks for a permission the panel holds.
 */

#include "AdminAuth.h"
#include "AdminRouter.h"
#include "FileJail.h"
#include "FileLinks.h"
#include "FileRoots.h"
#include "FilesService.h"
#include "LogTestDirectory.h"
#include "PanelAudit.h"
#include "PanelFileRules.h"
#include "PanelPermissions.h"
#include "PanelStore.h"
#include "Settings.h"
#include "SourceFolder.h"
#include "SpaceGuard.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    using Ambrose::FileOperation;
    using Json = nlohmann::json;

    constexpr std::string_view Owner = "owner";
    constexpr std::string_view Viewer = "viewer";

    std::string const TokenValue = "0123456789abcdef0123456789abcdef";
    std::string const PanelTokenValue = "fedcba9876543210fedcba9876543210";
    std::string const DatabasePassword = "hunter2-not-for-browsers";
    std::string const VerifierKey = std::string(64, 'a');

    void WriteFile(std::filesystem::path const& path, std::string const& contents)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary | std::ios::trunc) << contents;
    }

    class TestSessions final : public SessionSource
    {
    public:
        std::optional<SessionHolder> Hold(std::string_view secret) override
        {
            if (secret == Owner)
                return SessionHolder{ "csrf-owner", "user:1" };
            if (secret == Viewer)
                return SessionHolder{ "csrf-viewer", "user:2" };
            return std::nullopt;
        }
    };

    bool Mentions(Json const& value, std::string const& text)
    {
        if (value.is_string())
            return value.get_ref<std::string const&>().find(text) != std::string::npos;
        if (value.is_object() || value.is_array())
            for (Json const& child : value)
                if (Mentions(child, text))
                    return true;
        return false;
    }

    class Rig
    {
    public:
        Rig() : Router(Auth), Rules(Store)
        {
        }

        bool Open()
        {
            Base = FileRootSet::Normal(Directory.Path());
            Inputs.DataFolder = Base / "data";
            Inputs.InstallFolder = Base / "install";
            Inputs.ConfigFile = Base / "config" / "supervisor.conf";
            Inputs.SqlCustomFolder = Base / "source" / "data" / "sql" / "custom";
            Inputs.LogsFolder = Base / "logs";
            Inputs.BackupsFolder = Base / "data" / "backups";
            Inputs.StoreFile = Base / "data" / "panel" / "panel.sqlite3";
            Inputs.KeyringFile = Base / "data" / "keyring";
            Inputs.ClientInstalls = { Base / "client" };
            for (std::filesystem::path const& folder : { Inputs.InstallFolder, Inputs.SqlCustomFolder, Inputs.LogsFolder, Inputs.BackupsFolder, Base / "outside" })
                std::filesystem::create_directories(folder);
            WriteFile(Inputs.ConfigFile, "# Project Ambrose by Imjustchico\n# A supervisor configuration for the file route tests.\nAdmin.Token = " + TokenValue
                + "\r\nPanel.Token = \"" + PanelTokenValue + "\"\nLoginDatabaseInfo = 127.0.0.1;3306;ambrose;" + DatabasePassword + ";ambrose_login\nAccount.VerifierKeys = 1:" + VerifierKey
                + "\nSupervisor.Apps = gameserver\n");
            WriteFile(Inputs.DataFolder / "types" / "r806919.types.json", "{\"types\":[]}");
            WriteFile(Inputs.DataFolder / "supervisor" / "state.json", "{}");
            WriteFile(Inputs.DataFolder / "admin" / "gameserver.token", TokenValue);
            WriteFile(Inputs.KeyringFile, "keys");
            WriteFile(Inputs.DataFolder / "notes" / "today.txt", "private");
            WriteFile(Base / "outside" / "secret.txt", "outside every root");
            WriteFile(Base / "client" / "Bin" / "WizardGraphicalClient.exe", std::string("MZ\0\0client", 10));
            std::string why;
            if (!FileLinks::MakeFolderLink(Inputs.DataFolder / "escape", Base / "outside", why))
            {
                ADD_FAILURE() << why;
                return false;
            }

            std::vector<std::string> warnings;
            std::string error;
            if (!Store.Open(Inputs.StoreFile, Ambrose::FindSourceFolder(), warnings, error))
            {
                ADD_FAILURE() << error;
                return false;
            }
            FilesHooks hooks;
            hooks.Record = [this](AuditEvent const& event, std::string& failure) { return PanelAudit::Record(Store, event, {}, failure); };
            hooks.NameOf = [](AdminRequest const& request) { return request.Principal == "user:1" ? std::string("Merle") : std::string("Wren"); };
            hooks.SaveRules = [this](AdminRequest const&, AuditEvent const& event, std::string const& root, std::vector<std::string> const& patterns, std::string& failure)
            {
                return PanelAudit::Record(Store, event, [this, &root, &patterns](std::string& inner) { return Rules.Replace(root, patterns, std::nullopt, inner); }, failure);
            };
            hooks.Rebuild = [this](std::vector<std::string>& errors) { return Rebuild(errors); };
            Files = std::make_unique<FilesService>(Roots, Space, hooks);
            Router.SetPermissionCheck([](AdminRequest const& request, std::string_view permission)
            {
                if (request.Principal != "user:1" && request.Principal != "user:2")
                    return PermissionVerdict::Forbidden;
                PanelRole const role = request.Principal == "user:1" ? PanelRole::Owner : PanelRole::Viewer;
                return PanelPermissions::RoleAllows(role, permission) ? PermissionVerdict::Allowed : PermissionVerdict::Forbidden;
            });
            Router.SetPermissionKnown([](std::string_view permission) { return PanelPermissions::Holds(permission); });
            Router.SetBrowserAccess(AdminBrowserAccess{ &Sessions, "ambrose_files", false });
            Files->Register(Router);
            std::vector<std::string> errors;
            if (!Rebuild(errors))
            {
                ADD_FAILURE() << errors.front();
                return false;
            }
            return true;
        }

        bool Rebuild(std::vector<std::string>& errors)
        {
            std::string error;
            if (!Rules.Read(Inputs.OperatorRules, error))
            {
                errors.push_back(error);
                return false;
            }
            return Roots.Rebuild(Inputs, errors);
        }

        AdminRequest Request(std::string_view who, std::string method, std::string path, std::string query = {}, std::string body = {}) const
        {
            AdminRequest request;
            request.Method = std::move(method);
            request.Path = std::move(path);
            request.RemoteAddress = "127.0.0.1";
            request.Host = "127.0.0.1";
            request.Origin = "http://127.0.0.1";
            request.UserAgent = "file route tests";
            request.Cookie = "ambrose_files=" + std::string(who);
            request.Csrf = who == Owner ? "csrf-owner" : "csrf-viewer";
            request.Body = std::move(body);
            request.RawQuery = query;
            std::string_view rest(query);
            while (!rest.empty())
            {
                std::size_t const separator = rest.find('&');
                std::string_view const pair = rest.substr(0, separator);
                std::size_t const equals = pair.find('=');
                std::optional<std::string> const value = Ambrose::JailPaths::PercentDecode(equals == std::string_view::npos ? std::string_view() : pair.substr(equals + 1));
                if (value)
                    request.QueryValues[std::string(pair.substr(0, equals))] = *value;
                if (separator == std::string_view::npos)
                    break;
                rest.remove_prefix(separator + 1);
            }
            return request;
        }

        AdminResponse Get(std::string_view who, std::string path, std::string query = {}) const
        {
            return Router.Dispatch(Request(who, "GET", std::move(path), std::move(query)));
        }

        AdminRequest Signed(std::string_view who) const
        {
            AdminRequest request = Request(who, "GET", "/api/files");
            request.Principal = who == Owner ? "user:1" : "user:2";
            return request;
        }

        std::vector<Json> Recorded(std::string_view name)
        {
            std::vector<Json> rows;
            std::string error;
            std::optional<PanelStore::Statement> statement = Store.Prepare("SELECT properties, result, reason, actor_name FROM audit_event WHERE name = ?1 ORDER BY id", error);
            EXPECT_TRUE(statement.has_value()) << error;
            if (!statement)
                return rows;
            statement->Bind(1, name);
            while (statement->Step(error))
            {
                Json row;
                row["properties"] = Json::parse(statement->Text(0), nullptr, false);
                row["result"] = statement->Text(1);
                row["reason"] = statement->Text(2);
                row["actor"] = statement->Text(3);
                rows.push_back(std::move(row));
            }
            return rows;
        }

        std::filesystem::path HostOf(std::string_view root) const
        {
            FileRoots::Snapshot const set = Roots.Get();
            FileRoot const* const found = set->Find(root);
            return found ? found->HostPath() : std::filesystem::path();
        }

        LogTestDirectory Directory;
        std::filesystem::path Base;
        PanelStore Store;
        FileRoots Roots;
        Ambrose::SpaceGuard Space;
        AdminAuth Auth{ 10, 1.0 };
        TestSessions Sessions;
        AdminRouter Router;
        PanelFileRules Rules;
        FileRootInputs Inputs;
        std::unique_ptr<FilesService> Files;
    };
}

TEST(FilesServiceTest, TraversalAbsoluteEncodedDeviceAndEscapingLinkRequestsAre403AndAuditedWithTheResolvedPath)
{
    Rig rig;
    ASSERT_TRUE(rig.Open());
    std::filesystem::path const root = rig.HostOf("data");
    ASSERT_FALSE(root.empty());
    std::string const rootText = Ambrose::FileJail::HostText(root);

    struct Asked
    {
        std::string Route = {};
        std::string Query = {};
        std::string Code = {};
        std::filesystem::path Resolved = {};
    };
    std::vector<Asked> const asked{
        { "content", "path=../x", "traversal", (root.parent_path() / "x").lexically_normal() },
        { "content", "path=/etc/passwd", "absolute", std::filesystem::path("/etc/passwd").lexically_normal() },
        { "content", "path=C:/Windows", "absolute", std::filesystem::path("C:/Windows").lexically_normal() },
        { "content", "path=%2e%2e%2fx", "traversal", (root.parent_path() / "x").lexically_normal() },
        { "content", "path=CON", "device_name", (root / "CON").lexically_normal() },
        { "content", "path=nul.txt", "device_name", (root / "nul.txt").lexically_normal() },
        { "list", "path=escape", "link", (rig.Base / "outside").lexically_normal() },
        { "list", "path=escape/deeper", "link", (rig.Base / "outside").lexically_normal() },
    };
    for (Asked const& one : asked)
    {
        AdminResponse const response = rig.Get(Owner, "/api/files/data/" + one.Route, one.Query);
        EXPECT_EQ(response.Status, 403) << one.Query << ": " << response.Body;
        Json const body = Json::parse(response.Body, nullptr, false);
        ASSERT_TRUE(body.is_object()) << response.Body;
        EXPECT_EQ(body.value("error", std::string()), one.Code) << one.Query;
        EXPECT_EQ(body.value("root", std::string()), "data") << one.Query;
        EXPECT_FALSE(Mentions(body, rootText)) << one.Query << " names the root's host path: " << response.Body;
        EXPECT_FALSE(Mentions(body, Ambrose::FileJail::HostText(one.Resolved))) << one.Query << " names the host path: " << response.Body;
    }

    std::vector<Json> const rows = rig.Recorded("file:path.refused");
    ASSERT_EQ(rows.size(), asked.size());
    for (std::size_t index = 0; index < asked.size(); ++index)
    {
        Json const& properties = rows[index]["properties"];
        EXPECT_EQ(properties.value("root", std::string()), "data");
        EXPECT_EQ(properties.value("reason", std::string()), asked[index].Code);
        EXPECT_EQ(std::filesystem::path(properties.value("resolved", std::string())).lexically_normal(), asked[index].Resolved) << asked[index].Query;
        EXPECT_EQ(rows[index]["result"], "refused");
        EXPECT_EQ(rows[index]["actor"], "Merle");
    }
}

TEST(FilesServiceTest, AClientDerivedRootRefusesEveryBytePathForAnOwnerAndAViewer)
{
    Rig rig;
    ASSERT_TRUE(rig.Open());
    for (std::string_view const who : { Owner, Viewer })
    {
        AdminResponse const listed = rig.Get(who, "/api/files/client/list", "path=Bin");
        EXPECT_EQ(listed.Status, 200) << "the install is listed: " << listed.Body;
        AdminResponse const read = rig.Get(who, "/api/files/client/content", "path=Bin/WizardGraphicalClient.exe");
        EXPECT_EQ(read.Status, 403) << read.Body;
        Json const body = Json::parse(read.Body, nullptr, false);
        EXPECT_EQ(body.value("error", std::string()), "client_derived");
        EXPECT_EQ(body.value("root", std::string()), "client");
        Json refusal = body;
        refusal.erase("request_id");
        EXPECT_EQ(refusal.dump().find("MZ"), std::string::npos) << "no byte of the program reaches the answer: " << read.Body;

        AdminResponse const types = rig.Get(who, "/api/files/data/content", "path=types/r806919.types.json");
        EXPECT_EQ(types.Status, 403) << types.Body;
        EXPECT_EQ(Json::parse(types.Body, nullptr, false).value("error", std::string()), "client_derived");

        for (FileOperation const operation : { FileOperation::Read, FileOperation::Preview, FileOperation::Download, FileOperation::Archive, FileOperation::Share,
                 FileOperation::Extract, FileOperation::Sftp, FileOperation::Pull })
        {
            for (auto const& [root, path] : { std::pair<std::string_view, std::string_view>{ "client", "Bin/WizardGraphicalClient.exe" },
                     std::pair<std::string_view, std::string_view>{ "data", "types/r806919.types.json" } })
            {
                FileDecision const decision = rig.Files->Authorize(rig.Signed(who), rig.Router, root, path, operation);
                EXPECT_FALSE(decision.Allowed) << who << " " << Ambrose::FilePolicy::NameOf(operation) << " " << root;
                if (who == Owner)
                {
                    EXPECT_EQ(decision.Code, "client_derived") << Ambrose::FilePolicy::NameOf(operation) << " " << root;
                }
            }
        }
    }
}

TEST(FilesServiceTest, AConfFileReadWithoutTheSecretsRightShowsEverySecretRedacted)
{
    Rig rig;
    ASSERT_TRUE(rig.Open());
    std::vector<std::string> const secrets{ TokenValue, PanelTokenValue, DatabasePassword, VerifierKey };

    AdminResponse const viewer = rig.Get(Viewer, "/api/files/config/content", "path=supervisor.conf&reveal=1");
    ASSERT_EQ(viewer.Status, 200) << viewer.Body;
    for (std::string const& secret : secrets)
        EXPECT_EQ(viewer.Body.find(secret), std::string::npos) << "a viewer asking to see secrets still sees them masked";
    Json const masked = Json::parse(viewer.Body);
    std::string const text = masked["text"].get<std::string>();
    EXPECT_NE(text.find("Supervisor.Apps = gameserver"), std::string::npos) << "values that are not secrets are kept";
    EXPECT_NE(text.find("127.0.0.1;3306;ambrose;***;ambrose_login"), std::string::npos) << text;
    EXPECT_NE(text.find("Panel.Token = \"***\""), std::string::npos) << "quotes are kept around a masked value: " << text;
    EXPECT_NE(text.find("\r\n"), std::string::npos) << "line endings are kept";
    EXPECT_TRUE(masked["redacted"].get<bool>());
    EXPECT_FALSE(masked["revealed"].get<bool>());
    EXPECT_EQ(masked["redacted_keys"], (Json{ "Admin.Token", "Panel.Token", "LoginDatabaseInfo", "Account.VerifierKeys" }));
    EXPECT_TRUE(rig.Recorded("settings:secret.revealed").empty());

    AdminResponse const plain = rig.Get(Owner, "/api/files/config/content", "path=supervisor.conf");
    ASSERT_EQ(plain.Status, 200) << plain.Body;
    EXPECT_EQ(plain.Body.find(DatabasePassword), std::string::npos) << "an owner who did not ask sees the mask too";

    AdminResponse const owner = rig.Get(Owner, "/api/files/config/content", "path=supervisor.conf&reveal=1");
    ASSERT_EQ(owner.Status, 200) << owner.Body;
    Json const shown = Json::parse(owner.Body);
    for (std::string const& secret : secrets)
        EXPECT_NE(shown["text"].get<std::string>().find(secret), std::string::npos);
    EXPECT_TRUE(shown["revealed"].get<bool>());
    EXPECT_EQ(shown["revealed_keys"].size(), 4u);

    std::vector<Json> const reveals = rig.Recorded("settings:secret.revealed");
    ASSERT_EQ(reveals.size(), 1u);
    EXPECT_EQ(reveals[0]["properties"]["root"], "config");
    EXPECT_EQ(reveals[0]["properties"]["path"], "supervisor.conf");
    EXPECT_EQ(reveals[0]["properties"]["keys"].size(), 4u);
    for (std::string const& secret : secrets)
        EXPECT_FALSE(Mentions(reveals[0], secret)) << "a reveal is recorded without a value";

    WriteFile(rig.Inputs.LogsFolder / "supervisor.conf.bak", "Admin.Token = " + TokenValue + "\n");
    AdminResponse const copy = rig.Get(Viewer, "/api/files/logs/content", "path=supervisor.conf.bak");
    ASSERT_EQ(copy.Status, 200) << copy.Body;
    EXPECT_EQ(copy.Body.find(TokenValue), std::string::npos) << "a copy of a configuration file is masked too";
    EXPECT_TRUE(Json::parse(copy.Body)["redacted"].get<bool>());

    WriteFile(rig.Inputs.ConfigFile.parent_path() / "big.conf", std::string(static_cast<std::size_t>(sSettings.Get<uint64>("Files.ReadMaxBytes")) + 1, '#'));
    EXPECT_EQ(rig.Get(Owner, "/api/files/config/content", "path=big.conf").Status, 413) << "a configuration file too large to mask in one read is not shown";
}

TEST(FilesServiceTest, AProtectedPathIsRefusedForListingReadingAndEveryWrite)
{
    Rig rig;
    ASSERT_TRUE(rig.Open());
    auto const names = [&rig](std::string_view who, std::string const& query)
    {
        AdminResponse const response = rig.Get(who, "/api/files/data/list", query);
        EXPECT_EQ(response.Status, 200) << response.Body;
        std::vector<std::string> found;
        Json const body = Json::parse(response.Body, nullptr, false);
        if (body.is_object())
            for (Json const& entry : body["entries"])
                found.push_back(entry["name"].get<std::string>());
        return found;
    };
    auto const holds = [](std::vector<std::string> const& list, std::string_view name) { return std::find(list.begin(), list.end(), name) != list.end(); };

    std::vector<std::string> const top = names(Owner, "");
    EXPECT_FALSE(holds(top, "panel"));
    EXPECT_FALSE(holds(top, "admin"));
    EXPECT_FALSE(holds(top, "keyring"));
    EXPECT_TRUE(holds(top, "types"));
    EXPECT_TRUE(holds(top, "supervisor"));
    EXPECT_TRUE(holds(top, "notes"));

    for (auto const& [route, query] : { std::pair<std::string, std::string>{ "list", "path=panel" }, std::pair<std::string, std::string>{ "content", "path=panel/panel.sqlite3" } })
    {
        AdminResponse const response = rig.Get(Owner, "/api/files/data/" + route, query);
        EXPECT_EQ(response.Status, 403) << response.Body;
        Json const body = Json::parse(response.Body, nullptr, false);
        EXPECT_EQ(body.value("error", std::string()), "protected");
        EXPECT_EQ(body.value("rule", std::string()), "/panel/");
    }

    std::vector<FileOperation> const every{ FileOperation::Write, FileOperation::Upload, FileOperation::Create, FileOperation::Rename, FileOperation::Move, FileOperation::Copy,
        FileOperation::Delete, FileOperation::Permissions, FileOperation::Truncate, FileOperation::Extract, FileOperation::Download, FileOperation::Archive };
    for (FileOperation const operation : every)
    {
        FileDecision const decision = rig.Files->Authorize(rig.Signed(Owner), rig.Router, "data", "panel/panel.sqlite3", operation);
        EXPECT_FALSE(decision.Allowed) << Ambrose::FilePolicy::NameOf(operation);
        EXPECT_EQ(decision.Rule, "/panel/") << Ambrose::FilePolicy::NameOf(operation);
    }

    AdminResponse const refused = rig.Router.Dispatch(rig.Request(Viewer, "PUT", "/api/files/data/rules", {}, "{\"patterns\":[\"notes/\"]}"));
    EXPECT_EQ(refused.Status, 403) << "only an owner changes protected paths: " << refused.Body;
    AdminResponse const invalid = rig.Router.Dispatch(rig.Request(Owner, "PUT", "/api/files/data/rules", {}, "{\"patterns\":[\"# a note\",\"notes/\",\"[broken\"]}"));
    ASSERT_EQ(invalid.Status, 422) << invalid.Body;
    EXPECT_NE(Json::parse(invalid.Body)["fields"]["patterns"].get<std::string>().find("Line 3"), std::string::npos) << invalid.Body;
    AdminResponse const saved = rig.Router.Dispatch(rig.Request(Owner, "PUT", "/api/files/data/rules", {}, "{\"patterns\":[\"# private notes\",\"notes/\"],\"reason\":\"kept private\"}"));
    ASSERT_EQ(saved.Status, 200) << saved.Body;
    Json const answer = Json::parse(saved.Body);
    EXPECT_EQ(answer["patterns"], (Json{ "notes/" }));
    EXPECT_TRUE(answer["rebuilt"].get<bool>());
    ASSERT_EQ(rig.Recorded("file:rules.changed").size(), 1u);
    EXPECT_EQ(rig.Recorded("file:rules.changed")[0]["reason"], "kept private");

    EXPECT_FALSE(holds(names(Owner, ""), "notes")) << "an owner's pattern hides the folder from the listing";
    AdminResponse const listed = rig.Get(Viewer, "/api/files/data/list", "path=notes");
    EXPECT_EQ(listed.Status, 403);
    EXPECT_EQ(Json::parse(listed.Body, nullptr, false).value("rule", std::string()), "notes/");
    AdminResponse const read = rig.Get(Owner, "/api/files/data/content", "path=notes/today.txt");
    EXPECT_EQ(read.Status, 403);
    EXPECT_EQ(read.Body.find("private\""), std::string::npos);
    for (FileOperation const operation : every)
    {
        FileDecision const decision = rig.Files->Authorize(rig.Signed(Owner), rig.Router, "data", "notes/today.txt", operation);
        EXPECT_FALSE(decision.Allowed) << Ambrose::FilePolicy::NameOf(operation);
        EXPECT_EQ(decision.Rule, "notes/") << Ambrose::FilePolicy::NameOf(operation);
    }
    AdminResponse const rules = rig.Get(Viewer, "/api/files/data/rules");
    ASSERT_EQ(rules.Status, 200) << rules.Body;
    EXPECT_EQ(Json::parse(rules.Body)["patterns"], (Json{ "notes/" }));
}

TEST(FilesServiceTest, ListsAFolderWithItsPolicyPagedSortedAndFiltered)
{
    Rig rig;
    ASSERT_TRUE(rig.Open());
    WriteFile(rig.Inputs.LogsFolder / "Supervisor.log", std::string(30, 's'));
    WriteFile(rig.Inputs.LogsFolder / "gameserver.log", std::string(10, 'g'));
    WriteFile(rig.Inputs.LogsFolder / "loginserver.log", std::string(20, 'l'));
    WriteFile(rig.Inputs.LogsFolder / "readme.txt", "r");
    std::filesystem::create_directories(rig.Inputs.LogsFolder / "apps");

    AdminResponse const response = rig.Get(Viewer, "/api/files/logs/list", "sort=size&order=desc&q=LOG&limit=2");
    ASSERT_EQ(response.Status, 200) << response.Body;
    Json const body = Json::parse(response.Body);
    EXPECT_EQ(body["root"], "logs");
    EXPECT_EQ(body["path"], "");
    EXPECT_EQ(body["total"], 3) << "the filter runs before the count";
    EXPECT_EQ(body["limit"], 2);
    EXPECT_FALSE(body["truncated"].get<bool>());
    ASSERT_EQ(body["entries"].size(), 2u);
    EXPECT_EQ(body["entries"][0]["name"], "Supervisor.log");
    EXPECT_EQ(body["entries"][0]["kind"], "file");
    EXPECT_EQ(body["entries"][0]["size"], 30);
    EXPECT_TRUE(body["entries"][0]["openable"].get<bool>());
    EXPECT_TRUE(body["entries"][0]["modified_ms"].is_number());
    EXPECT_EQ(body["entries"][1]["name"], "loginserver.log");
    EXPECT_FALSE(body["policy"]["operations"]["write"]["allowed"].get<bool>());
    EXPECT_NE(body["policy"]["operations"]["write"]["reason"].get<std::string>().find("truncate, rotate or trash"), std::string::npos);
    EXPECT_TRUE(body["policy"]["operations"]["truncate"]["allowed"].get<bool>());

    AdminResponse const folders = rig.Get(Viewer, "/api/files/logs/list", "");
    Json const all = Json::parse(folders.Body);
    ASSERT_FALSE(all["entries"].empty());
    EXPECT_EQ(all["entries"][0]["name"], "apps") << "folders come first";

    EXPECT_EQ(rig.Get(Viewer, "/api/files/logs/list", "limit=0").Status, 422);
    EXPECT_EQ(rig.Get(Viewer, "/api/files/logs/list", "sort=colour").Status, 422);
    EXPECT_EQ(rig.Get(Viewer, "/api/files/logs/list", "path=Supervisor.log").Status, 409);
    EXPECT_EQ(rig.Get(Viewer, "/api/files/logs/list", "path=missing").Status, 404);
    EXPECT_EQ(rig.Get(Viewer, "/api/files/nowhere/list", "").Status, 404);

    AdminResponse const roots = rig.Get(Viewer, "/api/files");
    ASSERT_EQ(roots.Status, 200) << roots.Body;
    Json const listing = Json::parse(roots.Body);
    EXPECT_EQ(listing["schema"], FilesService::SchemaVersion);
    bool sawClient = false;
    for (Json const& root : listing["roots"])
    {
        EXPECT_FALSE(Mentions(root, Ambrose::FileJail::HostText(rig.Base))) << "no root answers with its host path";
        if (root["id"] == "client")
        {
            sawClient = true;
            EXPECT_TRUE(root["client_derived"].get<bool>());
            EXPECT_FALSE(root["policy"]["operations"]["download"]["allowed"].get<bool>());
        }
    }
    EXPECT_TRUE(sawClient);
}

TEST(FilesServiceTest, ReadsAWindowCutOnACharacterBoundaryWithAnEntityTagWhenTheFileFitsWhole)
{
    Rig rig;
    ASSERT_TRUE(rig.Open());
    std::string const small = "\xEF\xBB\xBFline one\r\nline two\r\n";
    WriteFile(rig.Inputs.LogsFolder / "small.log", small);
    AdminResponse const whole = rig.Get(Viewer, "/api/files/logs/content", "path=small.log");
    ASSERT_EQ(whole.Status, 200) << whole.Body;
    Json const body = Json::parse(whole.Body);
    EXPECT_EQ(body["text"], small) << "the byte order mark and CRLF are kept";
    EXPECT_TRUE(body["bom"].get<bool>());
    EXPECT_TRUE(body["eof"].get<bool>());
    ASSERT_TRUE(body["etag"].is_string());
    std::string const tag = body["etag"].get<std::string>();
    EXPECT_EQ(tag.front(), '"');
    EXPECT_NE(std::find(whole.Headers.begin(), whole.Headers.end(), std::pair<std::string, std::string>("ETag", tag)), whole.Headers.end());

    WriteFile(rig.Inputs.LogsFolder / "binary.dat", std::string("a\0b", 3));
    Json const binary = Json::parse(rig.Get(Viewer, "/api/files/logs/content", "path=binary.dat").Body);
    EXPECT_TRUE(binary["binary"].get<bool>());
    EXPECT_TRUE(binary["text"].is_null());

    std::size_t const most = static_cast<std::size_t>(sSettings.Get<uint64>("Files.ReadMaxBytes"));
    std::string large(most - 1, 'x');
    large += "\xE2\x82\xAC";
    large += "tail";
    WriteFile(rig.Inputs.LogsFolder / "large.log", large);
    AdminResponse const first = rig.Get(Viewer, "/api/files/logs/content", "path=large.log");
    ASSERT_EQ(first.Status, 200);
    Json const window = Json::parse(first.Body);
    EXPECT_EQ(window["length"], most - 1) << "the window stops before a character it would cut";
    EXPECT_EQ(window["next_offset"], most - 1);
    EXPECT_FALSE(window["eof"].get<bool>());
    EXPECT_TRUE(window["etag"].is_null()) << "a file larger than one read carries no content hash";
    AdminResponse const second = rig.Get(Viewer, "/api/files/logs/content", "path=large.log&offset=" + std::to_string(most - 1));
    ASSERT_EQ(second.Status, 200);
    Json const rest = Json::parse(second.Body);
    EXPECT_EQ(rest["text"], "\xE2\x82\xAC" "tail");
    EXPECT_TRUE(rest["eof"].get<bool>());
}

TEST(FilesServiceTest, ARelayedTokenIsHeldToTheRightsItWasForwarded)
{
    AdminAuth auth(10, 1.0);
    AdminRouter router(auth);
    FileRoots roots;
    Ambrose::SpaceGuard space;
    FilesService files(roots, space);
    AdminRequest request;
    request.Principal = "token";
    request.ForwardedGrants = std::set<std::string, std::less<>>{ "files.list" };
    FileDecision const read = files.Authorize(request, router, "data", "", FileOperation::Read);
    EXPECT_FALSE(read.Allowed);
    EXPECT_EQ(read.Status, 403);
    EXPECT_EQ(read.Code, "forbidden");
    FileDecision const listed = files.Authorize(request, router, "data", "", FileOperation::List);
    EXPECT_EQ(listed.Code, "unknown_root") << "a forwarded right passes the gate, and an empty set of roots has no data root";
}

TEST(FilesServiceTest, EveryRouteAsksForAPermissionThePanelHolds)
{
    AdminAuth auth(10, 1.0);
    AdminRouter router(auth);
    router.SetPermissionKnown([](std::string_view permission) { return PanelPermissions::Holds(permission); });
    FileRoots roots;
    Ambrose::SpaceGuard space;
    FilesService files(roots, space);
    files.Register(router);
    EXPECT_TRUE(router.RouteProblems().empty());
    EXPECT_TRUE(router.RefusedRoutes().empty());
    EXPECT_TRUE(router.Has("GET", "/api/files"));
    for (Ambrose::FileOperation const operation : Ambrose::FilePolicy::Operations())
        EXPECT_TRUE(PanelPermissions::Holds(FilesService::PermissionFor(operation))) << Ambrose::FilePolicy::NameOf(operation);
    EXPECT_EQ(FilesService::PermissionFor(FileOperation::Share), "files.download");
    EXPECT_EQ(FilesService::PermissionFor(FileOperation::Extract), "files.archive");
    EXPECT_EQ(FilesService::PermissionFor(FileOperation::Truncate), "files.write");
    EXPECT_TRUE(FilesService::IsConf("supervisor.conf"));
    EXPECT_TRUE(FilesService::IsConf("gameserver.conf.dist"));
    EXPECT_TRUE(FilesService::IsConf("supervisor.conf.bak")) << "a copy of a configuration file holds the same secrets";
    EXPECT_TRUE(FilesService::IsConf("Supervisor.CONF~"));
    EXPECT_FALSE(FilesService::IsConf("notes.config"));
    EXPECT_FALSE(FilesService::IsConf(".conf"));
}
