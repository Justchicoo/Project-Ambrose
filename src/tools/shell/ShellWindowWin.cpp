/*
 * Project Ambrose by Imjustchico
 * The shell window on Windows: a plain window holding a WebView2 whose user data folder is the window's profile, so nothing is written beside the executable. The program's own page is answered from memory for every request to its https origin through a web resource handler, and a message from the page is admitted only from that origin, handed to the program and answered on the view's own thread, because WebView2 is a single-threaded apartment. A view bound to a remote panel has web messages turned off and no handler. A navigation away from the bound origin and every new window go to the system browser, a document request to any other origin is answered empty in the view so nothing reaches that origin from it, and the program's own page, which carries a content policy keeping it on its own origin, has every request elsewhere refused in the view and named once, a download is saved only where the save dialog says, and a certificate the view cannot verify is allowed only when its fingerprint equals the pin. A page that has not finished its first navigation within the start timeout closes the window and is reported, so the program can fall back. Where the window was left is read before it opens and written when it closes, the message loop runs only while the window exists and posts no quit, so the next window this thread opens does not end at once, and a probe runs its scripts after the first page settles and then closes the window.
 */

#include "ShellWindow.h"

#ifdef _WIN32

#include "ConfigMgr.h"
#include "EmbeddedPage.h"
#include "LauncherPlace.h"
#include "ShellRules.h"
#include "TlsCertificate.h"

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <wrl.h>
#include <WebView2.h>

#include <fmt/format.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <system_error>

namespace
{
    using Microsoft::WRL::Callback;
    using Microsoft::WRL::ComPtr;

    constexpr wchar_t const* WindowClass = L"AmbroseShellWindow";
    constexpr UINT_PTR ProbeTimer = 1;
    constexpr UINT_PTR ProbeDeadline = 2;
    constexpr UINT_PTR StartDeadline = 3;

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

    std::string Take(LPWSTR text)
    {
        if (text == nullptr)
            return std::string();
        std::string const narrow = Narrow(text);
        CoTaskMemFree(text);
        return narrow;
    }

    struct Running
    {
        ShellWindowOptions const* Options = nullptr;
        HWND Window = nullptr;
        ComPtr<ICoreWebView2Environment> Environment;
        ComPtr<ICoreWebView2Controller> Controller;
        ComPtr<ICoreWebView2> View;
        std::optional<ShellOrigin> Bound;
        std::unique_ptr<ShellGate> Gate;
        std::set<std::string> Refused;
        std::string Failure;
        bool Settled = false;
        bool ProbeStarted = false;
        std::size_t ProbeNext = 0;
        std::vector<std::string> ProbeResults;
        bool ProbeDone = false;
        bool Loaded = false;

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

