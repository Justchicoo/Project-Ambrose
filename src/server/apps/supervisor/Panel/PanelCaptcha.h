/*
 * Project Ambrose by Imjustchico
 * Verifies a sign-in captcha answer against its provider through the outbound HTTP library, failing closed when the provider cannot be reached or is not one the panel knows, and names the provider's own widget script so the sign-in page loads the script the panel names instead of carrying another host's address in the dashboard bundle.
 */

#ifndef AMBROSE_PANELCAPTCHA_H
#define AMBROSE_PANELCAPTCHA_H

#include "Types.h"

#include <string>
#include <string_view>

struct PanelCaptchaResult
{
    enum class Outcome : uint8
    {
        Verified,
        Invalid,
        Unreachable,
        Misconfigured
    };

    Outcome Result = Outcome::Misconfigured;
    std::string Detail;
};

class PanelCaptcha
{
public:
    static constexpr uint32 AfterFailures = 3;

    static std::string VerifyUrlFor(std::string_view provider);
    static std::string ScriptUrlFor(std::string_view provider);
    static PanelCaptchaResult Verify(std::string_view provider, std::string_view secret, std::string_view token, std::string_view remoteIp, std::string_view verifyUrl);
};

#endif
