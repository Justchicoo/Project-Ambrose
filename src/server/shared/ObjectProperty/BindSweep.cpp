/*
 * Project Ambrose by Imjustchico
 * Hands out the archive's entries to worker threads one index at a time, running on the calling thread and whichever workers could be started; each worker reads its entry through the archive's own lock, skips anything that is not a BINd file, decodes the rest in parallel and keeps its own tallies, which are merged in entry order afterwards so the report is the same however the work was split; an entry that throws is counted as unreadable or failed without letting the exception leave its thread, and issues are grouped by kind and hash, counted once per use and once per file with the first file, path and detail they appear with, a root class that stops a file from decoding included among the unknown classes.
 */

#include "BindSweep.h"
#include "KiwadArchive.h"

#include <algorithm>
#include <atomic>
#include <exception>
#include <limits>
#include <map>
#include <new>
#include <set>
#include <thread>
#include <utility>

namespace
{
    constexpr std::size_t NoEntry = std::numeric_limits<std::size_t>::max();
    constexpr std::string_view RootPath = "the root object";

    enum class Stage : uint8
    {
        Reading,
        Counted,
        Decoded,
        Done
    };

    struct Use
    {
        uint64 Count = 0;
        uint64 Files = 0;
        std::size_t FirstEntry = NoEntry;
        std::string FirstPath;
        std::string FirstDetail;
        std::map<uint64, uint64> BitSizes;
    };

    using IssueKey = std::pair<DecodeIssueKind, uint32>;

    struct Tally
    {
        uint64 Files = 0;
        uint64 Decoded = 0;
        uint64 ReadErrors = 0;
        std::vector<std::pair<std::size_t, BindSweepFailure>> Failures;
        std::map<uint32, Use> Classes;
        std::map<IssueKey, Use> Issues;
    };

    void Count(Use& use, std::size_t entry, std::string_view path, std::string_view detail, bool newInFile)
    {
        ++use.Count;
        if (newInFile)
            ++use.Files;
        if (entry < use.FirstEntry)
        {
            use.FirstEntry = entry;
            use.FirstPath = path;
            use.FirstDetail = detail;
        }
    }

    void CountBits(Use& use, uint64 bits)
    {
        ++use.BitSizes[bits];
    }

    void Merge(Use& into, Use&& from)
    {
        into.Count += from.Count;
        into.Files += from.Files;
        if (from.FirstEntry < into.FirstEntry)
        {
            into.FirstEntry = from.FirstEntry;
            into.FirstPath = std::move(from.FirstPath);
            into.FirstDetail = std::move(from.FirstDetail);
        }
        for (auto const& [bits, count] : from.BitSizes)
            into.BitSizes[bits] += count;
    }

    void SweepEntry(KiwadArchive const& archive, TypeCatalogPtr const& catalog, SerializerLimits const& limits, std::size_t index, Tally& tally, Stage& stage)
    {
        KiwadEntry const& entry = archive.GetEntries()[index];
        KiwadReadResult const read = archive.Read(entry);
        if (!read.Succeeded())
        {
            ++tally.ReadErrors;
            stage = Stage::Done;
            return;
        }
        if (!BindFile::IsBind(read.Data))
        {
            stage = Stage::Done;
            return;
        }
        ++tally.Files;
        stage = Stage::Counted;
        BindReadResult const result = BindFile::Read(catalog, read.Data, limits);
        if (!result.Ok())
        {
            if (result.Decoded.Status == SerializerStatus::UnknownClass && result.RootClassHash != 0)
                Count(tally.Classes[result.RootClassHash], index, RootPath, result.Detail, true);
            tally.Failures.emplace_back(index, BindSweepFailure{ entry.Name, result.Status, result.Decoded.Status, result.RootClassHash, result.Detail });
            stage = Stage::Done;
            return;
        }
        ++tally.Decoded;
        stage = Stage::Decoded;
        std::set<IssueKey> seen;
        for (DecodeIssue const& issue : result.Decoded.Issues)
        {
            IssueKey const key{ issue.Kind, issue.Hash };
            bool const newInFile = seen.insert(key).second;
            if (issue.Kind == DecodeIssueKind::UnknownClass)
                Count(tally.Classes[issue.Hash], index, issue.Path, issue.Detail, newInFile);
            else
            {
                Count(tally.Issues[key], index, issue.Path, issue.Detail, newInFile);
                CountBits(tally.Issues[key], issue.Bits);
            }
        }
        stage = Stage::Done;
    }

