/*
 * Project Ambrose by Imjustchico
 * Read-only KIWAD archive on disk: exact and case-insensitive entry lookup, stored and inflated reads, and CRC checks.
 */

#ifndef AMBROSE_KIWADARCHIVE_H
#define AMBROSE_KIWADARCHIVE_H

#include "KiwadHeader.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

struct KiwadReadResult
{
    std::string Error;
    std::vector<uint8> Data;

    bool Succeeded() const noexcept { return Error.empty(); }
};

class KiwadArchive
{
public:
    static constexpr std::size_t DefaultMaxEntrySize = std::size_t{ 1 } << 30;

    static std::unique_ptr<KiwadArchive> Open(std::filesystem::path const& path, std::string& error);

    KiwadArchive(KiwadArchive const&) = delete;
    KiwadArchive& operator=(KiwadArchive const&) = delete;

    std::filesystem::path const& GetPath() const noexcept;
    uint64 GetFileSize() const noexcept;
    KiwadHeader const& GetHeader() const noexcept;
    std::vector<KiwadEntry> const& GetEntries() const noexcept;
    KiwadEntry const* Find(std::string_view name) const;
    std::vector<KiwadEntry const*> FindAll(std::string_view name) const;
    std::size_t GetDuplicateNameCount() const noexcept;
    std::size_t GetCaseCollisionCount() const noexcept;

    KiwadReadResult ReadStored(KiwadEntry const& entry) const;
    KiwadReadResult Read(KiwadEntry const& entry, std::size_t maxSize = DefaultMaxEntrySize) const;
    KiwadReadResult Read(std::string_view name, std::size_t maxSize = DefaultMaxEntrySize) const;
    bool VerifyCrc(KiwadEntry const& entry) const;

    static std::string NormalizeSlashes(std::string_view name);
    static std::string NormalizeName(std::string_view name);
    static uint64 GetMaxCompressedSize(uint32 size) noexcept;

private:
    KiwadArchive(std::filesystem::path path, uint64 fileSize, std::ifstream stream, KiwadHeader header);

    std::filesystem::path _path;
    uint64 _fileSize;
    mutable std::mutex _mutex;
    mutable std::ifstream _stream;
    KiwadHeader _header;
    std::map<std::string, std::size_t, std::less<>> _exact;
    std::multimap<std::string, std::size_t, std::less<>> _folded;
    std::size_t _duplicates = 0;
    std::size_t _caseCollisions = 0;
};

#endif
