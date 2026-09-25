/*
 * Project Ambrose by Imjustchico
 * Opens a plain window, puts a WebView2 in it and maps the folder holding the built page onto a name the view can load, so the page is read from disk and no listener is opened. A message from the page is read as the object the shared host sends, handed to the launcher, and the answer posted back as an object on the same thread the view runs on, because WebView2 is a single-threaded apartment and answering from anywhere else is how a window stops drawing. Where the window was left is read before it is opened and written when it closes, through the numbers LauncherPlace judges, and a place no screen holds any more is dropped rather than put back. Nothing here is compiled anywhere but Windows; LauncherWindowGtk.cpp is the same window on a desktop whose build found webkit2gtk, a build that found neither says so plainly rather than opening nothing, and Available is what the caller asks.
 */

#include "LauncherWindow.h"

#ifdef _WIN32

#include "ConfigMgr.h"
#include "LauncherPlace.h"

#include <windows.h>
#include <wrl.h>
#include <WebView2.h>

#include <atomic>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>

namespace
{
    using Microsoft::WRL::Callback;
    using Microsoft::WRL::ComPtr;

    constexpr wchar_t const* WindowClass = L"AmbroseLauncherWindow";

    std::wstring Widen(std::string const& text)
    {
        if (text.empty())
            return std::wstring();
        int const size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        std::wstring wide(static_cast<std::size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
        return wide;
    }

    std::string Narrow(std::wstring const& text)
    {
        if (text.empty())
            return std::string();
        int const size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string narrow(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrow.data(), size, nullptr, nullptr);
        return narrow;
    }

    void WritePlace(HWND window, std::filesystem::path const& file);

    LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM wide, LPARAM low)
    {
        if (message == WM_CLOSE)
        {
            if (auto const* const file = reinterpret_cast<std::filesystem::path const*>(GetWindowLongPtrW(window, GWLP_USERDATA)))
                WritePlace(window, *file);
        }
        if (message == WM_GETMINMAXINFO)
        {
            auto* const limits = reinterpret_cast<MINMAXINFO*>(low);
            limits->ptMinTrackSize.x = LauncherPlace::MinimumWidth;
            limits->ptMinTrackSize.y = LauncherPlace::MinimumHeight;
            return 0;
        }
        if (message == WM_DESTROY)
        {
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(window, message, wide, low);
    }

    std::optional<WindowPlace> ReadPlace(std::filesystem::path const& file)
    {
        std::ifstream held(file, std::ios::binary);
        if (!held)
            return std::nullopt;
        std::ostringstream text;
        text << held.rdbuf();
        std::optional<WindowPlace> place = LauncherPlace::Read(text.str());
        if (!place)
            return std::nullopt;
        RECT const corners{ place->X, place->Y, place->X + place->Width, place->Y + place->Height };
        if (MonitorFromRect(&corners, MONITOR_DEFAULTTONULL) == nullptr)
            return std::nullopt;
        return place;
    }

    void WritePlace(HWND window, std::filesystem::path const& file)
    {
        WINDOWPLACEMENT placement{};
        placement.length = sizeof(placement);
        if (!GetWindowPlacement(window, &placement))
            return;

        WindowPlace place;
        place.X = placement.rcNormalPosition.left;
        place.Y = placement.rcNormalPosition.top;
        place.Width = placement.rcNormalPosition.right - placement.rcNormalPosition.left;
        place.Height = placement.rcNormalPosition.bottom - placement.rcNormalPosition.top;
        place.Maximised = placement.showCmd == SW_SHOWMAXIMIZED;
        if (place.Width < LauncherPlace::MinimumWidth || place.Height < LauncherPlace::MinimumHeight)
            return;

        std::error_code made;
        std::filesystem::create_directories(file.parent_path(), made);
        std::ofstream writing(file, std::ios::binary | std::ios::trunc);
        if (writing)
            writing << LauncherPlace::Describe(place);
    }
}

bool LauncherWindow::Available()
{
    LPWSTR version = nullptr;
    HRESULT const found = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version);
    bool const has = SUCCEEDED(found) && version != nullptr;
    if (version)
        CoTaskMemFree(version);
    return has;
}

