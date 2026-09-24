/*
 * Project Ambrose by Imjustchico
 * Reads and writes the store's own format: a magic and a version, then each series with the subject and name it is known by, and each of its resolutions with the size of its buckets and the buckets themselves. Numbers are written little-endian by hand rather than by copying the structures, so a file written on one machine reads on another and a field added later does not move what is already there. A file whose magic, version or lengths do not agree is refused whole and named, because half a history read as if it were all of it would show a gap that never happened. Saving goes to a temporary beside the real file and is moved into place, since a history is worth more than the newest few samples and a torn file is worth nothing.
 */

#include "SeriesStore.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <system_error>

namespace
{
    using namespace Ambrose;

    void PutUnsigned(std::string& out, uint64 value, int bytes)
    {
        for (int at = 0; at < bytes; ++at)
            out.push_back(static_cast<char>((value >> (8 * at)) & 0xFF));
    }

    void PutDouble(std::string& out, double value)
    {
        uint64 bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        PutUnsigned(out, bits, 8);
    }

    bool TakeUnsigned(std::string_view& in, uint64& value, int bytes)
    {
        if (in.size() < static_cast<std::size_t>(bytes))
            return false;
        value = 0;
        for (int at = 0; at < bytes; ++at)
            value |= static_cast<uint64>(static_cast<unsigned char>(in[at])) << (8 * at);
        in.remove_prefix(bytes);
        return true;
    }

    bool TakeDouble(std::string_view& in, double& value)
    {
        uint64 bits = 0;
        if (!TakeUnsigned(in, bits, 8))
            return false;
        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }

    bool TakeText(std::string_view& in, std::string& value)
    {
        uint64 length = 0;
        if (!TakeUnsigned(in, length, 2) || in.size() < length)
            return false;
        value.assign(in.substr(0, static_cast<std::size_t>(length)));
        in.remove_prefix(static_cast<std::size_t>(length));
        return true;
    }

    void PutText(std::string& out, std::string const& value)
    {
        PutUnsigned(out, value.size(), 2);
        out.append(value);
    }
}

namespace Ambrose
{
    SeriesStore::SeriesStore(std::vector<SeriesResolution> resolutions) : _resolutions(std::move(resolutions))
    {
    }

    TimeSeries& SeriesStore::TakeLocked(std::string_view subject, std::string_view series)
    {
        SeriesName name{ std::string(subject), std::string(series) };
        auto const found = _series.find(name);
        if (found != _series.end())
            return found->second;
        return _series.emplace(std::move(name), TimeSeries(_resolutions)).first->second;
    }

    void SeriesStore::Add(std::string_view subject, std::string_view series, int64 atMilliseconds, double value)
    {
        if (subject.empty() || series.empty())
            return;
        std::lock_guard const lock(_mutex);
        TakeLocked(subject, series).Add(atMilliseconds, value);
    }

    std::vector<SeriesPoint> SeriesStore::Between(std::string_view subject, std::string_view series, int64 fromMilliseconds,
        int64 toMilliseconds, std::size_t mostPoints) const
    {
        std::lock_guard const lock(_mutex);
        auto const found = _series.find(SeriesName{ std::string(subject), std::string(series) });
        if (found == _series.end())
            return {};
        return found->second.Between(fromMilliseconds, toMilliseconds, mostPoints);
    }

    std::vector<std::string> SeriesStore::Subjects() const
    {
        std::lock_guard const lock(_mutex);
        std::vector<std::string> subjects;
        for (auto const& [name, series] : _series)
        {
            (void)series;
            if (subjects.empty() || subjects.back() != name.Subject)
                subjects.push_back(name.Subject);
        }
        return subjects;
    }

    std::vector<std::string> SeriesStore::SeriesOf(std::string_view subject) const
    {
        std::lock_guard const lock(_mutex);
        std::vector<std::string> names;
        for (auto const& [name, series] : _series)
        {
            (void)series;
            if (name.Subject == subject)
                names.push_back(name.Series);
        }
        return names;
    }

    bool SeriesStore::Has(std::string_view subject, std::string_view series) const
    {
        std::lock_guard const lock(_mutex);
        return _series.contains(SeriesName{ std::string(subject), std::string(series) });
    }

    std::size_t SeriesStore::Count() const
    {
        std::lock_guard const lock(_mutex);
        return _series.size();
    }

    void SeriesStore::Clear()
    {
        std::lock_guard const lock(_mutex);
        _series.clear();
    }

