/*
 * Project Ambrose by Imjustchico
 * The shell window on a desktop that is not Windows: a GTK window holding a WebKitGTK view whose web context keeps its data and cache in the window's profile, built only where the build found webkit2gtk. The program's own page is served from memory through a scheme of its own, registered as both secure and CORS-enabled so the page gets a real origin, a secure context and working storage. Only a view showing the program's own page has a message handler, and a message is answered only while that page is on the program's own origin. A navigation away from the bound origin and every new window go to the system browser, a window a script opens without a click included, since WebKit would otherwise drop it before the shell could route it, a download is saved only where the save dialog says, and a certificate the view cannot verify is allowed for its host only when its fingerprint equals the pin, after which the page is loaded again. Where the window was left is written while the window is closing, a page that has not finished loading within the start timeout closes the window and is reported, and a probe runs its scripts after the first load finishes and then closes the window.
 */

#include "ShellWindow.h"

#if defined(AMBROSE_HAS_WEBKITGTK)

#include "ConfigMgr.h"
#include "EmbeddedPage.h"
#include "LauncherPlace.h"
#include "ShellRules.h"
#include "TlsCertificate.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>

namespace
{
    constexpr char const* HandlerName = "ambrose";
    constexpr char const* HandlerSignal = "script-message-received::ambrose";

    struct Running
    {
        ShellWindowOptions const* Options = nullptr;
        GtkWidget* Window = nullptr;
        WebKitWebView* View = nullptr;
        WebKitWebContext* Context = nullptr;
        std::optional<ShellOrigin> Bound;
        std::unique_ptr<ShellGate> Gate;
        bool ProbeStarted = false;
        bool ProbeDone = false;
        std::size_t ProbeNext = 0;
        std::vector<std::string> ProbeResults;
        guint Deadline = 0;
        guint StartGuard = 0;
        bool Loaded = false;
        std::string Failure;

        void Log(std::string const& line) const
        {
            if (Options->Log)
                Options->Log(line);
            else
                std::cerr << Options->Program << ": " << line << '\n';
        }

        void OpenExternal(std::string const& url) const
        {
            if (Options->OpenExternal)
                Options->OpenExternal(url);
            else
                ShellWindow::OpenInSystemBrowser(url);
        }
    };

    void WritePlace(GtkWindow* window, std::filesystem::path const& file)
    {
        if (file.empty())
            return;
        WindowPlace place;
        gtk_window_get_position(window, &place.X, &place.Y);
        gtk_window_get_size(window, &place.Width, &place.Height);
        place.Maximised = gtk_window_is_maximized(window) == TRUE;
        if (place.Width < LauncherPlace::MinimumWidth || place.Height < LauncherPlace::MinimumHeight)
            return;

        std::error_code made;
        std::filesystem::create_directories(file.parent_path(), made);
        std::ofstream writing(file, std::ios::binary | std::ios::trunc);
        if (writing)
            writing << LauncherPlace::Describe(place);
    }

    std::optional<WindowPlace> ReadPlace(std::filesystem::path const& file)
    {
        if (file.empty())
            return std::nullopt;
        std::ifstream held(file, std::ios::binary);
        if (!held)
            return std::nullopt;
        std::ostringstream text;
        text << held.rdbuf();
        std::optional<WindowPlace> const place = LauncherPlace::Read(text.str());
        if (!place)
            return std::nullopt;
        GdkDisplay* const display = gdk_display_get_default();
        if (display == nullptr || gdk_display_get_monitor_at_point(display, place->X, place->Y) == nullptr)
            return std::nullopt;
        return place;
    }

    void Serve(WebKitURISchemeRequest* request, gpointer data)
    {
        auto const* const running = static_cast<Running const*>(data);
        char const* const asked = webkit_uri_scheme_request_get_path(request);
        EmbeddedAnswer const answer = running->Options->Page->Serve(asked == nullptr ? std::string_view("/") : std::string_view(asked));

        GInputStream* const body = answer.File == nullptr
            ? g_memory_input_stream_new()
            : g_memory_input_stream_new_from_data(answer.File->Bytes.data(), static_cast<gssize>(answer.File->Bytes.size()), nullptr);
        WebKitURISchemeResponse* const response = webkit_uri_scheme_response_new(body, answer.File == nullptr ? 0 : static_cast<gint64>(answer.File->Bytes.size()));
        webkit_uri_scheme_response_set_status(response, static_cast<guint>(answer.Status), answer.Status == 200 ? "OK" : "Not Found");
        SoupMessageHeaders* const headers = soup_message_headers_new(SOUP_MESSAGE_HEADERS_RESPONSE);
        for (auto const& [name, value] : answer.Headers)
        {
            if (name == "Content-Type")
                webkit_uri_scheme_response_set_content_type(response, value.c_str());
            else
                soup_message_headers_append(headers, name.c_str(), value.c_str());
        }
        webkit_uri_scheme_response_set_http_headers(response, headers);
        webkit_uri_scheme_request_finish_with_response(request, response);
        g_object_unref(response);
        g_object_unref(body);
    }