    void WritePlace(HWND window, std::filesystem::path const& file)
    {
        if (file.empty())
            return;
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

    std::optional<WindowPlace> ReadPlace(std::filesystem::path const& file)
    {
        if (file.empty())
            return std::nullopt;
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

    void FinishProbe(Running& running)
    {
        if (running.ProbeDone)
            return;
        running.ProbeDone = true;
        KillTimer(running.Window, ProbeTimer);
        KillTimer(running.Window, ProbeDeadline);
        if (running.Options->Probe->Done)
            running.Options->Probe->Done(running.ProbeResults);
        PostMessageW(running.Window, WM_CLOSE, 0, 0);
    }

    void RunProbeStep(Running& running)
    {
        ShellProbe const& probe = *running.Options->Probe;
        if (running.ProbeNext >= probe.Scripts.size() || !running.View)
        {
            FinishProbe(running);
            return;
        }
        std::wstring const script = Widen(probe.Scripts[running.ProbeNext++]);
        Running* const held = &running;
        HRESULT const asked = running.View->ExecuteScript(script.c_str(),
            Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                [held](HRESULT ran, LPCWSTR result) -> HRESULT
                {
                    held->ProbeResults.push_back(SUCCEEDED(ran) && result != nullptr ? Narrow(result) : std::string("null"));
                    SetTimer(held->Window, ProbeTimer, static_cast<UINT>(held->Options->Probe->Gap.count()), nullptr);
                    return S_OK;
                })
                .Get());
        if (FAILED(asked))
        {
            running.ProbeResults.push_back("null");
            SetTimer(running.Window, ProbeTimer, static_cast<UINT>(probe.Gap.count()), nullptr);
        }
    }

    LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM wide, LPARAM low)
    {
        auto* const running = reinterpret_cast<Running*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_CLOSE && running != nullptr)
        {
            if (!running->Options->OffScreen)
                WritePlace(window, running->Options->PlaceFile);
            if (running->Controller)
            {
                running->Controller->Close();
                running->Controller.Reset();
                running->View.Reset();
            }
        }
        if (message == WM_TIMER && running != nullptr)
        {
            if (wide == ProbeTimer)
            {
                KillTimer(window, ProbeTimer);
                RunProbeStep(*running);
                return 0;
            }
            if (wide == StartDeadline)
            {
                KillTimer(window, StartDeadline);
                if (!running->Loaded)
                {
                    running->Failure = fmt::format("the web view did not show its page within {} seconds", running->Options->StartTimeout.count() / 1000);
                    running->Settled = true;
                    PostMessageW(window, WM_CLOSE, 0, 0);
                }
                return 0;
            }
            if (wide == ProbeDeadline)
            {
                running->Log("the probe ran out of time");
                FinishProbe(*running);
                return 0;
            }
        }
        if (message == WM_SIZE && running != nullptr && running->Controller)
        {
            RECT bounds{};
            GetClientRect(window, &bounds);
            running->Controller->put_Bounds(bounds);
        }
        if (message == WM_GETMINMAXINFO && (running == nullptr || !running->Options->OffScreen))
        {
            auto* const limits = reinterpret_cast<MINMAXINFO*>(low);
            limits->ptMinTrackSize.x = LauncherPlace::MinimumWidth;
            limits->ptMinTrackSize.y = LauncherPlace::MinimumHeight;
            return 0;
        }
        return DefWindowProcW(window, message, wide, low);
    }

