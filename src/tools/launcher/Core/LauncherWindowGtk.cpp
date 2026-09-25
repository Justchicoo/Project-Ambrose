/*
 * Project Ambrose by Imjustchico
 * The same launcher window on a desktop that is not Windows: a GTK window holding a WebKitGTK view, built only where the build found webkit2gtk, and answering the same page with the same launcher behind it. The page is served to the view through a scheme of the launcher's own that reads the folder beside the program, so no listener is opened here either and a request that climbs out of that folder is refused rather than answered. A message from the page arrives as the object the shared host sends and the answer is handed back by calling the page's own reply function, because that is the contract the bridge sets for a WebKit host. Where the window was left is written as the window is being closed, while there is still a window to ask. A machine with no display says so and is left with the console launcher.
 */

#include "LauncherWindow.h"

#if defined(AMBROSE_HAS_WEBKITGTK)

#include "ConfigMgr.h"
#include "LauncherPlace.h"

#include <nlohmann/json.hpp>

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include <algorithm>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>

namespace
{
    constexpr char const* Scheme = "ambrose";
    constexpr char const* HandlerName = "ambrose";
    constexpr char const* HandlerSignal = "script-message-received::ambrose";

    struct Serving
    {
        std::filesystem::path Folder;
    };

    struct Replying
    {
        LauncherWindow::Answering Answer;
        WebKitWebView* View = nullptr;
    };

    char const* TypeOf(std::filesystem::path const& file)
    {
        std::string const suffix = file.extension().string();
        if (suffix == ".html")
            return "text/html";
        if (suffix == ".js" || suffix == ".mjs")
            return "text/javascript";
        if (suffix == ".css")
            return "text/css";
        if (suffix == ".json")
            return "application/json";
        if (suffix == ".woff2")
            return "font/woff2";
        if (suffix == ".svg")
            return "image/svg+xml";
        if (suffix == ".png")
            return "image/png";
        if (suffix == ".ico")
            return "image/vnd.microsoft.icon";
        return "application/octet-stream";
    }

    std::optional<std::filesystem::path> Under(std::filesystem::path const& folder, std::string const& asked)
    {
        std::filesystem::path wanted = folder;
        std::istringstream parts(asked);
        std::string part;
        while (std::getline(parts, part, '/'))
        {
            if (part.empty() || part == ".")
                continue;
            if (part == "..")
                return std::nullopt;
            wanted /= part;
        }
        if (wanted == folder)
            return std::nullopt;

        std::error_code failed;
        std::filesystem::path const real = std::filesystem::weakly_canonical(wanted, failed);
        if (failed)
            return std::nullopt;
        std::filesystem::path const root = std::filesystem::weakly_canonical(folder, failed);
        if (failed)
            return std::nullopt;
        auto const inside = std::mismatch(root.begin(), root.end(), real.begin(), real.end());
        if (inside.first != root.end())
            return std::nullopt;
        return real;
    }

