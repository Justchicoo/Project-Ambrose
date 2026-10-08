/*
 * Project Ambrose by Imjustchico
 * Sends the panel's test mail over SMTP through the outbound HTTP library, reaching only the address it is given and reporting the SMTP server's own reply when delivery fails.
 */

#ifndef AMBROSE_PANELMAIL_H
#define AMBROSE_PANELMAIL_H

#include "Types.h"

#include <string>
#include <string_view>

struct PanelMailSettings
{
    std::string SmtpHost;
    uint16 SmtpPort = 587;
    std::string TlsMode = "starttls";
    std::string Username;
    std::string Password;
    std::string FromAddress;
    std::string FromName = "Ambrose";
};

struct PanelMailResult
{
    bool Sent = false;
    std::string Error;
};

class PanelMail
{
public:
    static PanelMailResult SendTestMail(PanelMailSettings const& settings, std::string_view toAddress);
};

#endif
