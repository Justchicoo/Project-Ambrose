/*
 * Project Ambrose by Imjustchico
 * Names a probe's reply: a pin the transport refused is a changed certificate with both fingerprints said, a reply that never arrived is unreachable with the reason, and a reply is a panel only when it is the JSON object an Ambrose panel's session route answers, naming itself the panel and saying whether this browser is signed in.
 */

#include "PanelProbe.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

std::string_view PanelProbe::Name(PanelState state)
{
    switch (state)
    {
        case PanelState::Answering:
            return "answering";
        case PanelState::Unreachable:
            return "unreachable";
        case PanelState::CertificateChanged:
            return "certificate changed";
        case PanelState::NotAPanel:
            return "not an Ambrose panel";
    }
    return "unreachable";
}

PanelProbeResult PanelProbe::Run(PanelTransport& transport, PanelAddress const& address)
{
    return Classify(address, transport.Get(address, Path));
}

PanelProbeResult PanelProbe::Classify(PanelAddress const& address, PanelReply const& reply)
{
    PanelProbeResult result;
    if (reply.PinRefused)
    {
        result.State = PanelState::CertificateChanged;
        result.Word = fmt::format("{} now serves the certificate {}, not the pinned {}; nothing was sent. Confirm the new one against the supervisor's log, or pair again",
            address.Origin.Describe(), PanelAddress::Grouped(reply.Served), PanelAddress::Grouped(address.Pin));
        return result;
    }
    if (!reply.Connected)
    {
        result.State = PanelState::Unreachable;
        result.Word = fmt::format("{} did not answer: {}", address.Origin.Describe(), reply.Error.empty() ? "no reply" : reply.Error);
        return result;
    }
    nlohmann::json const body = nlohmann::json::parse(reply.Body, nullptr, false);
    if (reply.Status != 200 || !body.is_object() || body.value("app", std::string()) != "panel" || !body.contains("signed_in") || !body["signed_in"].is_boolean())
    {
        result.State = PanelState::NotAPanel;
        result.Word = fmt::format("{} answered {}, which is not an Ambrose panel's session", address.Origin.Describe(), reply.Status);
        return result;
    }
    result.State = PanelState::Answering;
    result.SignedIn = body["signed_in"].get<bool>();
    result.Word = fmt::format("{} is an Ambrose panel and answers", address.Origin.Describe());
    return result;
}
