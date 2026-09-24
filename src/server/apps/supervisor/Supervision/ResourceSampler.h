/*
 * Project Ambrose by Imjustchico
 * Reads what every app the supervisor runs is costing the machine and writes it into the history the panel draws from. Processor time is a total the operating system keeps, so a share is only meaningful between two readings; the sampler keeps the last reading of each app and works the share out against the time that actually passed rather than the time a round was meant to take, because a round that ran late would otherwise report a spike that never happened. An app with no process is not sampled at all, so its graph carries a gap rather than a row of zeroes that would read as an app running and idle. The reading is taken through a function so a test can hand it processes that behave exactly as the test needs, and so the benchmark measures the sampler rather than the operating system.
 */

#ifndef AMBROSE_RESOURCESAMPLER_H
#define AMBROSE_RESOURCESAMPLER_H

#include "ManagedApp.h"
#include "ProcessInfo.h"
#include "SeriesStore.h"

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class ResourceSampler
{
public:
    using Reader = std::function<std::optional<Ambrose::ProcessSnapshot>(uint32 processId)>;

    static constexpr char const* CpuSeries = "cpu_percent";
    static constexpr char const* MemorySeries = "memory_bytes";
    static constexpr char const* ThreadSeries = "threads";
    static constexpr char const* HandleSeries = "handles";
    static constexpr char const* DiskReadSeries = "disk_read_bytes";
    static constexpr char const* DiskWriteSeries = "disk_write_bytes";

    struct Reading
    {
        std::string App;
        bool Sampled = false;
        double CpuPercent = 0.0;
        Ambrose::ProcessSnapshot Process;
    };

    explicit ResourceSampler(Ambrose::SeriesStore& store, Reader reader = {});

    std::vector<Reading> Sample(std::vector<AppSnapshot> const& apps, int64 atMilliseconds);
    void Forget(std::string const& app);

    static std::vector<std::string> SeriesNames();

private:
    struct Last
    {
        int64 AtMilliseconds = 0;
        Ambrose::ProcessSnapshot Process;
        bool Held = false;
    };

    Ambrose::SeriesStore& _store;
    Reader _reader;
    uint32 _cores;
    std::unordered_map<std::string, Last> _last;
};

#endif