    void Serve(WebKitURISchemeRequest* request, gpointer data)
    {
        auto const* const serving = static_cast<Serving const*>(data);
        char const* const asked = webkit_uri_scheme_request_get_path(request);
        std::optional<std::filesystem::path> const file = Under(serving->Folder, asked == nullptr ? std::string() : std::string(asked));
        if (!file)
        {
            GError* const refused = g_error_new_literal(G_FILE_ERROR, G_FILE_ERROR_ACCES, "that is not a file this window serves");
            webkit_uri_scheme_request_finish_error(request, refused);
            g_error_free(refused);
            return;
        }

        GFile* const held = g_file_new_for_path(file->c_str());
        GError* missing = nullptr;
        GFileInputStream* const reading = g_file_read(held, nullptr, &missing);
        if (reading == nullptr)
        {
            webkit_uri_scheme_request_finish_error(request, missing);
            g_error_free(missing);
            g_object_unref(held);
            return;
        }

        gint64 size = -1;
        GFileInfo* const about = g_file_query_info(held, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
        if (about != nullptr)
        {
            size = g_file_info_get_size(about);
            g_object_unref(about);
        }
        webkit_uri_scheme_request_finish(request, G_INPUT_STREAM(reading), size, TypeOf(*file));
        g_object_unref(reading);
        g_object_unref(held);
    }

    void Heard(WebKitUserContentManager*, WebKitJavascriptResult* result, gpointer data)
    {
        auto* const replying = static_cast<Replying*>(data);
        JSCValue* const value = webkit_javascript_result_get_js_value(result);
        char* const message = jsc_value_to_json(value, 0);
        if (message == nullptr)
            return;

        std::string const reply = replying->Answer(message);
        g_free(message);
        if (reply.empty() || replying->View == nullptr)
            return;

        std::string const script = "window.ambroseHostReply(JSON.parse(" + nlohmann::json(reply).dump() + "))";
        webkit_web_view_evaluate_javascript(replying->View, script.c_str(), -1, nullptr, nullptr, nullptr, nullptr, nullptr);
    }

    void WritePlace(GtkWindow* window, std::filesystem::path const& file)
    {
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

    gboolean Leaving(GtkWidget* window, GdkEvent*, gpointer data)
    {
        WritePlace(GTK_WINDOW(window), *static_cast<std::filesystem::path const*>(data));
        return FALSE;
    }

    void Ended(GtkWidget*, gpointer)
    {
        gtk_main_quit();
    }

    std::optional<WindowPlace> ReadPlace(std::filesystem::path const& file)
    {
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
}

bool LauncherWindow::Available()
{
    return gtk_init_check(nullptr, nullptr) == TRUE;
}

bool LauncherWindow::Show(std::filesystem::path const& page, std::filesystem::path const& placeFile, Answering answer, std::string& error)
{
    if (!Available())
    {
        error = "this machine has no desktop to open a window on, so there is no window to open";
        return false;
    }
    if (!std::filesystem::exists(page))
    {
        error = "the launcher's own page was not found beside the program at " + ConfigMgr::PathToUtf8(page);
        return false;
    }

    Serving serving{ page.parent_path() };
    WebKitWebContext* const context = webkit_web_context_new_ephemeral();
    webkit_web_context_register_uri_scheme(context, Scheme, Serve, &serving, nullptr);
    webkit_security_manager_register_uri_scheme_as_cors_enabled(webkit_web_context_get_security_manager(context), Scheme);

    WebKitUserContentManager* const content = webkit_user_content_manager_new();
    webkit_user_content_manager_register_script_message_handler(content, HandlerName);

    auto* const view = WEBKIT_WEB_VIEW(
        g_object_new(WEBKIT_TYPE_WEB_VIEW, "web-context", context, "user-content-manager", content, nullptr));
    Replying replying{ std::move(answer), view };
    g_signal_connect(content, HandlerSignal, G_CALLBACK(Heard), &replying);

    GtkWidget* const window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Ambrose");
    std::optional<WindowPlace> const remembered = ReadPlace(placeFile);
    gtk_window_set_default_size(GTK_WINDOW(window), remembered ? remembered->Width : DefaultWidth,
        remembered ? remembered->Height : DefaultHeight);
    if (remembered)
    {
        gtk_window_move(GTK_WINDOW(window), remembered->X, remembered->Y);
        if (remembered->Maximised)
            gtk_window_maximize(GTK_WINDOW(window));
    }

    GdkGeometry smallest{};
    smallest.min_width = LauncherPlace::MinimumWidth;
    smallest.min_height = LauncherPlace::MinimumHeight;
    gtk_window_set_geometry_hints(GTK_WINDOW(window), nullptr, &smallest, GDK_HINT_MIN_SIZE);
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(view));
    g_signal_connect(window, "delete-event", G_CALLBACK(Leaving), const_cast<std::filesystem::path*>(&placeFile));
    g_signal_connect(window, "destroy", G_CALLBACK(Ended), nullptr);

    std::string const start = std::string(Scheme) + "://" + VirtualHost + "/" + page.filename().string();
    webkit_web_view_load_uri(view, start.c_str());

    gtk_widget_show_all(window);
    gtk_main();

    g_object_unref(content);
    g_object_unref(context);
    return true;
}

#endif
