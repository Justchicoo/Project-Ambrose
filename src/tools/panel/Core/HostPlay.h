/*
 * Project Ambrose by Imjustchico
 * Two decisions hosting on this computer makes before anything starts. Play starts the client through the launcher against the local login server only once the supervisor reports that server ready, and until then says what it waits for, the server's state and its start step. A private database from the package's server component runs only when every one of its files matches the SHA-256 its published list gives, so a changed or missing file is refused before any of it runs, and the refusal names the file.
 */

#ifndef AMBROSE_HOSTPLAY_H
#define AMBROSE_HOSTPLAY_H

#include "Types.h"

#include <filesystem>
#include <string>
#include <vector>

struct PlayDecision
{
    bool Ready = false;
    std::string Waiting = {};
    std::vector<std::string> LauncherArguments = {};
};

class HostPlay
{
public:
    static constexpr char const* LoginApp = "loginserver";
    static constexpr char const* ChecksumFile = "SHA256SUMS";

    HostPlay() = delete;

    static PlayDecision Decide(std::string const& appsJson, std::string const& host, uint16 loginPort);
    static bool VerifyDatabase(std::filesystem::path const& folder, std::string& error);
};

#endif