    void Heard(WebKitUserContentManager*, WebKitJavascriptResult* result, gpointer data)
    {
        auto* const running = static_cast<Running*>(data);
        char const* const source = webkit_web_view_get_uri(running->View);
        if (!running->Gate->Admit(source == nullptr ? std::string_view() : std::string_view(source)) || !running->Options->Answer)
            return;
        JSCValue* const value = webkit_javascript_result_get_js_value(result);
        char* const message = jsc_value_to_json(value, 0);
        if (message == nullptr)
            return;
        std::string const reply = running->Options->Answer(message);
        g_free(message);
        if (reply.empty())
            return;
        std::string const script = "window.ambroseHostReply(JSON.parse(" + nlohmann::json(reply).dump() + "))";
        webkit_web_view_evaluate_javascript(running->View, script.c_str(), -1, nullptr, nullptr, nullptr, nullptr, nullptr);
    }

    void Route(Running& running, std::string const& url, bool newWindow)
    {
        ShellNavigation const decision = ShellRules::Navigate(*running.Bound, url, newWindow);
        if (decision == ShellNavigation::SystemBrowser)
            running.OpenExternal(url);
        else if (decision == ShellNavigation::Refuse)
            running.Log("a window bound to " + running.Bound->Describe() + " refused to open " + url.substr(0, 200));
    }

    gboolean Decide(WebKitWebView*, WebKitPolicyDecision* decision, WebKitPolicyDecisionType type, gpointer data)
    {
        auto* const running = static_cast<Running*>(data);
        if (type == WEBKIT_POLICY_DECISION_TYPE_RESPONSE)
        {
            auto* const answer = WEBKIT_RESPONSE_POLICY_DECISION(decision);
            if (!webkit_response_policy_decision_is_mime_type_supported(answer))
            {
                webkit_policy_decision_download(decision);
                return TRUE;
            }
            return FALSE;
        }
        auto* const navigation = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
        WebKitNavigationAction* const action = webkit_navigation_policy_decision_get_navigation_action(navigation);
        char const* const uri = webkit_uri_request_get_uri(webkit_navigation_action_get_request(action));
        std::string const url = uri == nullptr ? std::string() : std::string(uri);
        bool const newWindow = type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION;
        if (!newWindow && ShellRules::Navigate(*running->Bound, url, false) == ShellNavigation::Stay)
            return FALSE;
        webkit_policy_decision_ignore(decision);
        Route(*running, url, newWindow);
        return TRUE;
    }

    GtkWidget* Create(WebKitWebView*, WebKitNavigationAction* action, gpointer data)
    {
        auto* const running = static_cast<Running*>(data);
        char const* const uri = webkit_uri_request_get_uri(webkit_navigation_action_get_request(action));
        Route(*running, uri == nullptr ? std::string() : std::string(uri), true);
        return nullptr;
    }

