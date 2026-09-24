/*
 * Project Ambrose by Imjustchico
 * Which settings are secrets and how their values are hidden in text a person or a stream may see: the admin token, the passwords inside database connection strings and the verifier keys never leave the process in the clear.
 */

#ifndef AMBROSE_LOGREDACTION_H
#define AMBROSE_LOGREDACTION_H

#include <string>
#include <string_view>

class LogRedaction
{
public:
    static constexpr std::string_view Mask = "***";

    LogRedaction() = delete;

    static bool IsSecretSetting(std::string_view key) noexcept;
    static std::string RedactSettingValue(std::string_view key, std::string_view value);
    static std::string DescribeSettingChange(std::string_view key, std::string_view value, std::string_view source);
    static std::string Redact(std::string_view text);
};

#endif
