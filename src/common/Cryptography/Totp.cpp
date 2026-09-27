/*
 * Project Ambrose by Imjustchico
 * Makes each step's code with Botan's HOTP over HMAC-SHA-1, reads a typed code with its spaces and dashes taken out, compares it with every step in the window in constant time, and writes the otpauth link with its label and issuer percent-encoded.
 */

#include "Totp.h"
#include "Base32.h"
#include "ConstantTime.h"

#include <botan/otp.h>

#include <fmt/format.h>

namespace
{
    std::string PercentEncoded(std::string_view text)
    {
        std::string out;
        out.reserve(text.size());
        for (char const c : text)
        {
            unsigned char const byte = static_cast<unsigned char>(c);
            bool const plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
            if (plain)
                out.push_back(c);
            else
                out += fmt::format("%{:02X}", byte);
        }
        return out;
    }
}

uint64 Totp::StepAt(int64 unixSeconds) noexcept
{
    return unixSeconds <= 0 ? 0 : static_cast<uint64>(unixSeconds) / StepSeconds;
}

uint32 Totp::Code(std::span<uint8 const> secret, uint64 step)
{
    Botan::HOTP hotp(secret.data(), secret.size(), "SHA-1", Digits);
    return hotp.generate_hotp(step);
}

std::string Totp::Format(uint32 code)
{
    return fmt::format("{:0{}}", code, Digits);
}

std::optional<std::string> Totp::Normalize(std::string_view typed)
{
    std::string digits;
    for (char const c : typed)
    {
        if (c == ' ' || c == '-')
            continue;
        if (c < '0' || c > '9')
            return std::nullopt;
        digits.push_back(c);
    }
    if (digits.size() != Digits)
        return std::nullopt;
    return digits;
}

std::optional<uint64> Totp::Match(std::span<uint8 const> secret, std::string_view typed, uint64 nowStep, uint32 window, uint64 lastAcceptedStep)
{
    std::optional<std::string> const code = Normalize(typed);
    if (!code)
        return std::nullopt;
    uint64 const first = nowStep > window ? nowStep - window : 0;
    uint64 const last = nowStep + window;
    std::optional<uint64> accepted;
    for (uint64 step = first; step <= last; ++step)
    {
        bool const same = Ambrose::Crypto::ConstantTimeEquals(Format(Code(secret, step)), *code);
        if (same && step > lastAcceptedStep && !accepted)
            accepted = step;
    }
    return accepted;
}

std::string Totp::Uri(std::string_view issuer, std::string_view account, std::span<uint8 const> secret)
{
    std::string const named = PercentEncoded(issuer);
    return fmt::format("otpauth://totp/{}:{}?secret={}&issuer={}&algorithm=SHA1&digits={}&period={}", named, PercentEncoded(account), Base32::Encode(secret), named, Digits,
        StepSeconds);
}