    gboolean ChooseDestination(WebKitDownload* download, gchar* suggested, gpointer data)
    {
        auto* const running = static_cast<Running*>(data);
        std::filesystem::path chosen;
        if (running->Options->SaveAs)
            chosen = running->Options->SaveAs(std::filesystem::path(suggested == nullptr ? "" : suggested));
        else
        {
            GtkWidget* const dialog = gtk_file_chooser_dialog_new("Save", GTK_WINDOW(running->Window), GTK_FILE_CHOOSER_ACTION_SAVE, "_Cancel",
                GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, nullptr);
            gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
            if (suggested != nullptr)
                gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), suggested);
            if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
            {
                char* const file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
                if (file != nullptr)
                {
                    chosen = file;
                    g_free(file);
                }
            }
            gtk_widget_destroy(dialog);
        }
        if (chosen.empty())
        {
            webkit_download_cancel(download);
            return TRUE;
        }
        gchar* const uri = g_filename_to_uri(chosen.c_str(), nullptr, nullptr);
        if (uri == nullptr)
        {
            webkit_download_cancel(download);
            return TRUE;
        }
        webkit_download_set_destination(download, uri);
        g_free(uri);
        return TRUE;
    }

    void DownloadStarted(WebKitWebContext*, WebKitDownload* download, gpointer data)
    {
        g_signal_connect(download, "decide-destination", G_CALLBACK(ChooseDestination), data);
    }

    gboolean TlsFailed(WebKitWebView* view, gchar* failing, GTlsCertificate* certificate, GTlsCertificateFlags, gpointer data)
    {
        auto* const running = static_cast<Running*>(data);
        std::string const url = failing == nullptr ? std::string() : std::string(failing);
        std::optional<ShellOrigin> const origin = ShellOrigin::Of(url);
        ShellOrigin const named = origin.value_or(*running->Bound);
        gchar* pem = nullptr;
        g_object_get(certificate, "certificate-pem", &pem, nullptr);
        std::string const fingerprint = pem == nullptr ? std::string() : TlsCertificate::Fingerprint(pem);
        g_free(pem);
        std::string const pin = running->Options->PinFor ? running->Options->PinFor(named) : std::string();
        ShellPinDecision const decision = ShellRules::Certificate(named, fingerprint, pin);
        if (!decision.Accept)
        {
            running->Log(decision.Reason);
            return FALSE;
        }
        std::string host = named.Host;
        if (host.starts_with('[') && host.ends_with(']'))
            host = host.substr(1, host.size() - 2);
        webkit_web_context_allow_tls_certificate_for_host(running->Context, certificate, host.c_str());
        webkit_web_view_load_uri(view, url.c_str());
        return TRUE;
    }

    void FinishProbe(Running& running)
    {
        if (running.ProbeDone)
            return;
        running.ProbeDone = true;
        if (running.Deadline != 0)
            g_source_remove(running.Deadline);
        running.Deadline = 0;
        if (running.Options->Probe->Done)
            running.Options->Probe->Done(running.ProbeResults);
        gtk_widget_destroy(running.Window);
    }

    gboolean ProbeStep(gpointer data);

    void Evaluated(GObject* source, GAsyncResult* result, gpointer data)
    {
        auto* const running = static_cast<Running*>(data);
        GError* failed = nullptr;
        JSCValue* const value = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(source), result, &failed);
        if (value == nullptr)
        {
            running->ProbeResults.push_back("null");
            if (failed != nullptr)
                g_error_free(failed);
        }
        else
        {
            char* const json = jsc_value_to_json(value, 0);
            running->ProbeResults.push_back(json == nullptr ? std::string("null") : std::string(json));
            g_free(json);
            g_object_unref(value);
        }
        g_timeout_add(static_cast<guint>(running->Options->Probe->Gap.count()), ProbeStep, running);
    }

    gboolean ProbeStep(gpointer data)
    {
        auto* const running = static_cast<Running*>(data);
        if (running->ProbeDone)
            return G_SOURCE_REMOVE;
        ShellProbe const& probe = *running->Options->Probe;
        if (running->ProbeNext >= probe.Scripts.size())
        {
            FinishProbe(*running);
            return G_SOURCE_REMOVE;
        }
        std::string const& script = probe.Scripts[running->ProbeNext++];
        webkit_web_view_evaluate_javascript(running->View, script.c_str(), -1, nullptr, nullptr, nullptr, Evaluated, running);
        return G_SOURCE_REMOVE;
    }

    gboolean ProbeOutOfTime(gpointer data)
    {
        auto* const running = static_cast<Running*>(data);
        running->Deadline = 0;
        running->Log("the probe ran out of time");
        FinishProbe(*running);
        return G_SOURCE_REMOVE;
    }

    gboolean StartOutOfTime(gpointer data)
    {
        auto* const running = static_cast<Running*>(data);
        running->StartGuard = 0;
        if (running->Loaded)
            return G_SOURCE_REMOVE;
        running->Failure = fmt::format("the web view did not show its page within {} seconds", running->Options->StartTimeout.count() / 1000);
        gtk_widget_destroy(running->Window);
        return G_SOURCE_REMOVE;
    }

    void LoadChanged(WebKitWebView*, WebKitLoadEvent event, gpointer data)
    {
        auto* const running = static_cast<Running*>(data);
        if (event == WEBKIT_LOAD_FINISHED && !running->Loaded)
        {
            running->Loaded = true;
            if (running->StartGuard != 0)
                g_source_remove(running->StartGuard);
            running->StartGuard = 0;
        }
        if (event != WEBKIT_LOAD_FINISHED || !running->Options->Probe || running->ProbeStarted)
            return;
        running->ProbeStarted = true;
        g_timeout_add(static_cast<guint>(running->Options->Probe->Gap.count()), ProbeStep, running);
    }

    gboolean Leaving(GtkWidget* window, GdkEvent*, gpointer data)
    {
        auto const* const running = static_cast<Running const*>(data);
        if (!running->Options->OffScreen)
            WritePlace(GTK_WINDOW(window), running->Options->PlaceFile);
        return FALSE;
    }

    void Ended(GtkWidget*, gpointer)
    {
        gtk_main_quit();
    }
}

bool ShellWindow::Available()
{
    return gtk_init_check(nullptr, nullptr) == TRUE;
}

void ShellWindow::OpenInSystemBrowser(std::string const& url)
{
    g_app_info_launch_default_for_uri(url.c_str(), nullptr, nullptr);
}