bool LauncherWindow::Show(std::filesystem::path const& page, std::filesystem::path const& placeFile, Answering answer, std::string& error)
{
    if (!Available())
    {
        error = "this machine has no WebView2 runtime, so there is no window to open";
        return false;
    }
    if (!std::filesystem::exists(page))
    {
        error = "the launcher's own page was not found beside the program at " + ConfigMgr::PathToUtf8(page);
        return false;
    }

    HRESULT const started = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool const ownsCom = SUCCEEDED(started);

    WNDCLASSEXW description{};
    description.cbSize = sizeof(description);
    description.lpfnWndProc = Procedure;
    description.hInstance = GetModuleHandleW(nullptr);
    description.lpszClassName = WindowClass;
    description.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    RegisterClassExW(&description);

    std::optional<WindowPlace> const remembered = ReadPlace(placeFile);
    HWND const window = CreateWindowExW(0, WindowClass, L"Ambrose", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        DefaultWidth, DefaultHeight, nullptr, nullptr, description.hInstance, nullptr);
    if (!window)
    {
        error = "the launcher could not open a window";
        if (ownsCom)
            CoUninitialize();
        return false;
    }

    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&placeFile));
    if (remembered)
    {
        WINDOWPLACEMENT placement{};
        placement.length = sizeof(placement);
        placement.showCmd = static_cast<UINT>(remembered->Maximised ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
        placement.rcNormalPosition = RECT{ remembered->X, remembered->Y, remembered->X + remembered->Width,
            remembered->Y + remembered->Height };
        SetWindowPlacement(window, &placement);
    }

    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> view;
    std::atomic<bool> settled{ false };
    std::string failure;
    std::wstring const folder = page.parent_path().wstring();
    std::wstring const start = std::wstring(L"http://") + Widen(VirtualHost) + L"/" + page.filename().wstring();

    HRESULT const asked = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [&](HRESULT made, ICoreWebView2Environment* environment) -> HRESULT
            {
                if (FAILED(made) || environment == nullptr)
                {
                    failure = "the web view could not be started";
                    settled = true;
                    return made;
                }
                return environment->CreateCoreWebView2Controller(window,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [&](HRESULT ready, ICoreWebView2Controller* made2) -> HRESULT
                        {
                            if (FAILED(ready) || made2 == nullptr)
                            {
                                failure = "the web view could not be put in the window";
                                settled = true;
                                return ready;
                            }
                            controller = made2;
                            controller->get_CoreWebView2(&view);

                            ComPtr<ICoreWebView2_3> mapping;
                            if (SUCCEEDED(view.As(&mapping)))
                            {
                                mapping->SetVirtualHostNameToFolderMapping(Widen(VirtualHost).c_str(), folder.c_str(),
                                    COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
                            }

                            EventRegistrationToken token{};
                            view->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [&](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT
                                    {
                                        LPWSTR text = nullptr;
                                        if (FAILED(args->get_WebMessageAsJson(&text)) || text == nullptr)
                                            return S_OK;
                                        std::string const reply = answer(Narrow(text));
                                        CoTaskMemFree(text);
                                        if (!reply.empty() && view)
                                            view->PostWebMessageAsJson(Widen(reply).c_str());
                                        return S_OK;
                                    })
                                    .Get(),
                                &token);

                            RECT bounds{};
                            GetClientRect(window, &bounds);
                            controller->put_Bounds(bounds);
                            view->Navigate(start.c_str());
                            settled = true;
                            return S_OK;
                        })
                        .Get());
            })
            .Get());

    if (FAILED(asked))
    {
        error = "the web view could not be asked for";
        DestroyWindow(window);
        if (ownsCom)
            CoUninitialize();
        return false;
    }

    ShowWindow(window, remembered && remembered->Maximised ? SW_SHOWMAXIMIZED : SW_SHOW);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
        if (settled && !failure.empty())
            break;
        if (!IsWindow(window))
            break;
        if (controller)
        {
            RECT bounds{};
            GetClientRect(window, &bounds);
            controller->put_Bounds(bounds);
        }
    }

    if (ownsCom)
        CoUninitialize();
    if (!failure.empty())
    {
        error = failure;
        return false;
    }
    return true;
}

#elif !defined(AMBROSE_HAS_WEBKITGTK)

bool LauncherWindow::Available()
{
    return false;
}

bool LauncherWindow::Show(std::filesystem::path const&, std::filesystem::path const&, Answering, std::string& error)
{
    error = "this build has no window of its own, because no web view was found when it was built, so the console launcher is what runs here";
    return false;
}

#endif
