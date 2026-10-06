/*
 * Project Ambrose by Imjustchico
 * How a chosen panel is opened: in a window of the program's own, run as a process of its own that is handed the panel's name, origin, pin and first address on its input, so a one-time link never appears on a command line another process could read and a window that fails takes no other with it; or, on a machine with no web view, in the default browser at that same first address, with the program saying once why. What opens a window and what opens the browser are handed in, so a test sees each decision without either.
 */

#ifndef AMBROSE_PANELOPENER_H
#define AMBROSE_PANELOPENER_H

#include "PanelList.h"

#include <functional>
#include <string>

struct PanelWindowOrder
{
    std::string Name;
    std::string Start;
    std::string Pin;

    std::string Describe() const;
    static bool Read(std::string const& line, PanelWindowOrder& order, std::string& error);
};

class PanelOpener
{
public:
    using Launch = std::function<bool(PanelWindowOrder const& order, std::string& error)>;
    using Browser = std::function<void(std::string const& url)>;
    using Say = std::function<void(std::string const& line)>;

    static constexpr char const* NoWebView = "this computer has no web view, so each panel opens in the default browser instead of a window of the program's own";

    PanelOpener(bool webView, Launch launch, Browser browser, Say say);

    bool Open(PanelEntry const& entry, std::string const& start, std::string& error);

private:
    bool _webView;
    Launch _launch;
    Browser _browser;
    Say _say;
    bool _said = false;
};

#endif
