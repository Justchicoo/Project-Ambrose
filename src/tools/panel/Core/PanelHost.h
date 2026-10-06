/*
 * Project Ambrose by Imjustchico
 * What the panel program's own screens may ask of it over the host channel, and nothing else: the list with This computer at its head, a probe of every entry while the list is shown, a look at the certificate an address serves before it is pinned, adding, pairing, renaming, editing and forgetting an entry, opening one in a window of its own, and what the about screen shows. Each answer is JSON the screens read; a refusal names why. Opening is handed to a function the program supplies, so a test sees what would open without a window, and the probe goes through the transport the program is given, so a test records every connection that would be made.
 */

#ifndef AMBROSE_PANELHOST_H
#define AMBROSE_PANELHOST_H

#include "PanelList.h"
#include "PanelProbe.h"
#include "ThisComputer.h"

#include <filesystem>
#include <functional>
#include <string>

struct PanelHostBuild
{
    std::string Version = {};
    std::string Revision = {};
    std::string WebViewVersion = {};
};

class PanelHost
{
public:
    using Opener = std::function<bool(PanelEntry const& entry, std::string const& start, std::string& error)>;
    using Clock = std::function<int64()>;

    PanelHost(PanelList& list, PanelTransport& transport, AdminAsker& asker, std::filesystem::path dataFolder, PanelHostBuild build, Opener open, Clock now,
        uint16 adminPort = ThisComputer::DefaultAdminPort);

    std::string Answer(std::string const& message);

    static constexpr int64 ThisComputerId = 0;

private:
    std::string Handle(std::string const& method, std::string const& path, std::string const& body, int& status);

    PanelList& _list;
    PanelTransport& _transport;
    AdminAsker& _asker;
    std::filesystem::path _dataFolder;
    PanelHostBuild _build;
    Opener _open;
    Clock _now;
    uint16 _adminPort;
};

#endif
