/*
 * Project Ambrose by Imjustchico
 * Writes and reads the one JSON line a panel's window process is handed, and sends a panel to its window or, with no web view, to the default browser, saying why only the first time.
 */

#include "PanelOpener.h"

#include <nlohmann/json.hpp>

std::string PanelWindowOrder::Describe() const
{
    return nlohmann::json{ { "name", Name }, { "start", Start }, { "pin", Pin } }.dump();
}

bool PanelWindowOrder::Read(std::string const& line, PanelWindowOrder& order, std::string& error)
{
    nlohmann::json const read = nlohmann::json::parse(line, nullptr, false);
    if (!read.is_object() || !read.contains("start") || !read["start"].is_string())
    {
        error = "a panel window is handed one line naming the panel and where it starts, and this is not one";
        return false;
    }
    order.Name = read.value("name", std::string("Ambrose panel"));
    order.Start = read["start"].get<std::string>();
    order.Pin = read.value("pin", std::string());
    return true;
}

PanelOpener::PanelOpener(bool webView, Launch launch, Browser browser, Say say)
    : _webView(webView), _launch(std::move(launch)), _browser(std::move(browser)), _say(std::move(say))
{
}

bool PanelOpener::Open(PanelEntry const& entry, std::string const& start, std::string& error)
{
    if (!_webView)
    {
        if (!_said && _say)
            _say(NoWebView);
        _said = true;
        _browser(start);
        return true;
    }
    PanelWindowOrder order;
    order.Name = entry.Name;
    order.Start = start;
    order.Pin = entry.Address.Trust == PanelTrust::Pinned ? entry.Address.Pin : std::string();
    return _launch(order, error);
}
