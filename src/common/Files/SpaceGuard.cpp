/*
 * Project Ambrose by Imjustchico
 * Reads a volume's space from the nearest folder that exists: statvfs with the device of the folder and the mount point found by walking up until the device changes, or GetDiskFreeSpaceExW with the volume's serial and the root GetVolumePathNameW gives; holds the ledger under one lock, refuses a reservation that would leave less than the minimum once every other reservation is counted, and gives a reservation's bytes back exactly once whether it is settled, released or dropped.
 */

#include "SpaceGuard.h"

#include <fmt/format.h>

#include <algorithm>
#include <iterator>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <cerrno>
#include <cstring>
#endif

namespace
{
    std::filesystem::path NearestExisting(std::filesystem::path where)
    {
        std::error_code code;
        where = std::filesystem::absolute(where, code).lexically_normal();
        while (!where.empty() && !std::filesystem::exists(where, code))
        {
            std::filesystem::path parent = where.parent_path();
            if (parent == where)
                break;
            where = std::move(parent);
        }
        return where;
    }

    std::string Bytes(uint64 bytes)
    {
        constexpr char const* Units[] = { "bytes", "KiB", "MiB", "GiB", "TiB", "PiB" };
        double value = static_cast<double>(bytes);
        std::size_t unit = 0;
        while (value >= 1024.0 && unit + 1 < std::size(Units))
        {
            value /= 1024.0;
            ++unit;
        }
        if (unit == 0)
            return fmt::format("{} bytes", bytes);
        return fmt::format("{:.1f} {} ({} bytes)", value, Units[unit], bytes);
    }
}

std::optional<Ambrose::VolumeSpace> Ambrose::SystemSpaceProbe::Probe(std::filesystem::path const& where, std::string& error) const
{
    std::filesystem::path const existing = NearestExisting(where);
    VolumeSpace space;
#ifdef _WIN32
    std::wstring const path = existing.wstring();
    wchar_t root[MAX_PATH + 1] = {};
    if (!GetVolumePathNameW(path.c_str(), root, MAX_PATH))
    {
        error = fmt::format("the volume holding the folder could not be found (error {})", GetLastError());
        return std::nullopt;
    }
    ULARGE_INTEGER available = {};
    ULARGE_INTEGER total = {};
    ULARGE_INTEGER totalFree = {};
    if (!GetDiskFreeSpaceExW(root, &available, &total, &totalFree))
    {
        error = fmt::format("the free space of the volume could not be read (error {})", GetLastError());
        return std::nullopt;
    }
    DWORD serial = 0;
    if (!GetVolumeInformationW(root, nullptr, 0, &serial, nullptr, nullptr, nullptr, 0))
    {
        error = fmt::format("the volume could not be identified (error {})", GetLastError());
        return std::nullopt;
    }
    space.Id = serial;
    std::filesystem::path const name(root);
    std::u8string const utf8 = name.u8string();
    space.Name = std::string(utf8.begin(), utf8.end());
    space.Free = available.QuadPart;
    space.Total = total.QuadPart;
#else
    struct stat status = {};
    if (::stat(existing.c_str(), &status) != 0)
    {
        error = fmt::format("the folder's volume could not be read: {}", std::strerror(errno));
        return std::nullopt;
    }
    struct statvfs figures = {};
    if (::statvfs(existing.c_str(), &figures) != 0)
    {
        error = fmt::format("the free space of the volume could not be read: {}", std::strerror(errno));
        return std::nullopt;
    }
    std::filesystem::path mount = existing;
    while (mount.has_parent_path() && mount.parent_path() != mount)
    {
        struct stat above = {};
        if (::stat(mount.parent_path().c_str(), &above) != 0 || above.st_dev != status.st_dev)
            break;
        mount = mount.parent_path();
    }
    space.Id = static_cast<uint64>(status.st_dev);
    space.Name = mount.string();
    space.Free = static_cast<uint64>(figures.f_bavail) * static_cast<uint64>(figures.f_frsize);
    space.Total = static_cast<uint64>(figures.f_blocks) * static_cast<uint64>(figures.f_frsize);
#endif
    return space;
}