    std::filesystem::path AskWhereToSave(std::filesystem::path const& suggested)
    {
        std::wstring buffer = suggested.filename().wstring();
        buffer.resize(MAX_PATH * 4, L'\0');
        OPENFILENAMEW asking{};
        asking.lStructSize = sizeof(asking);
        asking.lpstrFile = buffer.data();
        asking.nMaxFile = static_cast<DWORD>(buffer.size());
        asking.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetSaveFileNameW(&asking))
            return std::filesystem::path();
        return std::filesystem::path(buffer.c_str());
    }

    void ServeOwnPage(Running& running, ICoreWebView2WebResourceRequestedEventArgs* args)
    {
        ComPtr<ICoreWebView2WebResourceRequest> request;
        if (FAILED(args->get_Request(&request)))
            return;
        LPWSTR uri = nullptr;
        request->get_Uri(&uri);
        std::string const url = Take(uri);
        std::string const origin = running.Bound->Describe();
        std::string const target = url.starts_with(origin) ? url.substr(origin.size()) : std::string("/");

        EmbeddedAnswer const answer = running.Options->Page->Serve(target);
        std::string headers;
        for (auto const& [name, value] : answer.Headers)
            headers += name + ": " + value + "\r\n";
        ComPtr<IStream> body;
        if (answer.File != nullptr)
            body.Attach(SHCreateMemStream(reinterpret_cast<BYTE const*>(answer.File->Bytes.data()), static_cast<UINT>(answer.File->Bytes.size())));
        ComPtr<ICoreWebView2WebResourceResponse> response;
        if (SUCCEEDED(running.Environment->CreateWebResourceResponse(body.Get(), answer.Status, answer.Status == 200 ? L"OK" : L"Not Found",
                Widen(headers).c_str(), &response)))
            args->put_Response(response.Get());
    }

    void Route(Running& running, std::string const& url, bool newWindow)
    {
        ShellNavigation const decision = ShellRules::Navigate(*running.Bound, url, newWindow);
        if (decision == ShellNavigation::SystemBrowser)
            running.OpenExternal(url);
        else if (decision == ShellNavigation::Refuse)
            running.Log("a window bound to " + running.Bound->Describe() + " refused to open " + url.substr(0, 200));
    }

    HRESULT Attach(Running& running, ICoreWebView2Controller* made)
    {
        ShellWindowOptions const& options = *running.Options;
        running.Controller = made;
        running.Controller->get_CoreWebView2(&running.View);
        ICoreWebView2* const view = running.View.Get();
        bool const own = options.Page != nullptr;

        ComPtr<ICoreWebView2Settings> settings;
        if (SUCCEEDED(view->get_Settings(&settings)))
        {
            settings->put_IsWebMessageEnabled(own ? TRUE : FALSE);
            settings->put_AreHostObjectsAllowed(FALSE);
            settings->put_IsStatusBarEnabled(FALSE);
        }

        EventRegistrationToken token{};
        view->AddWebResourceRequestedFilter(L"*", own ? COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL : COREWEBVIEW2_WEB_RESOURCE_CONTEXT_DOCUMENT);
        if (own)
        {
            std::wstring const filter = Widen(running.Bound->Describe()) + L"/*";
            view->AddWebResourceRequestedFilter(filter.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
        }
        view->add_WebResourceRequested(Callback<ICoreWebView2WebResourceRequestedEventHandler>(
                                           [&running](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT
                                           {
                                               ComPtr<ICoreWebView2WebResourceRequest> request;
                                               LPWSTR uri = nullptr;
                                               if (FAILED(args->get_Request(&request)) || FAILED(request->get_Uri(&uri)))
                                                   return S_OK;
                                               std::optional<ShellOrigin> const origin = ShellOrigin::Of(Take(uri));
                                               if (!origin)
                                                   return S_OK;
                                               if (*origin == *running.Bound)
                                               {
                                                   if (running.Options->Page != nullptr)
                                                       ServeOwnPage(running, args);
                                                   return S_OK;
                                               }
                                               bool const own = running.Options->Page != nullptr;
                                               if (own)
                                               {
                                                   std::string const named = origin->Describe();
                                                   if (running.Refused.insert(named).second)
                                                       running.Log("the program's own page asked for " + named + ", which it never reaches, so nothing was sent");
                                               }
                                               ComPtr<ICoreWebView2WebResourceResponse> nothing;
                                               if (SUCCEEDED(running.Environment->CreateWebResourceResponse(nullptr, own ? 403 : 204, own ? L"Forbidden" : L"No Content", L"", &nothing)))
                                                   args->put_Response(nothing.Get());
                                               return S_OK;
                                           })
                                           .Get(),
            &token);
        if (own)
        {
            view->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                             [&running](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT
                                             {
                                                 LPWSTR source = nullptr;
                                                 args->get_Source(&source);
                                                 if (!running.Gate->Admit(Take(source)))
                                                     return S_OK;
                                                 LPWSTR text = nullptr;
                                                 if (FAILED(args->get_WebMessageAsJson(&text)) || text == nullptr || !running.Options->Answer)
                                                     return S_OK;
                                                 std::string const reply = running.Options->Answer(Take(text));
                                                 if (!reply.empty())
                                                     sender->PostWebMessageAsJson(Widen(reply).c_str());
                                                 return S_OK;
                                             })
                                             .Get(),
                &token);
        }

        view->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
                                         [&running](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT
                                         {
                                             LPWSTR uri = nullptr;
                                             args->get_Uri(&uri);
                                             std::string const url = Take(uri);
                                             if (ShellRules::Navigate(*running.Bound, url, false) == ShellNavigation::Stay)
                                                 return S_OK;
                                             args->put_Cancel(TRUE);
                                             Route(running, url, false);
                                             return S_OK;
                                         })
                                         .Get(),
            &token);

        view->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                                         [&running](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT
                                         {
                                             LPWSTR uri = nullptr;
                                             args->get_Uri(&uri);
                                             args->put_Handled(TRUE);
                                             Route(running, Take(uri), true);
                                             return S_OK;
                                         })
                                         .Get(),
            &token);

        view->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                          [&running](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT
                                          {
                                              if (!running.Loaded)
                                              {
                                                  running.Loaded = true;
                                                  KillTimer(running.Window, StartDeadline);
                                              }
                                              if (running.Options->Probe && !running.ProbeStarted)
                                              {
                                                  running.ProbeStarted = true;
                                                  SetTimer(running.Window, ProbeTimer, static_cast<UINT>(running.Options->Probe->Gap.count()), nullptr);
                                              }
                                              return S_OK;
                                          })
                                          .Get(),
            &token);

        ComPtr<ICoreWebView2_4> downloads;
        if (SUCCEEDED(running.View.As(&downloads)))
        {
            downloads->add_DownloadStarting(Callback<ICoreWebView2DownloadStartingEventHandler>(
                                                [&running](ICoreWebView2*, ICoreWebView2DownloadStartingEventArgs* args) -> HRESULT
                                                {
                                                    LPWSTR suggested = nullptr;
                                                    args->get_ResultFilePath(&suggested);
                                                    std::filesystem::path const proposed(Widen(Take(suggested)));
                                                    std::filesystem::path const chosen = running.Options->SaveAs ? running.Options->SaveAs(proposed) : AskWhereToSave(proposed);
                                                    args->put_Handled(TRUE);
                                                    if (chosen.empty())
                                                        args->put_Cancel(TRUE);
                                                    else
                                                        args->put_ResultFilePath(chosen.wstring().c_str());
                                                    return S_OK;
                                                })
                                                .Get(),
                &token);
        }

        ComPtr<ICoreWebView2_14> certificates;
        if (SUCCEEDED(running.View.As(&certificates)))
        {
            certificates->add_ServerCertificateErrorDetected(
                Callback<ICoreWebView2ServerCertificateErrorDetectedEventHandler>(
                    [&running](ICoreWebView2*, ICoreWebView2ServerCertificateErrorDetectedEventArgs* args) -> HRESULT
                    {
                        LPWSTR uri = nullptr;
                        args->get_RequestUri(&uri);
                        std::optional<ShellOrigin> const origin = ShellOrigin::Of(Take(uri));
                        ComPtr<ICoreWebView2Certificate> certificate;
                        LPWSTR pem = nullptr;
                        std::string fingerprint;
                        if (SUCCEEDED(args->get_ServerCertificate(&certificate)) && certificate && SUCCEEDED(certificate->ToPemEncoding(&pem)))
                            fingerprint = TlsCertificate::Fingerprint(Take(pem));
                        ShellOrigin const named = origin.value_or(*running.Bound);
                        std::string const pin = running.Options->PinFor ? running.Options->PinFor(named) : std::string();
                        ShellPinDecision const decision = ShellRules::Certificate(named, fingerprint, pin);
                        args->put_Action(decision.Accept ? COREWEBVIEW2_SERVER_CERTIFICATE_ERROR_ACTION_ALWAYS_ALLOW
                                                         : COREWEBVIEW2_SERVER_CERTIFICATE_ERROR_ACTION_CANCEL);
                        if (!decision.Accept)
                            running.Log(decision.Reason);
                        return S_OK;
                    })
                    .Get(),
                &token);
        }

        RECT bounds{};
        GetClientRect(running.Window, &bounds);
        running.Controller->put_Bounds(bounds);
        running.Controller->put_IsVisible(TRUE);
        view->Navigate(Widen(ShellWindow::StartUrl(options)).c_str());
        running.Settled = true;
        return S_OK;
    }
}

