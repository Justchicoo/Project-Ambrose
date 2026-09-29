/*
 * Project Ambrose by Imjustchico
 * How secret settings' values are hidden in text a person or a stream may see, asking the settings table which settings are secrets: the admin and panel tokens, the passwords inside database connection strings and the verifier keys never leave the process in the clear, whether in a log line or in a configuration file read through the panel, which is masked line by line with its layout kept.
 */

#ifndef AMBROSE_LOGREDACTION_H
#define AMBROSE_LOGREDACTION_H

#include <string>
#include <string_view>
#include <vector>

class LogRedaction
{
public:
    static constexpr std::string_view Mask = "***";

    LogRedaction() = delete;

    static bool IsSecretSetting(std::string_view key);
    static std::string RedactSettingValue(std::string_view key, std::string_view value);
    static std::string MaskSecretValue(std::string_view key, std::string_view value);
    static std::string DescribeSettingChange(std::string_view key, std::string_view value, std::string_view source);
    static std::string Redact(std::string_view text);
    static std::string RedactConf(std::string_view text, std::vector<std::string>* redactedKeys = nullptr);
};

#endif
