/*
 * Project Ambrose by Imjustchico
 * The desktop shell's smoke program, run by the ShellSmoke test with real web view windows kept off screen. available says whether this machine has a web view; own opens the launcher's page from memory and reports its origin, whether it is a secure context and whether local storage keeps a value; remote serves a page from two loopback listeners, opens a view bound to the first, has the page post to the host channel, set a cookie, navigate to another origin and open a new window, then opens a view bound to the second listener and deletes the first profile, and checks that no answer came, both requests went to the system browser without either reaching the other listener, the view stayed put, the second profile saw no cookie and the deleted one left no file; pin serves the first certificate it is given over TLS on loopback and expects a view pinned to it to load, then writes the second over the same files, as --panel-self-signed rewrites them, reloads, and expects a view with the same pin, and one with none, to be refused with the fingerprints named. It prints what it saw and exits 0 when every check holds, 1 when one fails and 3 when this machine has no web view.
 */

#include "AdminServer.h"
#include "ConfigMgr.h"
#include "LauncherWindow.h"
#include "ListenerSettings.h"
#include "Log.h"
#include "ShellProfiles.h"
#include "ShellWindow.h"
#include "TlsCertificate.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace
{
    constexpr int Passed = 0;
    constexpr int Failed = 1;
    constexpr int Usage = 2;
    constexpr int NoWebView = 3;
    constexpr char const* PageTitle = "Loopback page";

    struct Seen
    {
        std::mutex Lock;
        std::vector<std::string> External;
        std::vector<std::string> Lines;
        int Answers = 0;
    };

    bool Check(bool holds, std::string const& what)
    {
        std::cout << (holds ? "ok: " : "FAILED: ") << what << '\n';
        return holds;
    }

    std::filesystem::path Utf8Path(std::string const& text)
    {
        return ConfigMgr::PathFromUtf8(text);
    }

    std::vector<std::string> RunWindow(ShellWindowOptions options, std::vector<std::string> scripts, Seen& seen, std::string& error)
    {
        std::vector<std::string> results;
        options.OffScreen = true;
        options.Log = [&seen](std::string const& line)
        {
            std::lock_guard const lock(seen.Lock);
            seen.Lines.push_back(line);
            std::cout << "log: " << line << '\n';
        };
        options.OpenExternal = [&seen](std::string const& url)
        {
            std::lock_guard const lock(seen.Lock);
            seen.External.push_back(url);
        };
        options.Probe = ShellProbe{};
        options.Probe->Scripts = std::move(scripts);
        options.Probe->Done = [&results](std::vector<std::string> const& found) { results = found; };
        if (!ShellWindow::Show(options, error))
            std::cout << "window: " << error << '\n';
        for (std::string const& result : results)
            std::cout << "probe: " << result << '\n';
        return results;
    }

    std::string Text(std::vector<std::string> const& results, std::size_t index)
    {
        if (index >= results.size())
            return std::string();
        nlohmann::json const value = nlohmann::json::parse(results[index], nullptr, false);
        return value.is_string() ? value.get<std::string>() : results[index];
    }

    std::unique_ptr<AdminServer> Listen(Log& log, std::filesystem::path const& work, std::string const& name, std::filesystem::path const& certificate,
        std::filesystem::path const& key, ListenerSettings& settings)
    {
        std::filesystem::path const site = work / (name + "-site");
        std::filesystem::create_directories(site);
        std::ofstream(site / "index.html", std::ios::binary)
            << "<!doctype html><html><head><meta charset=\"utf-8\"><title>" << PageTitle << "</title><script src=\"page.js\"></script></head><body></body></html>";
        std::ofstream(site / "page.js", std::ios::binary)
            << "window.replies = 0;"
            << "if (window.chrome && window.chrome.webview) { window.chrome.webview.addEventListener('message', () => { window.replies++; }); }";
        settings.Enable = true;
        settings.BindIp = "127.0.0.1";
        settings.Port = 0;
        settings.Token = "shell-smoke-token-" + name + "-0123456789";
        settings.DashboardDir = site;
        settings.CertificateFile = certificate;
        settings.PrivateKeyFile = key;
        auto server = std::make_unique<AdminServer>(log, "shell-smoke-" + name, work / (name + "-data"), work);
        std::string error;
        if (!server->Start(settings, error))
        {
            std::cout << "listener " << name << ": " << error << '\n';
            return nullptr;
        }
        return server;
    }

    int Own(std::filesystem::path const& data)
    {
        Seen seen;
        std::string error;
        ShellWindowOptions options = LauncherWindow::Options(data, std::filesystem::path(), nullptr);
        std::vector<std::string> const results = RunWindow(options,
            { "JSON.stringify({ secure: window.isSecureContext, origin: location.origin, storage: (() => { try { localStorage.setItem('ambrose-smoke', 'kept'); return localStorage.getItem('ambrose-smoke') === 'kept'; } catch (e) { return false; } })() })" },
            seen, error);
        nlohmann::json const report = nlohmann::json::parse(Text(results, 0), nullptr, false);
        bool ok = Check(error.empty(), "the launcher's window opened from memory");
        ok = Check(report.is_object() && report.value("origin", "") == ShellOrigin::Own(LauncherWindow::Program).Describe(), "the page is on the launcher's own origin") && ok;
        ok = Check(report.is_object() && report.value("secure", false), "the page is a secure context") && ok;
        ok = Check(report.is_object() && report.value("storage", false), "the page can write local storage") && ok;
        return ok ? Passed : Failed;
    }

    int Remote(std::filesystem::path const& work)
    {
        Log log;
        ListenerSettings firstSettings;
        ListenerSettings secondSettings;
        std::unique_ptr<AdminServer> const first = Listen(log, work, "first", {}, {}, firstSettings);
        std::unique_ptr<AdminServer> const second = Listen(log, work, "second", {}, {}, secondSettings);
        if (!first || !second)
            return Failed;
        std::string const firstOrigin = "http://127.0.0.1:" + std::to_string(first->GetPort());
        std::string const elsewhere = "http://localhost:" + std::to_string(second->GetPort()) + "/api/elsewhere";
        std::string const newWindow = "http://127.0.0.1:" + std::to_string(second->GetPort()) + "/api/window";
        std::filesystem::path const data = work / "remote-data";
        std::atomic<int> reached{ 0 };
        for (std::string const path : { "/api/elsewhere", "/api/window" })
            second->Routes().AddOpen("GET", path, [&reached](AdminRequest const&)
            {
                ++reached;
                return AdminResponse::Json(200, "{}");
            });

        Seen seen;
        std::string error;
        ShellWindowOptions options;
        options.Program = "panel";
        options.Remote = firstOrigin + "/";
        options.DataFolder = data;
        options.Answer = [&seen](std::string const&)
        {
            std::lock_guard const lock(seen.Lock);
            ++seen.Answers;
            return std::string("{\"id\":1,\"ok\":true}");
        };
        std::vector<std::string> const results = RunWindow(options,
            {
                "(() => { let posted = 'no channel'; try { if (window.chrome && window.chrome.webview) { window.chrome.webview.postMessage({ id: 1, path: '/status' }); posted = 'posted'; } } catch (e) { posted = 'refused'; } try { if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.ambrose) { window.webkit.messageHandlers.ambrose.postMessage({ id: 1 }); posted = 'posted'; } } catch (e) { posted = 'refused'; } return posted; })()",
                "document.cookie = 'smoke=first; max-age=3600; path=/'; document.cookie",
                "location.href = '" + elsewhere + "'; 'asked'",
                "window.open('" + newWindow + "'); 'opened'",
                "JSON.stringify({ href: location.href, replies: window.replies, title: document.title })",
            },
            seen, error);
        nlohmann::json const after = nlohmann::json::parse(Text(results, 4), nullptr, false);

        bool ok = Check(error.empty(), "a view bound to " + firstOrigin + " opened");
        ok = Check(seen.Answers == 0 && after.is_object() && after.value("replies", -1) == 0, "a page at a remote origin that posts to the host channel gets no answer") && ok;
        ok = Check(Text(results, 1).find("smoke=first") != std::string::npos, "the first profile keeps its cookie") && ok;
        ok = Check(after.is_object() && after.value("href", "") == firstOrigin + "/" && after.value("title", "") == PageTitle, "the view stayed on its own origin") && ok;
        ok = Check(std::find(seen.External.begin(), seen.External.end(), elsewhere) != seen.External.end(), "a navigation to another origin went to the system browser") && ok;
        ok = Check(std::find(seen.External.begin(), seen.External.end(), newWindow) != seen.External.end(), "a new window went to the system browser") && ok;
        ok = Check(reached == 0, "nothing the view was sent away from reached the other origin") && ok;

        ShellWindowOptions other;
        other.Program = "panel";
        other.Remote = "http://127.0.0.1:" + std::to_string(second->GetPort()) + "/";
        other.DataFolder = data;
        Seen otherSeen;
        std::vector<std::string> const secondResults = RunWindow(other, { "document.cookie" }, otherSeen, error);
        ok = Check(secondResults.size() == 1 && Text(secondResults, 0).find("smoke=first") == std::string::npos,
                 "a second remote profile shares no cookie with the first, though both are on 127.0.0.1") && ok;

        ShellOrigin const firstBound = *ShellOrigin::Of(firstOrigin);
        std::filesystem::path const firstProfile = ShellProfiles::FolderFor(data, firstBound);
        ok = Check(std::filesystem::exists(firstProfile), "the first profile was kept under the program's data folder") && ok;
        std::string deleteError;
        bool const deleted = ShellProfiles::Delete(data, firstBound, deleteError);
        ok = Check(deleted && !std::filesystem::exists(firstProfile), "deleting the first profile leaves no file of it " + deleteError) && ok;
        ok = Check(std::filesystem::exists(ShellProfiles::FolderFor(data, *ShellOrigin::Of(other.Remote))), "deleting one profile leaves the other") && ok;

        first->Stop();
        second->Stop();
        return ok ? Passed : Failed;
    }

    int Pin(std::filesystem::path const& work, std::filesystem::path const& firstCertificate, std::filesystem::path const& firstKey,
        std::filesystem::path const& secondCertificate, std::filesystem::path const& secondKey)
    {
        std::string error;
        TlsCertificate firstPair;
        TlsCertificate secondPair;
        if (!firstPair.Load(firstCertificate, firstKey, error) || !secondPair.Load(secondCertificate, secondKey, error))
        {
            std::cout << "certificates: " << error << '\n';
            return Failed;
        }
        std::string const firstPrint = firstPair.GetInfo().Fingerprint;
        std::string const secondPrint = secondPair.GetInfo().Fingerprint;
        if (!Check(!firstPrint.empty() && firstPrint != secondPrint, "the two certificates differ"))
            return Failed;

        std::filesystem::path const servedCertificate = work / "served-certificate.pem";
        std::filesystem::path const servedKey = work / "served-key.pem";
        std::filesystem::copy_file(firstCertificate, servedCertificate, std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy_file(firstKey, servedKey, std::filesystem::copy_options::overwrite_existing);
        Log log;
        ListenerSettings settings;
        std::unique_ptr<AdminServer> const server = Listen(log, work, "tls", servedCertificate, servedKey, settings);
        if (!server)
            return Failed;
        std::string const origin = "https://127.0.0.1:" + std::to_string(server->GetPort());

        auto pinned = [&](std::string const& pin, std::string const& folder, Seen& seen)
        {
            ShellWindowOptions options;
            options.Program = "panel";
            options.Remote = origin + "/";
            options.DataFolder = work / folder;
            options.PinFor = [pin](ShellOrigin const&) { return pin; };
            std::string windowError;
            std::vector<std::string> const results = RunWindow(options, { "document.title" }, seen, windowError);
            return Text(results, 0);
        };

        Seen accepted;
        bool ok = Check(pinned(firstPrint, "pin-first", accepted) == PageTitle, "a view pinned to the served certificate loads the page");

        std::filesystem::copy_file(secondCertificate, servedCertificate, std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy_file(secondKey, servedKey, std::filesystem::copy_options::overwrite_existing);
        ok = Check(server->Reload(settings) && server->GetFingerprint() == secondPrint, "the listener now serves the second certificate") && ok;

        Seen refused;
        std::string const title = pinned(firstPrint, "pin-second", refused);
        bool named = false;
        for (std::string const& line : refused.Lines)
            named = named || (line.find(secondPrint) != std::string::npos && line.find(firstPrint) != std::string::npos);
        ok = Check(title != PageTitle && named, "a second certificate on the same host and port is refused with both fingerprints named") && ok;

        Seen unpinned;
        std::string const bare = pinned("", "pin-none", unpinned);
        bool said = false;
        for (std::string const& line : unpinned.Lines)
            said = said || (line.find(secondPrint) != std::string::npos && line.find("no certificate is pinned") != std::string::npos);
        ok = Check(bare != PageTitle && said, "with no pin every certificate error is refused") && ok;

        server->Stop();
        return ok ? Passed : Failed;
    }
}

int main(int argc, char** argv)
{
    std::vector<std::string> const arguments(argv + 1, argv + argc);
    if (arguments.size() == 1 && arguments[0] == "available")
    {
        if (ShellWindow::Available())
            return Passed;
        std::cout << "shell smoke skipped: this machine has no web view\n";
        return NoWebView;
    }
    if (arguments.size() < 2)
    {
        std::cerr << "Usage: shell_smoke available | own <data> | remote <work> | pin <work> <cert1> <key1> <cert2> <key2>\n";
        return Usage;
    }
    if (!ShellWindow::Available())
    {
        std::cout << "shell smoke skipped: this machine has no web view\n";
        return NoWebView;
    }
    std::filesystem::path const folder = Utf8Path(arguments[1]);
    std::filesystem::create_directories(folder);
    if (arguments[0] == "own")
        return Own(folder);
    if (arguments[0] == "remote")
        return Remote(folder);
    if (arguments[0] == "pin" && arguments.size() == 6)
        return Pin(folder, Utf8Path(arguments[2]), Utf8Path(arguments[3]), Utf8Path(arguments[4]), Utf8Path(arguments[5]));
    std::cerr << "Usage: shell_smoke available | own <data> | remote <work> | pin <work> <cert1> <key1> <cert2> <key2>\n";
    return Usage;
}