    void RecordThrow(Tally& tally, std::string const& file, std::size_t index, Stage stage, bool outOfMemory) noexcept
    {
        if (stage == Stage::Reading)
        {
            ++tally.ReadErrors;
            return;
        }
        if (stage == Stage::Done)
            return;
        if (stage == Stage::Decoded)
            --tally.Decoded;
        try
        {
            tally.Failures.emplace_back(index, BindSweepFailure{ file, outOfMemory ? BindStatus::OutOfMemory : BindStatus::DecodeFailed,
                outOfMemory ? SerializerStatus::OutOfMemory : SerializerStatus::Ok, 0, outOfMemory ? "ran out of memory while being swept" : "threw while being swept" });
        }
        catch (...)
        {
            --tally.Files;
            ++tally.ReadErrors;
        }
    }
}

BindSweepReport BindSweep::Run(KiwadArchive const& archive, TypeCatalogPtr const& catalog, unsigned threads, SerializerLimits const& limits)
{
    std::vector<KiwadEntry> const& entries = archive.GetEntries();
    unsigned const hardware = std::max(std::thread::hardware_concurrency(), 1u);
    std::size_t const requested = std::min<std::size_t>(threads == 0 ? hardware : threads, MaxThreads);
    unsigned const workers = static_cast<unsigned>(std::clamp<std::size_t>(requested, 1, std::max<std::size_t>(entries.size(), 1)));
    std::atomic<std::size_t> next{ 0 };
    std::vector<Tally> tallies(workers);
    auto const work = [&archive, &catalog, &limits, &entries, &next](Tally& tally)
    {
        for (std::size_t index = next.fetch_add(1, std::memory_order_relaxed); index < entries.size(); index = next.fetch_add(1, std::memory_order_relaxed))
        {
            Stage stage = Stage::Reading;
            try
            {
                SweepEntry(archive, catalog, limits, index, tally, stage);
            }
            catch (std::bad_alloc const&)
            {
                RecordThrow(tally, entries[index].Name, index, stage, true);
            }
            catch (...)
            {
                RecordThrow(tally, entries[index].Name, index, stage, false);
            }
        }
    };
    std::vector<std::thread> pool;
    try
    {
        pool.reserve(workers - 1);
        for (unsigned worker = 1; worker < workers; ++worker)
            pool.emplace_back(work, std::ref(tallies[worker]));
    }
    catch (std::exception const&)
    {
    }
    work(tallies[0]);
    for (std::thread& thread : pool)
        thread.join();

    BindSweepReport report;
    report.Entries = entries.size();
    std::vector<std::pair<std::size_t, BindSweepFailure>> failures;
    std::map<uint32, Use> classes;
    std::map<IssueKey, Use> issues;
    for (Tally& tally : tallies)
    {
        report.Files += tally.Files;
        report.Decoded += tally.Decoded;
        report.ReadErrors += tally.ReadErrors;
        std::move(tally.Failures.begin(), tally.Failures.end(), std::back_inserter(failures));
        for (auto& [hash, use] : tally.Classes)
            Merge(classes[hash], std::move(use));
        for (auto& [key, use] : tally.Issues)
            Merge(issues[key], std::move(use));
    }
    std::stable_sort(failures.begin(), failures.end(), [](auto const& left, auto const& right) { return left.first < right.first; });
    for (auto& failure : failures)
        report.Failures.push_back(std::move(failure.second));
    for (auto& [hash, use] : classes)
        report.UnknownClasses.push_back(BindSweepUnknownClass{ hash, use.Count, use.Files, entries[use.FirstEntry].Name, std::move(use.FirstPath) });
    for (auto& [key, use] : issues)
        report.Issues.push_back(BindSweepIssue{ key.first, key.second, use.Count, use.Files, entries[use.FirstEntry].Name, std::move(use.FirstPath), std::move(use.FirstDetail), std::move(use.BitSizes) });
    std::sort(report.UnknownClasses.begin(), report.UnknownClasses.end(), [](BindSweepUnknownClass const& left, BindSweepUnknownClass const& right)
    {
        return left.Count != right.Count ? left.Count > right.Count : left.Hash < right.Hash;
    });
    std::stable_sort(report.Issues.begin(), report.Issues.end(), [](BindSweepIssue const& left, BindSweepIssue const& right) { return left.Count > right.Count; });
    return report;
}
