/*
 * Project Ambrose by Imjustchico
 * Per-call-site cache of a constant category's logger index and effective level, keyed by routing generation, holding as well the file, line and function the call sits at, which the compiler folds in and which cost the call nothing.
 */

#ifndef AMBROSE_LOGSITE_H
#define AMBROSE_LOGSITE_H

#include "LogCommon.h"
#include "LogSource.h"

#include <atomic>
#include <string_view>

class LogSite
{
public:
    constexpr explicit LogSite(std::string_view category) noexcept : _category(category) { }
    constexpr LogSite(std::string_view category, LogSource source) noexcept : _category(category), _source(source) { }

    LogSite(LogSite const&) = delete;
    LogSite& operator=(LogSite const&) = delete;

    constexpr std::string_view GetCategory() const noexcept { return _category; }
    constexpr LogSource const& GetSource() const noexcept { return _source; }
    uint64 LoadCache() const noexcept { return _cache.load(std::memory_order_relaxed); }
    void StoreCache(uint64 value) const noexcept { _cache.store(value, std::memory_order_relaxed); }

    static constexpr uint64 Pack(uint64 generation, uint16 loggerIndex, uint8 effectiveLevel) noexcept
    {
        return (generation << 24) | (uint64{ loggerIndex } << 8) | effectiveLevel;
    }

    static constexpr uint64 GenerationOf(uint64 packed) noexcept { return packed >> 24; }
    static constexpr uint16 LoggerIndexOf(uint64 packed) noexcept { return static_cast<uint16>(packed >> 8); }
    static constexpr uint8 LevelOf(uint64 packed) noexcept { return static_cast<uint8>(packed & 0xFF); }

private:
    std::string_view _category;
    LogSource _source{};
    mutable std::atomic<uint64> _cache{ 0 };
};

#endif
