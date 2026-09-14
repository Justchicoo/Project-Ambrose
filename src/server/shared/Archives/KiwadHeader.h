/*
 * Project Ambrose by Imjustchico
 * KIWAD archive header and table of contents: bounds-checked parsing of entries and measurement of the TOC length.
 */

#ifndef AMBROSE_KIWADHEADER_H
#define AMBROSE_KIWADHEADER_H

#include "Types.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct KiwadEntry
{
    std::string Name;
    uint32 Offset = 0;
    uint32 Size = 0;
    uint32 CompressedSize = 0;
    bool Compressed = false;
    uint32 Crc = 0;

    uint32 GetStoredSize() const noexcept { return Compressed ? CompressedSize : Size; }
};

enum class KiwadError : uint8
{
    None,
    BadMagic,
    Truncated,
    TooManyEntries,
    NameTooLong,
    EntryOutOfBounds,
    BadCompressedSize
};

struct KiwadParseResult;

class KiwadHeader
{
public:
    static constexpr std::string_view Magic = "KIWAD";
    static constexpr std::size_t VersionOneHeaderSize = 13;
    static constexpr std::size_t VersionTwoHeaderSize = 14;
    static constexpr std::size_t EntryFixedSize = 21;
    static constexpr uint32 StoredMarker = 0xFFFFFFFFu;
    static constexpr uint32 MaxNameLength = 32768;

    static constexpr std::size_t MaxReservedEntries = 65536;

    using Reader = std::function<bool(std::span<uint8> destination)>;

    struct ParseOptions
    {
        bool CheckEntryBounds = true;
    };

    uint32 Version = 0;
    std::optional<uint8> Flags;
    std::vector<KiwadEntry> Entries;
    uint64 TocLength = 0;

    static KiwadParseResult Parse(Reader const& read, uint64 fileSize, ParseOptions options);
    static KiwadParseResult Parse(Reader const& read, uint64 fileSize);
    static KiwadParseResult Parse(std::span<uint8 const> data, ParseOptions options);
    static KiwadParseResult Parse(std::span<uint8 const> data);
    static std::optional<uint64> MeasureTocLength(std::span<uint8 const> data);
    static std::size_t GetHeaderSize(uint32 version) noexcept;
    static std::string_view GetErrorName(KiwadError error) noexcept;
};

struct KiwadParseResult
{
    KiwadError Error = KiwadError::None;
    std::string Message;
    std::optional<KiwadHeader> Header;

    bool Succeeded() const noexcept { return Error == KiwadError::None; }
};

#endif
