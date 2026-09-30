/*
 * Project Ambrose by Imjustchico
 * Sweeps every BINd file in a KIWAD archive on several threads, or only the entries it is given by name, and every entry that is a versionable object with no BINd header, such as a zone's gamedata.bin, and reports how many decode, every file that does not, the classes the type dump does not list with each property an object of one holds, by hash and size, and every other kind of issue, each grouped by hash and the class that owns it with how often and in how many files it appears and where it first does and the first distinct values a skipped property held, names every file an unknown class appears in, so a later sweep can be limited to them, and counts each file name's files, decodes, failures and root classes, which is how a kind of file every zone holds is told apart.
 */

#ifndef AMBROSE_BINDSWEEP_H
#define AMBROSE_BINDSWEEP_H

#include "BindFile.h"

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <utility>
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

using BindSweepValue = std::pair<uint64, std::vector<uint8>>;

struct BindSweepFileKind
{
    uint64 Files = 0;
    uint64 Decoded = 0;
    uint64 Failed = 0;
    std::map<uint32, uint64> Roots;
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
    uint32 Owner = 0;
    uint64 Count = 0;
    uint64 Files = 0;
    std::string FirstFile;
    std::string FirstPath;
    std::string FirstDetail;
    std::map<uint64, uint64> BitSizes;
    std::vector<BindSweepValue> Values;
    bool MoreValues = false;
};

struct BindSweepClassProperty
{
    uint32 Owner = 0;
    uint32 Hash = 0;
    uint64 Count = 0;
    uint64 Files = 0;
    std::string FirstFile;
    std::string FirstPath;
    std::map<uint64, uint64> BitSizes;
    std::vector<BindSweepValue> Values;
    bool MoreValues = false;
};

struct BindSweepReport
{
    uint64 Entries = 0;
    uint64 Files = 0;
    uint64 Decoded = 0;
    uint64 Headerless = 0;
    uint64 HeaderlessDecoded = 0;
    uint64 ReadErrors = 0;
    std::vector<BindSweepFailure> Failures;
    std::vector<BindSweepUnknownClass> UnknownClasses;
    std::vector<BindSweepClassProperty> ClassProperties;
    std::vector<BindSweepIssue> Issues;
    std::vector<std::string> UnknownFiles;
    std::map<std::string, BindSweepFileKind> FileKinds;
};

class BindSweep
{
public:
    static constexpr unsigned MaxThreads = 1024;
    static constexpr std::size_t MaxValues = 16;

    static std::string KindOf(std::string_view entry);

    BindSweep() = delete;

    static BindSweepReport Run(KiwadArchive const& archive, TypeCatalogPtr const& catalog, unsigned threads = 0, SerializerLimits const& limits = BindFile::GetDefaultLimits(),
        std::vector<std::string> const* only = nullptr);
};

#endif
