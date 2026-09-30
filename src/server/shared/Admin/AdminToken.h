/*
 * Project Ambrose by Imjustchico
 * The admin API bearer token: where it comes from, what makes one valid, the folder a generated token falls back to when the machine has no data folder, reading it the way the app does without ever writing one, for a command-line caller that asks a running app, writing a generated token into a fresh file only the current user can read, and putting those permissions back on a token file that already exists; a secret the supervisor keeps for the machine, such as its keyring, may name the system account and the administrators as readers too, which on Windows adds them to the file's access list and elsewhere changes nothing.
 */

#ifndef AMBROSE_ADMINTOKEN_H
#define AMBROSE_ADMINTOKEN_H

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

struct ListenerSettings;

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
    enum class SecretReaders
    {
        Owner,
        OwnerAndAdministrators
    };

    std::string Generate();
    std::optional<std::string> Validate(std::string_view token);
    std::filesystem::path DefaultFile(std::string const& appName, std::filesystem::path const& dataFolder, std::filesystem::path const& fallbackFolder = {});
    bool WriteSecretFile(std::filesystem::path const& file, std::string_view text, std::string& error, SecretReaders readers = SecretReaders::Owner);
    bool SecureFile(std::filesystem::path const& file, std::string& error, SecretReaders readers = SecretReaders::Owner);
    AdminTokenResult Read(ListenerSettings const& settings, std::string const& appName, std::filesystem::path const& dataFolder, std::filesystem::path const& fallbackFolder = {});
    AdminTokenResult Resolve(ListenerSettings const& settings, std::string const& appName, std::filesystem::path const& dataFolder, std::filesystem::path const& fallbackFolder = {});
}

#endif
