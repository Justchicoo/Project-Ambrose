/*
 * Project Ambrose by Imjustchico
 * Keeps the disk the databases, logs and backups share from filling: a volume is known by its device or serial and named by its mount point or drive, its free space comes from a probe a test can replace, the minimum is the larger of a byte figure and a share of the volume, and every writer reserves what it will write against a per-volume ledger before a byte is streamed, the reservation released or settled when the write ends, so two writers cannot each see the same free space; a refusal names the volume with its free, reserved, minimum and requested figures.
 */

#ifndef AMBROSE_SPACEGUARD_H
#define AMBROSE_SPACEGUARD_H

#include "Types.h"

#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace Ambrose
{
    struct VolumeSpace
    {
        uint64 Id = 0;
        std::string Name = {};
        uint64 Free = 0;
        uint64 Total = 0;
    };

    class SpaceProbe
    {
    public:
        virtual ~SpaceProbe() = default;
        virtual std::optional<VolumeSpace> Probe(std::filesystem::path const& where, std::string& error) const = 0;
    };

    class SystemSpaceProbe final : public SpaceProbe
    {
    public:
        std::optional<VolumeSpace> Probe(std::filesystem::path const& where, std::string& error) const override;
    };

    struct SpaceRefusal
    {
        std::string Volume = {};
        uint64 Free = 0;
        uint64 Reserved = 0;
        uint64 Minimum = 0;
        uint64 Requested = 0;

        std::string Message() const;
    };

    struct VolumeFigures
    {
        VolumeSpace Space = {};
        uint64 Minimum = 0;
        uint64 Reserved = 0;
    };

    class SpaceGuard;

    class SpaceReservation
    {
    public:
        SpaceReservation() noexcept = default;
        ~SpaceReservation();
        SpaceReservation(SpaceReservation&& other) noexcept;
        SpaceReservation& operator=(SpaceReservation&& other) noexcept;
        SpaceReservation(SpaceReservation const&) = delete;
        SpaceReservation& operator=(SpaceReservation const&) = delete;

        bool Held() const noexcept { return _guard != nullptr; }
        uint64 Bytes() const noexcept { return _bytes; }
        uint64 Volume() const noexcept { return _volume; }
        void Settle() noexcept;
        void Release() noexcept;

    private:
        friend class SpaceGuard;

        SpaceReservation(SpaceGuard* guard, uint64 volume, uint64 bytes) noexcept : _guard(guard), _volume(volume), _bytes(bytes) {}

        SpaceGuard* _guard = nullptr;
        uint64 _volume = 0;
        uint64 _bytes = 0;
    };

    class SpaceGuard
    {
    public:
        static constexpr uint64 DefaultMinimumBytes = 1073741824ull;
        static constexpr uint32 DefaultMinimumPercent = 5;

        explicit SpaceGuard(std::shared_ptr<SpaceProbe const> probe = nullptr);

        SpaceGuard(SpaceGuard const&) = delete;
        SpaceGuard& operator=(SpaceGuard const&) = delete;

        void SetMinimum(uint64 bytes, uint32 percent);
        uint64 MinimumFor(uint64 total) const;
        std::optional<SpaceReservation> Reserve(std::filesystem::path const& where, uint64 bytes, SpaceRefusal& refusal, std::string& error);
        std::optional<VolumeFigures> Figures(std::filesystem::path const& where, std::string& error) const;
        uint64 ReservedOn(uint64 volume) const;

    private:
        friend class SpaceReservation;

        void GiveBack(uint64 volume, uint64 bytes) noexcept;

        std::shared_ptr<SpaceProbe const> _probe;
        mutable std::mutex _mutex;
        std::map<uint64, uint64> _ledger;
        uint64 _minimumBytes = DefaultMinimumBytes;
        uint32 _minimumPercent = DefaultMinimumPercent;
    };
}

#endif