    bool SeriesStore::Save(std::filesystem::path const& file, std::string& error) const
    {
        std::string out;
        out.append(Magic);
        PutUnsigned(out, Version, 4);

        {
            std::lock_guard const lock(_mutex);
            PutUnsigned(out, _series.size(), 4);
            for (auto const& [name, series] : _series)
            {
                PutText(out, name.Subject);
                PutText(out, name.Series);
                std::vector<std::pair<int64, std::vector<SeriesBucket>>> const tiers = series.Export();
                PutUnsigned(out, tiers.size(), 4);
                for (auto const& [bucketMilliseconds, buckets] : tiers)
                {
                    PutUnsigned(out, static_cast<uint64>(bucketMilliseconds), 8);
                    PutUnsigned(out, buckets.size(), 4);
                    for (SeriesBucket const& bucket : buckets)
                    {
                        PutUnsigned(out, static_cast<uint64>(bucket.Bucket), 8);
                        PutDouble(out, bucket.Sum);
                        PutDouble(out, bucket.Lowest);
                        PutDouble(out, bucket.Highest);
                        PutUnsigned(out, bucket.Samples, 4);
                    }
                }
            }
        }

        std::filesystem::path const temporary = std::filesystem::path(file).concat(".writing");
        std::error_code code;
        std::filesystem::create_directories(file.parent_path(), code);
        {
            std::ofstream writing(temporary, std::ios::binary | std::ios::trunc);
            if (!writing)
            {
                error = "the history could not be opened for writing";
                return false;
            }
            writing.write(out.data(), static_cast<std::streamsize>(out.size()));
            if (!writing)
            {
                error = "the history could not be written";
                return false;
            }
        }
        std::filesystem::rename(temporary, file, code);
        if (code)
        {
            std::filesystem::remove(file, code);
            std::filesystem::rename(temporary, file, code);
        }
        if (code)
        {
            error = "the history could not be moved into place: " + code.message();
            return false;
        }
        return true;
    }

    bool SeriesStore::Load(std::filesystem::path const& file, std::string& error)
    {
        std::error_code code;
        if (!std::filesystem::exists(file, code))
        {
            error = "there is no history to read yet";
            return false;
        }
        std::ifstream reading(file, std::ios::binary);
        if (!reading)
        {
            error = "the history could not be opened for reading";
            return false;
        }
        std::string text((std::istreambuf_iterator<char>(reading)), std::istreambuf_iterator<char>());

        std::string_view left(text);
        std::size_t const magic = std::strlen(Magic);
        if (left.size() < magic || left.substr(0, magic) != Magic)
        {
            error = "the history does not begin the way one written here does";
            return false;
        }
        left.remove_prefix(magic);

        uint64 version = 0;
        if (!TakeUnsigned(left, version, 4) || version != Version)
        {
            error = "the history was written in a version this server does not read";
            return false;
        }

        uint64 count = 0;
        if (!TakeUnsigned(left, count, 4))
        {
            error = "the history says nothing about how many series it holds";
            return false;
        }

        std::map<SeriesName, std::vector<std::pair<int64, std::vector<SeriesBucket>>>> read;
        for (uint64 at = 0; at < count; ++at)
        {
            SeriesName name;
            uint64 tiers = 0;
            if (!TakeText(left, name.Subject) || !TakeText(left, name.Series) || !TakeUnsigned(left, tiers, 4))
            {
                error = "the history ends in the middle of a series";
                return false;
            }
            std::vector<std::pair<int64, std::vector<SeriesBucket>>> held;
            held.reserve(static_cast<std::size_t>(std::min<uint64>(tiers, 64)));
            for (uint64 tier = 0; tier < tiers; ++tier)
            {
                uint64 bucketMilliseconds = 0;
                uint64 buckets = 0;
                if (!TakeUnsigned(left, bucketMilliseconds, 8) || !TakeUnsigned(left, buckets, 4))
                {
                    error = "the history ends in the middle of a resolution";
                    return false;
                }
                std::vector<SeriesBucket> inTier;
                inTier.reserve(static_cast<std::size_t>(std::min<uint64>(buckets, 1u << 20)));
                for (uint64 index = 0; index < buckets; ++index)
                {
                    SeriesBucket bucket;
                    uint64 at2 = 0;
                    uint64 samples = 0;
                    if (!TakeUnsigned(left, at2, 8) || !TakeDouble(left, bucket.Sum) || !TakeDouble(left, bucket.Lowest)
                        || !TakeDouble(left, bucket.Highest) || !TakeUnsigned(left, samples, 4))
                    {
                        error = "the history ends in the middle of a bucket";
                        return false;
                    }
                    bucket.Bucket = static_cast<int64>(at2);
                    bucket.Samples = static_cast<uint32>(samples);
                    inTier.push_back(bucket);
                }
                held.emplace_back(static_cast<int64>(bucketMilliseconds), std::move(inTier));
            }
            read.emplace(std::move(name), std::move(held));
        }

        std::lock_guard const lock(_mutex);
        for (auto const& [name, tiers] : read)
            TakeLocked(name.Subject, name.Series).Restore(tiers);
        return true;
    }
}
