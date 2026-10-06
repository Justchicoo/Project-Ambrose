/*
 * Project Ambrose by Imjustchico
 * Whether a panel in the list answers, named in one of four states: answering, unreachable, certificate changed, or not an Ambrose panel. The probe reads only the panel's public GET /api/panel/session, and the transport it goes through checks a pinned certificate before anything is sent, so a panel whose certificate changed is told nothing. The transport is an interface so a test answers as a panel would and records every connection the program would have opened.
 */

#ifndef AMBROSE_PANELPROBE_H
#define AMBROSE_PANELPROBE_H

#include "PanelAddress.h"

#include <string>
#include <string_view>

enum class PanelState
{
    Answering,
    Unreachable,
    CertificateChanged,
    NotAPanel,
};

struct PanelReply
{
    bool Connected = false;
    bool PinRefused = false;
    std::string Served = {};
    int Status = 0;
    std::string Body = {};
    std::string Error = {};
};

class PanelTransport
{
public:
    virtual ~PanelTransport() = default;
    virtual PanelReply Get(PanelAddress const& address, std::string_view path) = 0;
};

struct PanelProbeResult
{
    PanelState State = PanelState::Unreachable;
    std::string Word = {};
    bool SignedIn = false;
};

class PanelProbe
{
public:
    static constexpr std::string_view Path = "/api/panel/session";

    PanelProbe() = delete;

    static PanelProbeResult Run(PanelTransport& transport, PanelAddress const& address);
    static PanelProbeResult Classify(PanelAddress const& address, PanelReply const& reply);
    static std::string_view Name(PanelState state);
};

#endif
