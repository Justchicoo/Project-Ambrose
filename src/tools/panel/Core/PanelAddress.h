/*
 * Project Ambrose by Imjustchico
 * Where a panel in the program's list lives and how its certificate is trusted: loopback over plain HTTP, a certificate a public authority vouches for, one pinned to a SHA-256 fingerprint, or plain HTTP beyond loopback, which is refused unless the operator opts in for that entry and then carries a warning that the password and the session cross the network unencrypted. A 17.180 pairing line is read here too, giving the address, the pin and the one-time token together, so a pinned panel is never trusted on first use.
 */

#ifndef AMBROSE_PANELADDRESS_H
#define AMBROSE_PANELADDRESS_H

#include "ShellRules.h"

#include <optional>
#include <string>
#include <string_view>

enum class PanelTrust
{
    Loopback,
    Public,
    Pinned,
    PlainOptIn,
};

struct PanelAddress
{
    ShellOrigin Origin;
    PanelTrust Trust = PanelTrust::Public;
    std::string Pin = {};

    std::string Url() const { return Origin.Describe() + "/"; }
    std::string Warning() const;

    static std::optional<PanelAddress> Parse(std::string_view text, PanelTrust trust, std::string_view pin, std::string& error);
    static bool IsLoopback(ShellOrigin const& origin);
    static std::string_view Name(PanelTrust trust);
    static std::optional<PanelTrust> TrustNamed(std::string_view name);
    static std::string Grouped(std::string_view fingerprint);
};

struct PanelPairing
{
    PanelAddress Address;
    std::string Token;
    std::string Link;

    static std::optional<PanelPairing> Parse(std::string_view line, std::string& error);
};

#endif
