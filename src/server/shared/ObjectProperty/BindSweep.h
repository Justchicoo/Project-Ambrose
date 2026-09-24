/*
 * Project Ambrose by Imjustchico
 * Sweeps every BINd file in a KIWAD archive on several threads and reports how many decode, every file that does not, the classes the type dump does not list and every other kind of issue, each grouped by hash with how often and in how many files it appears and where it first does.
 */

#ifndef AMBROSE_BINDSWEEP_H
#define AMBROSE_BINDSWEEP_H

#include "BindFile.h"

#include <map>
#include <string>
#include <vector>

class KiwadArchive;

struct BindSweepFailure
{
    std::string File;
    BindStatus Status = BindStatus::Ok;
    SerializerStatus DecodeStatus = SerializerStatus::Ok;
    uint32 RootClassHash = 0;
    std::string Detail;
};

struct BindSweepUnknownClass
{
    uint32 Hash = 0;
    uint64 Count = 0;
    uint64 Files = 0;
    std::string FirstFile;
    std::string FirstPath;
};

struct BindSweepIssue
{
    DecodeIssueKind Kind = DecodeIssueKind::UnknownProperty;
    uint32 Hash = 0;
    uint64 Count = 0;
    uint64 Files = 0;
    std::string FirstFile;
    std::string FirstPath;
    std::string FirstDetail;
    std::map<uint64, uint64> BitSizes;
};

struct BindSweepReport
{
    uint64 Entries = 0;
    uint64 Files = 0;
    uint64 Decoded = 0;
    uint64 ReadErrors = 0;
    std::vector<BindSweepFailure> Failures;
    std::vector<BindSweepUnknownClass> UnknownClasses;
    std::vector<BindSweepIssue> Issues;
};

class BindSweep
{
public:
    static constexpr unsigned MaxThreads = 1024;

    BindSweep() = delete;

    static BindSweepReport Run(KiwadArchive const& archive, TypeCatalogPtr const& catalog, unsigned threads = 0, SerializerLimits const& limits = BindFile::GetDefaultLimits());
};

#endif
