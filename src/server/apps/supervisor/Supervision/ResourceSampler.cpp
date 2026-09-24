/*
 * Project Ambrose by Imjustchico
 * Takes one round of readings and writes each one into the history under the app's own name. An app the operating system will not answer about, because it has just stopped or was never started, is skipped and its last reading forgotten, so the share worked out after it starts again is against its new process rather than against a total that belongs to a process that no longer exists. The first reading of a process writes everything except the processor share, because a share needs two readings and inventing one from a single total would report every app as having used the whole machine since it booted.
 */

#include "ResourceSampler.h"

#include <utility>

ResourceSampler::ResourceSampler(Ambrose::SeriesStore& store, Reader reader)
    : _store(store), _reader(reader ? std::move(reader) : Reader(&Ambrose::ProcessInfo::SnapshotOf)),
      _cores(Ambrose::ProcessInfo::CoreCount())
{
}

std::vector<std::string> ResourceSampler::SeriesNames()
{
    return { CpuSeries, MemorySeries, ThreadSeries, HandleSeries, DiskReadSeries, DiskWriteSeries };
}

void ResourceSampler::Forget(std::string const& app)
{
    _last.erase(app);
}

std::vector<ResourceSampler::Reading> ResourceSampler::Sample(std::vector<AppSnapshot> const& apps, int64 atMilliseconds)
{
    std::vector<Reading> readings;
    readings.reserve(apps.size());
    for (AppSnapshot const& app : apps)
    {
        Reading reading;
        reading.App = app.Name;
        if (!app.ProcessId || *app.ProcessId <= 0)
        {
            Forget(app.Name);
            readings.push_back(std::move(reading));
            continue;
        }

        std::optional<Ambrose::ProcessSnapshot> const taken = _reader(static_cast<uint32>(*app.ProcessId));
        if (!taken)
        {
            Forget(app.Name);
            readings.push_back(std::move(reading));
            continue;
        }

        reading.Sampled = true;
        reading.Process = *taken;

        Last& last = _last[app.Name];
        if (last.Held && atMilliseconds > last.AtMilliseconds && taken->CpuMicroseconds >= last.Process.CpuMicroseconds)
        {
            uint64 const elapsed = static_cast<uint64>(atMilliseconds - last.AtMilliseconds) * 1000;
            reading.CpuPercent = Ambrose::ProcessInfo::CpuShare(last.Process, *taken, elapsed, _cores);
            _store.Add(app.Name, CpuSeries, atMilliseconds, reading.CpuPercent);
        }
        last.Held = true;
        last.AtMilliseconds = atMilliseconds;
        last.Process = *taken;

        _store.Add(app.Name, MemorySeries, atMilliseconds, static_cast<double>(taken->ResidentBytes));
        _store.Add(app.Name, ThreadSeries, atMilliseconds, static_cast<double>(taken->ThreadCount));
        _store.Add(app.Name, HandleSeries, atMilliseconds, static_cast<double>(taken->OpenHandles));
        _store.Add(app.Name, DiskReadSeries, atMilliseconds, static_cast<double>(taken->DiskReadBytes));
        _store.Add(app.Name, DiskWriteSeries, atMilliseconds, static_cast<double>(taken->DiskWriteBytes));
        readings.push_back(std::move(reading));
    }
    return readings;
}
