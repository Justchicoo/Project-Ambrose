/*
 * Project Ambrose by Imjustchico
 * Every history the supervisor keeps, named by the subject it describes and the series within it, so an app's processor share and a realm's player count are read the same way and a later milestone adds a series rather than a second store. It is written to and read from more than one thread, the sampler on its timer and the admin API on whichever thread the request arrived on, so every entry is taken under one lock and a range is copied out rather than handed out by reference. Saving writes the coarse resolution only, which is the thirty days worth keeping, and writes it beside the real file before moving it into place, so a supervisor killed mid-write loses the newest samples rather than every day it had. Loading replaces what a bucket holds rather than adding to it, so the same file read twice leaves the history it describes exactly once.
 */

#ifndef AMBROSE_SERIESSTORE_H
#define AMBROSE_SERIESSTORE_H

#include "TimeSeries.h"

#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Ambrose
{
    struct SeriesName
    {
        std::string Subject;
        std::string Series;

        bool operator<(SeriesName const& other) const
        {
            return Subject == other.Subject ? Series < other.Series : Subject < other.Subject;
        }
    };

    class SeriesStore
    {
    public:
        static constexpr char const* Magic = "AMBSERIES";
        static constexpr uint32 Version = 1;

        SeriesStore() = default;
        explicit SeriesStore(std::vector<SeriesResolution> resolutions);

        SeriesStore(SeriesStore const&) = delete;
        SeriesStore& operator=(SeriesStore const&) = delete;

        void Add(std::string_view subject, std::string_view series, int64 atMilliseconds, double value);
        std::vector<SeriesPoint> Between(std::string_view subject, std::string_view series, int64 fromMilliseconds,
            int64 toMilliseconds, std::size_t mostPoints) const;

        std::vector<std::string> Subjects() const;
        std::vector<std::string> SeriesOf(std::string_view subject) const;
        bool Has(std::string_view subject, std::string_view series) const;
        std::size_t Count() const;
        void Clear();

        bool Save(std::filesystem::path const& file, std::string& error) const;
        bool Load(std::filesystem::path const& file, std::string& error);

    private:
        TimeSeries& TakeLocked(std::string_view subject, std::string_view series);

        mutable std::mutex _mutex;
        std::vector<SeriesResolution> _resolutions = TimeSeries::DefaultResolutions();
        std::map<SeriesName, TimeSeries> _series;
    };
}

#endif
