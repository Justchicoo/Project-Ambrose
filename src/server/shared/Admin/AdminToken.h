/*
 * Project Ambrose by Imjustchico
 * The admin API bearer token: where it comes from, what makes one valid, writing a generated token into a fresh file only the current user can read, and putting those permissions back on a token file that already exists.
 */

#ifndef AMBROSE_ADMINTOKEN_H
#define AMBROSE_ADMINTOKEN_H

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

struct AdminSettings;

struct AdminTokenResult
{
    std::string Token;
    std::filesystem::path File;
    std::string Source;
    bool Generated = false;
    std::string Error;
    std::string Warning;

    bool Succeeded() const { return Error.empty(); }
};

namespace AdminToken
{
    std::string Generate();
    std::optional<std::string> Validate(std::string_view token);
    std::filesystem::path DefaultFile(std::string const& appName, std::filesystem::path const& dataFolder);
    bool WriteSecretFile(std::filesystem::path const& file, std::string_view text, std::string& error);
    bool SecureFile(std::filesystem::path const& file, std::string& error);
    AdminTokenResult Resolve(AdminSettings const& settings, std::string const& appName, std::filesystem::path const& dataFolder);
}

#endif
