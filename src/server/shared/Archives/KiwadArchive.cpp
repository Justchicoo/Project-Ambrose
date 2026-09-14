/*
 * Project Ambrose by Imjustchico
 * Opens a KIWAD file, indexes entries by exact and case-folded path, and reads, inflates, and checksums entry data under a lock.
 */

#include "KiwadArchive.h"
#include "Compression.h"
#include "ConfigMgr.h"
#include "Crc32.h"

#include <fmt/format.h>

#include <exception>
#include <system_error>

KiwadArchive::KiwadArchive(std::filesystem::path path, uint64 fileSize, std::ifstream stream, KiwadHeader header)
    : _path(std::move(path)), _fileSize(fileSize), _stream(std::move(stream)), _header(std::move(header))
{
    for (std::size_t index = 0; index < _header.Entries.size(); ++index)
    {
        std::string const exact = NormalizeSlashes(_header.Entries[index].Name);
        if (!_exact.emplace(exact, index).second)
        {
            ++_duplicates;
            continue;
        }
        std::string folded = NormalizeName(exact);
        if (_folded.find(folded) != _folded.end())
            ++_caseCollisions;
        _folded.emplace(std::move(folded), index);
    }
}

std::unique_ptr<KiwadArchive> KiwadArchive::Open(std::filesystem::path const& path, std::string& error)
{
    try
    {
        std::error_code sizeError;
        uintmax_t const fileSize = std::filesystem::file_size(path, sizeError);
        if (sizeError)
        {
            error = fmt::format("{}: {}", ConfigMgr::PathToUtf8(path), sizeError.message());
            return nullptr;
        }
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            error = fmt::format("{}: cannot open the file", ConfigMgr::PathToUtf8(path));
            return nullptr;
        }
        KiwadHeader::Reader const reader = [&stream](std::span<uint8> destination) -> bool
        {
            stream.read(reinterpret_cast<char*>(destination.data()), static_cast<std::streamsize>(destination.size()));
            return static_cast<std::size_t>(stream.gcount()) == destination.size();
        };
        KiwadParseResult parsed = KiwadHeader::Parse(reader, fileSize);
        if (!parsed.Succeeded())
        {
            error = fmt::format("{}: {}", ConfigMgr::PathToUtf8(path), parsed.Message);
            return nullptr;
        }
        stream.clear();
        return std::unique_ptr<KiwadArchive>(new KiwadArchive(path, fileSize, std::move(stream), std::move(*parsed.Header)));
    }
    catch (std::exception const& exception)
    {
        error = fmt::format("{}: {}", ConfigMgr::PathToUtf8(path), exception.what());
        return nullptr;
    }
}

std::filesystem::path const& KiwadArchive::GetPath() const noexcept
{
    return _path;
}

uint64 KiwadArchive::GetFileSize() const noexcept
{
    return _fileSize;
}

KiwadHeader const& KiwadArchive::GetHeader() const noexcept
{
    return _header;
}

std::vector<KiwadEntry> const& KiwadArchive::GetEntries() const noexcept
{
    return _header.Entries;
}

std::size_t KiwadArchive::GetDuplicateNameCount() const noexcept
{
    return _duplicates;
}

std::size_t KiwadArchive::GetCaseCollisionCount() const noexcept
{
    return _caseCollisions;
}

std::string KiwadArchive::NormalizeSlashes(std::string_view name)
{
    std::string normalized(name);
    for (char& c : normalized)
        if (c == '\\')
            c = '/';
    return normalized;
}

std::string KiwadArchive::NormalizeName(std::string_view name)
{
    std::string normalized = NormalizeSlashes(name);
    for (char& c : normalized)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return normalized;
}

uint64 KiwadArchive::GetMaxCompressedSize(uint32 size) noexcept
{
    uint64 const n = size;
    return n + (n >> 12) + (n >> 14) + (n >> 25) + 64;
}

