/*
 * Project Ambrose by Imjustchico
 * Time-based one-time codes as RFC 6238 and every authenticator app make them, SHA-1, six digits and thirty-second steps: the step a moment falls in, the code for a step, a typed code checked against the steps either side of now and accepted only for a step later than the last one accepted, so a code that worked once never works again, and the otpauth link an authenticator app reads the secret from.
 */

#ifndef AMBROSE_TOTP_H
#define AMBROSE_TOTP_H

#include "Types.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace Totp
{
    inline constexpr uint32 StepSeconds = 30;
    inline constexpr uint32 Digits = 6;
    inline constexpr std::size_t SecretBytes = 20;

    uint64 StepAt(int64 unixSeconds) noexcept;
    uint32 Code(std::span<uint8 const> secret, uint64 step);
    std::string Format(uint32 code);
    std::optional<std::string> Normalize(std::string_view typed);
    std::optional<uint64> Match(std::span<uint8 const> secret, std::string_view typed, uint64 nowStep, uint32 window, uint64 lastAcceptedStep);
    std::string Uri(std::string_view issuer, std::string_view account, std::span<uint8 const> secret);
}

#endif
