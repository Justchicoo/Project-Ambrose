/*
 * Project Ambrose by Imjustchico
 * Formats the login salt as the decimal session id, seconds and milliseconds concatenated without padding.
 */

#include "LoginSalt.h"

#include <fmt/format.h>

std::string LoginSalt::ToString() const
{
    return fmt::format("{}{}{}", SessionId, Seconds, Milliseconds);
}