std::string Ambrose::SpaceRefusal::Message() const
{
    return fmt::format("Writing {} to {} would leave less than its minimum of {} free: it has {} free and {} of that is reserved by other writes",
        Bytes(Requested), Volume, Bytes(Minimum), Bytes(Free), Bytes(Reserved));
}

Ambrose::SpaceReservation::~SpaceReservation()
{
    Release();
}

Ambrose::SpaceReservation::SpaceReservation(SpaceReservation&& other) noexcept
    : _guard(std::exchange(other._guard, nullptr)), _volume(other._volume), _bytes(std::exchange(other._bytes, 0))
{
}

Ambrose::SpaceReservation& Ambrose::SpaceReservation::operator=(SpaceReservation&& other) noexcept
{
    if (this != &other)
    {
        Release();
        _guard = std::exchange(other._guard, nullptr);
        _volume = other._volume;
        _bytes = std::exchange(other._bytes, 0);
    }
    return *this;
}

void Ambrose::SpaceReservation::Settle() noexcept
{
    Release();
}

void Ambrose::SpaceReservation::Release() noexcept
{
    if (!_guard)
        return;
    _guard->GiveBack(_volume, _bytes);
    _guard = nullptr;
    _bytes = 0;
}

Ambrose::SpaceGuard::SpaceGuard(std::shared_ptr<SpaceProbe const> probe) : _probe(probe ? std::move(probe) : std::make_shared<SystemSpaceProbe>())
{
}

void Ambrose::SpaceGuard::SetMinimum(uint64 bytes, uint32 percent)
{
    std::lock_guard const lock(_mutex);
    _minimumBytes = bytes;
    _minimumPercent = std::min<uint32>(percent, 100);
}

uint64 Ambrose::SpaceGuard::MinimumFor(uint64 total) const
{
    std::lock_guard const lock(_mutex);
    return std::max(_minimumBytes, total / 100 * _minimumPercent + total % 100 * _minimumPercent / 100);
}

uint64 Ambrose::SpaceGuard::ReservedOn(uint64 volume) const
{
    std::lock_guard const lock(_mutex);
    auto const found = _ledger.find(volume);
    return found == _ledger.end() ? 0 : found->second;
}

std::optional<Ambrose::VolumeFigures> Ambrose::SpaceGuard::Figures(std::filesystem::path const& where, std::string& error) const
{
    std::optional<VolumeSpace> const space = _probe->Probe(where, error);
    if (!space)
        return std::nullopt;
    VolumeFigures figures;
    figures.Space = *space;
    figures.Minimum = MinimumFor(space->Total);
    figures.Reserved = ReservedOn(space->Id);
    return figures;
}

std::optional<Ambrose::SpaceReservation> Ambrose::SpaceGuard::Reserve(std::filesystem::path const& where, uint64 bytes, SpaceRefusal& refusal, std::string& error)
{
    std::optional<VolumeSpace> const space = _probe->Probe(where, error);
    if (!space)
        return std::nullopt;
    uint64 const minimum = MinimumFor(space->Total);
    std::lock_guard const lock(_mutex);
    auto const found = _ledger.find(space->Id);
    uint64 const reserved = found == _ledger.end() ? 0 : found->second;
    uint64 const available = space->Free > reserved ? space->Free - reserved : 0;
    if (bytes > available || available - bytes < minimum)
    {
        refusal.Volume = space->Name;
        refusal.Free = space->Free;
        refusal.Reserved = reserved;
        refusal.Minimum = minimum;
        refusal.Requested = bytes;
        error = refusal.Message();
        return std::nullopt;
    }
    _ledger[space->Id] = reserved + bytes;
    return SpaceReservation(this, space->Id, bytes);
}

void Ambrose::SpaceGuard::GiveBack(uint64 volume, uint64 bytes) noexcept
{
    std::lock_guard const lock(_mutex);
    auto const found = _ledger.find(volume);
    if (found == _ledger.end())
        return;
    found->second -= std::min(found->second, bytes);
    if (found->second == 0)
        _ledger.erase(found);
}