bool ShellWindow::Available()
{
    LPWSTR version = nullptr;
    HRESULT const found = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version);
    bool const has = SUCCEEDED(found) && version != nullptr;
    if (version)
        CoTaskMemFree(version);
    return has;
}

void ShellWindow::OpenInSystemBrowser(std::string const& url)
{
    ShellExecuteW(nullptr, L"open", Widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
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
        error = "this machine has no WebView2 runtime, so there is no window to open";
        return false;
    }
    if (options.DataFolder.empty())
    {
        error = "no data folder was named for the window's web view";
        return false;
    }
    running.Gate = std::make_unique<ShellGate>(*running.Bound, options.Page == nullptr, [&running](std::string const& line) { running.Log(line); });

    std::filesystem::path const profile = ProfileFolder(options);
    std::error_code made;
    std::filesystem::create_directories(profile, made);
    if (made)
    {
        error = "the web view's profile folder " + ConfigMgr::PathToUtf8(profile) + " could not be made: " + made.message();
        return false;
    }

    static thread_local HRESULT const apartment = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    (void)apartment;

    WNDCLASSEXW description{};
    description.cbSize = sizeof(description);
    description.lpfnWndProc = Procedure;
    description.hInstance = GetModuleHandleW(nullptr);
    description.lpszClassName = WindowClass;
    description.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    RegisterClassExW(&description);

    std::optional<WindowPlace> const remembered = options.OffScreen ? std::nullopt : ReadPlace(options.PlaceFile);
    HWND const window = options.OffScreen
        ? CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, WindowClass, Widen(options.Title).c_str(), WS_POPUP, -32000, -32000,
              options.Width, options.Height, nullptr, nullptr, description.hInstance, nullptr)
        : CreateWindowExW(0, WindowClass, Widen(options.Title).c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, options.Width,
              options.Height, nullptr, nullptr, description.hInstance, nullptr);
    if (!window)
    {
        error = "no window could be opened";
        return false;
    }
    running.Window = window;
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&running));
    if (remembered)
    {
        WINDOWPLACEMENT placement{};
        placement.length = sizeof(placement);
        placement.showCmd = static_cast<UINT>(remembered->Maximised ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
        placement.rcNormalPosition = RECT{ remembered->X, remembered->Y, remembered->X + remembered->Width, remembered->Y + remembered->Height };
        SetWindowPlacement(window, &placement);
    }

    if (options.Probe)
        SetTimer(window, ProbeDeadline, static_cast<UINT>(options.Probe->Timeout.count()), nullptr);
    if (options.StartTimeout.count() > 0)
        SetTimer(window, StartDeadline, static_cast<UINT>(options.StartTimeout.count()), nullptr);
    ShowWindow(window, options.OffScreen ? SW_SHOWNOACTIVATE : (remembered && remembered->Maximised ? SW_SHOWMAXIMIZED : SW_SHOW));
    UpdateWindow(window);

    HRESULT const asked = CreateCoreWebView2EnvironmentWithOptions(nullptr, profile.wstring().c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [&running](HRESULT made2, ICoreWebView2Environment* environment) -> HRESULT
            {
                if (FAILED(made2) || environment == nullptr)
                {
                    running.Failure = fmt::format("the web view could not be started ({:#010x})", static_cast<unsigned long>(made2));
                    running.Settled = true;
                    return made2;
                }
                running.Environment = environment;
                return environment->CreateCoreWebView2Controller(running.Window,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [&running](HRESULT ready, ICoreWebView2Controller* controller) -> HRESULT
                        {
                            if (FAILED(ready) || controller == nullptr)
                            {
                                running.Failure = fmt::format("the web view could not be put in the window ({:#010x})", static_cast<unsigned long>(ready));
                                running.Settled = true;
                                return ready;
                            }
                            return Attach(running, controller);
                        })
                        .Get());
            })
            .Get());

    if (FAILED(asked))
    {
        error = "the web view could not be asked for";
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        DestroyWindow(window);
        return false;
    }


    MSG message{};
    while (IsWindow(window) && GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
        if (running.Settled && !running.Failure.empty())
            break;
        if (!IsWindow(window))
            break;
    }

    if (running.Controller)
        running.Controller->Close();
    running.View.Reset();
    running.Controller.Reset();
    running.Environment.Reset();
    if (IsWindow(window))
    {
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        DestroyWindow(window);
    }
    if (options.Probe && !running.ProbeDone && options.Probe->Done)
        options.Probe->Done(running.ProbeResults);
    if (!running.Failure.empty())
    {
        error = running.Failure;
        return false;
    }
    return true;
}

#endif
