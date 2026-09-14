/*
 * Project Ambrose by Imjustchico
 * Reads the KIWAD magic, version, entry count, optional flag byte, and 21-byte entry records without reading past the file.
 */

#include "KiwadHeader.h"

#include <fmt/format.h>

#include <algorithm>
#include <cstring>

namespace
{
    uint32 ReadUInt32(uint8 const* bytes) noexcept
    {
        return uint32{ bytes[0] } | (uint32{ bytes[1] } << 8) | (uint32{ bytes[2] } << 16) | (uint32{ bytes[3] } << 24);
    }

    KiwadParseResult Fail(KiwadError error, std::string message)
    {
        KiwadParseResult result;
        result.Error = error;
        result.Message = std::move(message);
        return result;
    }
}

std::size_t KiwadHeader::GetHeaderSize(uint32 version) noexcept
{
    return version >= 2 ? VersionTwoHeaderSize : VersionOneHeaderSize;
}

KiwadParseResult KiwadHeader::Parse(Reader const& read, uint64 fileSize)
{
    return Parse(read, fileSize, ParseOptions{});
}

KiwadParseResult KiwadHeader::Parse(Reader const& read, uint64 fileSize, ParseOptions options)
{
    KiwadHeader header;
    uint64 position = 0;
    auto const take = [&](std::span<uint8> destination) -> bool
    {
        if (position + destination.size() > fileSize || !read(destination))
            return false;
        position += destination.size();
        return true;
    };

    uint8 fixed[VersionOneHeaderSize];
    if (!take(fixed))
        return Fail(KiwadError::Truncated, fmt::format("the file is {} bytes, shorter than the {}-byte KIWAD header", fileSize, VersionOneHeaderSize));
    if (std::memcmp(fixed, Magic.data(), Magic.size()) != 0)
        return Fail(KiwadError::BadMagic, "the file does not start with KIWAD");
    header.Version = ReadUInt32(fixed + 5);
    uint32 const count = ReadUInt32(fixed + 9);
    if (header.Version >= 2)
    {
        uint8 flag = 0;
        if (!take(std::span<uint8>(&flag, 1)))
            return Fail(KiwadError::Truncated, "the file ends before the KIWAD flag byte");
        header.Flags = flag;
    }
    uint64 const remaining = fileSize - position;
    if (uint64{ count } * EntryFixedSize > remaining)
        return Fail(KiwadError::TooManyEntries, fmt::format("{} entries need at least {} bytes, but only {} remain", count, uint64{ count } * EntryFixedSize, remaining));

    header.Entries.reserve(std::min<std::size_t>(count, MaxReservedEntries));
    std::vector<uint8> name;
    for (uint32 index = 0; index < count; ++index)
    {
        uint8 record[EntryFixedSize];
        if (!take(record))
            return Fail(KiwadError::Truncated, fmt::format("the file ends inside entry {}", index));
        KiwadEntry entry;
        entry.Offset = ReadUInt32(record);
        entry.Size = ReadUInt32(record + 4);
        entry.CompressedSize = ReadUInt32(record + 8);
        entry.Compressed = record[12] != 0;
        entry.Crc = ReadUInt32(record + 13);
        uint32 const nameLength = ReadUInt32(record + 17);
        if (nameLength > MaxNameLength)
            return Fail(KiwadError::NameTooLong, fmt::format("entry {} has a {}-byte name, over the {}-byte limit", index, nameLength, MaxNameLength));
        if (nameLength > fileSize - position)
            return Fail(KiwadError::Truncated, fmt::format("the file ends inside the {}-byte name of entry {}", nameLength, index));
        name.resize(nameLength);
        if (!take(name))
            return Fail(KiwadError::Truncated, fmt::format("the file ends inside the name of entry {}", index));
        std::size_t length = name.size();
        while (length > 0 && name[length - 1] == 0)
            --length;
        entry.Name.assign(reinterpret_cast<char const*>(name.data()), length);
        if (entry.Compressed && entry.CompressedSize == StoredMarker)
            return Fail(KiwadError::BadCompressedSize, fmt::format("entry {} ({}) is marked compressed but has no compressed size", index, entry.Name));
        header.Entries.push_back(std::move(entry));
    }
    header.TocLength = position;

    for (std::size_t index = 0; options.CheckEntryBounds && index < header.Entries.size(); ++index)
    {
        KiwadEntry const& entry = header.Entries[index];
        if (uint64{ entry.Offset } + entry.GetStoredSize() > fileSize)
            return Fail(KiwadError::EntryOutOfBounds, fmt::format("entry {} ({}) spans bytes {} to {}, past the end of the {}-byte file", index, entry.Name, entry.Offset, uint64{ entry.Offset } + entry.GetStoredSize(), fileSize));
    }

    KiwadParseResult result;
    result.Header = std::move(header);
    return result;
}

KiwadParseResult KiwadHeader::Parse(std::span<uint8 const> data)
{
    return Parse(data, ParseOptions{});
}

KiwadParseResult KiwadHeader::Parse(std::span<uint8 const> data, ParseOptions options)
{
    std::size_t offset = 0;
    Reader const reader = [&](std::span<uint8> destination) -> bool
    {
        if (destination.size() > data.size() - offset)
            return false;
        std::copy_n(data.data() + offset, destination.size(), destination.data());
        offset += destination.size();
        return true;
    };
    return Parse(reader, data.size(), options);
}

std::optional<uint64> KiwadHeader::MeasureTocLength(std::span<uint8 const> data)
{
    KiwadParseResult const result = Parse(data, ParseOptions{ false });
    if (!result.Succeeded())
        return std::nullopt;
    return result.Header->TocLength;
}

std::string_view KiwadHeader::GetErrorName(KiwadError error) noexcept
{
    switch (error)
    {
        case KiwadError::None: return "none";
        case KiwadError::BadMagic: return "bad magic";
        case KiwadError::Truncated: return "truncated";
        case KiwadError::TooManyEntries: return "too many entries";
        case KiwadError::NameTooLong: return "name too long";
        case KiwadError::EntryOutOfBounds: return "entry out of bounds";
        case KiwadError::BadCompressedSize: return "bad compressed size";
    }
    return "unknown";
}