KiwadEntry const* KiwadArchive::Find(std::string_view name) const
{
    auto const exact = _exact.find(NormalizeSlashes(name));
    if (exact != _exact.end())
        return &_header.Entries[exact->second];
    auto const [first, last] = _folded.equal_range(NormalizeName(name));
    if (first == last || std::next(first) != last)
        return nullptr;
    return &_header.Entries[first->second];
}

std::vector<KiwadEntry const*> KiwadArchive::FindAll(std::string_view name) const
{
    std::vector<KiwadEntry const*> entries;
    auto const [first, last] = _folded.equal_range(NormalizeName(name));
    for (auto it = first; it != last; ++it)
        entries.push_back(&_header.Entries[it->second]);
    return entries;
}

KiwadReadResult KiwadArchive::ReadStored(KiwadEntry const& entry) const
{
    KiwadReadResult result;
    uint32 const storedSize = entry.GetStoredSize();
    if (uint64{ entry.Offset } + storedSize > _fileSize)
    {
        result.Error = fmt::format("{}: entry {} lies past the end of the file", ConfigMgr::PathToUtf8(_path), entry.Name);
        return result;
    }
    try
    {
        result.Data.resize(storedSize);
    }
    catch (std::exception const& exception)
    {
        result.Error = fmt::format("{}: cannot allocate {} bytes for {}: {}", ConfigMgr::PathToUtf8(_path), storedSize, entry.Name, exception.what());
        return result;
    }
    std::lock_guard lock(_mutex);
    _stream.clear();
    _stream.seekg(static_cast<std::streamoff>(entry.Offset));
    _stream.read(reinterpret_cast<char*>(result.Data.data()), static_cast<std::streamsize>(storedSize));
    if (static_cast<std::size_t>(_stream.gcount()) != storedSize)
    {
        result.Data.clear();
        result.Error = fmt::format("{}: could not read {} bytes of {}", ConfigMgr::PathToUtf8(_path), storedSize, entry.Name);
    }
    return result;
}

KiwadReadResult KiwadArchive::Read(KiwadEntry const& entry, std::size_t maxSize) const
{
    KiwadReadResult result;
    if (entry.Size > maxSize)
    {
        result.Error = fmt::format("{}: {} is {} bytes, over the {}-byte limit", ConfigMgr::PathToUtf8(_path), entry.Name, entry.Size, maxSize);
        return result;
    }
    if (entry.Compressed && entry.CompressedSize > GetMaxCompressedSize(entry.Size))
    {
        result.Error = fmt::format("{}: {} claims {} compressed bytes for {} bytes of data, more than deflate can produce", ConfigMgr::PathToUtf8(_path), entry.Name, entry.CompressedSize, entry.Size);
        return result;
    }
    KiwadReadResult stored = ReadStored(entry);
    if (!stored.Succeeded() || !entry.Compressed)
        return stored;
    Ambrose::Compression::InflateResult inflated = Ambrose::Compression::InflateExact(stored.Data, entry.Size);
    if (!inflated.Succeeded())
        result.Error = fmt::format("{}: cannot inflate {}: {}", ConfigMgr::PathToUtf8(_path), entry.Name, Ambrose::Compression::GetStatusName(inflated.Code));
    else
        result.Data = std::move(inflated.Data);
    return result;
}

KiwadReadResult KiwadArchive::Read(std::string_view name, std::size_t maxSize) const
{
    KiwadEntry const* const entry = Find(name);
    if (!entry)
    {
        KiwadReadResult result;
        std::size_t const matches = FindAll(name).size();
        result.Error = matches > 1
            ? fmt::format("{}: {} matches {} entries that differ only by letter case; use the exact name", ConfigMgr::PathToUtf8(_path), name, matches)
            : fmt::format("{}: no entry named {}", ConfigMgr::PathToUtf8(_path), name);
        return result;
    }
    return Read(*entry, maxSize);
}

bool KiwadArchive::VerifyCrc(KiwadEntry const& entry) const
{
    KiwadReadResult const stored = ReadStored(entry);
    return stored.Succeeded() && Crc32::ComputeClient(stored.Data) == entry.Crc;
}
