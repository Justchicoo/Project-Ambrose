/*
 * Project Ambrose by Imjustchico
 * Notices that the install a server runs from has been updated: its fingerprint is the revision Bin/revision.dat names with the size and write time of Bin/WizardGraphicalClient.exe, and a poll reports a new fingerprint only once two polls in a row read the same one, so an update still being written is not taken for the finished one. A fingerprint that cannot be read, as while a patcher has a file open, is no change.
 */

#ifndef AMBROSE_CLIENTREVISIONWATCH_H
#define AMBROSE_CLIENTREVISIONWATCH_H

#include "ClientSystem.h"
#include "Types.h"

#include <filesystem>
#include <optional>
#include <string>

struct ClientFingerprint
{
    std::string Revision;
    uint64 ExecutableSize = 0;
    int64 ExecutableWriteTime = 0;

    bool operator==(ClientFingerprint const&) const = default;
    std::string Describe() const;

    static std::optional<ClientFingerprint> Read(ClientSystem const& system, std::filesystem::path const& root);
};

struct ClientRevisionStatus
{
    bool Watching = false;
    std::string Current;
    std::string Updating;
    std::string LastUpdate;
};

class ClientRevisionWatch
{
public:
    void Watch(std::filesystem::path root, ClientFingerprint current);
    void Accept(ClientFingerprint current);
    std::optional<ClientFingerprint> Poll(ClientSystem const& system);

    bool IsWatching() const noexcept { return !_root.empty(); }
    std::filesystem::path const& GetRoot() const noexcept { return _root; }
    ClientFingerprint const& GetCurrent() const noexcept { return _current; }

private:
    std::filesystem::path _root;
    ClientFingerprint _current;
    std::optional<ClientFingerprint> _seen;
};

#endif
