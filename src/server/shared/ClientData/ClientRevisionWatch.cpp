/*
 * Project Ambrose by Imjustchico
 * Reads an install's fingerprint and reports a change to it once it has held for two polls.
 */

#include "ClientRevisionWatch.h"
#include "ClientLocator.h"

#include <fmt/format.h>

#include <chrono>
#include <system_error>
#include <utility>

std::string ClientFingerprint::Describe() const
{
    std::string const revision = Revision.empty() ? std::string("revision unknown") : Revision;
    if (ExecutableSize == 0 && ExecutableWriteTime == 0)
        return revision;
    return fmt::format("{}, its program {} bytes written at {}", revision, ExecutableSize, ExecutableWriteTime);
}

std::optional<ClientFingerprint> ClientFingerprint::Read(ClientSystem const& system, std::filesystem::path const& root)
{
    std::optional<ClientInstall> const install = ClientInstall::Inspect(system, root);
    if (!install || install->Revision.empty())
        return std::nullopt;
    ClientFingerprint fingerprint;
    fingerprint.Revision = install->Revision;
    if (install->HasProgram)
    {
        std::filesystem::path const program = install->ProgramPath();
        std::error_code error;
        std::uintmax_t const size = std::filesystem::file_size(program, error);
        if (error)
            return std::nullopt;
        std::filesystem::file_time_type const written = std::filesystem::last_write_time(program, error);
        if (error)
            return std::nullopt;
        fingerprint.ExecutableSize = static_cast<uint64>(size);
        fingerprint.ExecutableWriteTime = std::chrono::duration_cast<std::chrono::seconds>(written.time_since_epoch()).count();
    }
    return fingerprint;
}

void ClientRevisionWatch::Watch(std::filesystem::path root, ClientFingerprint current)
{
    _root = std::move(root);
    _current = std::move(current);
    _seen.reset();
}

void ClientRevisionWatch::Accept(ClientFingerprint current)
{
    _current = std::move(current);
    _seen.reset();
}

std::optional<ClientFingerprint> ClientRevisionWatch::Poll(ClientSystem const& system)
{
    if (_root.empty())
        return std::nullopt;
    std::optional<ClientFingerprint> read = ClientFingerprint::Read(system, _root);
    if (!read || *read == _current)
    {
        _seen.reset();
        return std::nullopt;
    }
    if (_seen && *_seen == *read)
        return read;
    _seen = std::move(read);
    return std::nullopt;
}