bool ShellWindow::Show(ShellWindowOptions const& options, std::string& error)
{
    Running running;
    running.Options = &options;
    running.Bound = BoundOrigin(options, error);
    if (!running.Bound)
        return false;
    if (!Available())
    {
        error = "this machine has no desktop to open a window on, so there is no window to open";
        return false;
    }
    if (options.DataFolder.empty())
    {
        error = "no data folder was named for the window's web view";
        return false;
    }
    bool const own = options.Page != nullptr;
    running.Gate = std::make_unique<ShellGate>(*running.Bound, !own, [&running](std::string const& line) { running.Log(line); });

    std::filesystem::path const profile = ProfileFolder(options);
    std::error_code made;
    std::filesystem::create_directories(profile / "data", made);
    std::filesystem::create_directories(profile / "cache", made);
    if (made)
    {
        error = "the web view's profile folder " + ConfigMgr::PathToUtf8(profile) + " could not be made: " + made.message();
        return false;
    }

    WebKitWebsiteDataManager* const manager = webkit_website_data_manager_new("base-data-directory", (profile / "data").c_str(),
        "base-cache-directory", (profile / "cache").c_str(), nullptr);
    running.Context = webkit_web_context_new_with_website_data_manager(manager);
    g_object_unref(manager);
    g_signal_connect(running.Context, "download-started", G_CALLBACK(DownloadStarted), &running);
    if (own)
    {
        webkit_web_context_register_uri_scheme(running.Context, ShellOrigin::OwnScheme, Serve, &running, nullptr);
        WebKitSecurityManager* const security = webkit_web_context_get_security_manager(running.Context);
        webkit_security_manager_register_uri_scheme_as_secure(security, ShellOrigin::OwnScheme);
        webkit_security_manager_register_uri_scheme_as_cors_enabled(security, ShellOrigin::OwnScheme);
    }

    WebKitUserContentManager* const content = webkit_user_content_manager_new();
    auto* const view = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW, "web-context", running.Context, "user-content-manager", content, nullptr));
    running.View = view;
    webkit_settings_set_javascript_can_open_windows_automatically(webkit_web_view_get_settings(view), TRUE);
    if (own)
    {
        webkit_user_content_manager_register_script_message_handler(content, HandlerName);
        g_signal_connect(content, HandlerSignal, G_CALLBACK(Heard), &running);
    }
    g_signal_connect(view, "decide-policy", G_CALLBACK(Decide), &running);
    g_signal_connect(view, "create", G_CALLBACK(Create), &running);
    g_signal_connect(view, "load-failed-with-tls-errors", G_CALLBACK(TlsFailed), &running);
    g_signal_connect(view, "load-changed", G_CALLBACK(LoadChanged), &running);

    GtkWidget* const window = options.OffScreen ? gtk_offscreen_window_new() : gtk_window_new(GTK_WINDOW_TOPLEVEL);
    running.Window = window;
    gtk_window_set_title(GTK_WINDOW(window), options.Title.c_str());
    std::optional<WindowPlace> const remembered = options.OffScreen ? std::nullopt : ReadPlace(options.PlaceFile);
    gtk_window_set_default_size(GTK_WINDOW(window), remembered ? remembered->Width : options.Width, remembered ? remembered->Height : options.Height);
    if (remembered)
    {
        gtk_window_move(GTK_WINDOW(window), remembered->X, remembered->Y);
        if (remembered->Maximised)
            gtk_window_maximize(GTK_WINDOW(window));
    }
    if (!options.OffScreen)
    {
        GdkGeometry smallest{};
        smallest.min_width = LauncherPlace::MinimumWidth;
        smallest.min_height = LauncherPlace::MinimumHeight;
        gtk_window_set_geometry_hints(GTK_WINDOW(window), nullptr, &smallest, GDK_HINT_MIN_SIZE);
    }
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(view));
    g_signal_connect(window, "delete-event", G_CALLBACK(Leaving), &running);
    g_signal_connect(window, "destroy", G_CALLBACK(Ended), nullptr);

    if (options.Probe)
        running.Deadline = g_timeout_add(static_cast<guint>(options.Probe->Timeout.count()), ProbeOutOfTime, &running);
    if (options.StartTimeout.count() > 0)
        running.StartGuard = g_timeout_add(static_cast<guint>(options.StartTimeout.count()), StartOutOfTime, &running);
    webkit_web_view_load_uri(view, StartUrl(options).c_str());

    gtk_widget_show_all(window);
    gtk_main();

    if (options.Probe && !running.ProbeDone && options.Probe->Done)
        options.Probe->Done(running.ProbeResults);
    if (running.StartGuard != 0)
        g_source_remove(running.StartGuard);
    g_object_unref(content);
    g_object_unref(running.Context);
    if (!running.Failure.empty())
    {
        error = running.Failure;
        return false;
    }
    return true;
}

#endif
