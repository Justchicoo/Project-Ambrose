/*
 * Project Ambrose by Imjustchico
 * The error codes MSG_USER_AUTHEN_RSP and MSG_USER_VALIDATE_RSP carry, and the name each is reported by in the Reason field.
 */

#ifndef AMBROSE_AUTHRESULT_H
#define AMBROSE_AUTHRESULT_H

#include "Types.h"

#include <string_view>

enum class AuthResult : int32
{
    Success = 0,
    AccountBanned = 0x0538FBC0,
    MachineBanned = 0x44FB7BF8,
    AuthenFailed = 0x3B689180,
    AisNoLogin = 0x6311BDD6,
    Timeout = 0x512C42FF,
    FtpCapped = 0x5BFF7366,
    ErrorNoLock = 0x67DD13EA,
    FailedUpload = 0x10857D75,
    ValidateFailed = 0x0EB64359
};

namespace AuthResults
{
    constexpr std::string_view GetName(AuthResult result) noexcept
    {
        switch (result)
        {
            case AuthResult::Success: return "Success";
            case AuthResult::AccountBanned: return "AccountBanned";
            case AuthResult::MachineBanned: return "MachineBanned";
            case AuthResult::AuthenFailed: return "AuthenFailed";
            case AuthResult::AisNoLogin: return "AISNoLogin";
            case AuthResult::Timeout: return "Timeout";
            case AuthResult::FtpCapped: return "FtpCapped";
            case AuthResult::ErrorNoLock: return "ErrorNoLock";
            case AuthResult::FailedUpload: return "FailedUpload";
            case AuthResult::ValidateFailed: return "ValidateFailed";
        }
        return "Unknown";
    }
}

#endif
