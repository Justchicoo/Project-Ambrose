/*
 * Project Ambrose by Imjustchico
 * Parses PE32+ headers, sections, exports, imports, base relocations, TLS and the exception table with every read bounds-checked, refuses import directories that list more imports or name bytes than the file can back or whose address tables overlap, maps RVAs to file offsets, follows chained unwind info to a function's primary start, and finds NUL-terminated strings in section data.
 */

#include "PeImage.h"
#include "ConfigMgr.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <system_error>

namespace
{
    constexpr std::size_t DosHeaderSize = 0x40;
    constexpr std::size_t NtHeaderOffsetField = 0x3C;
    constexpr std::size_t SignatureSize = 4;
    constexpr std::size_t FileHeaderSize = 20;
    constexpr std::size_t OptionalHeader64FixedSize = 112;
    constexpr std::size_t DataDirectoryEntrySize = 8;
    constexpr std::size_t MaxDataDirectories = 16;
    constexpr std::size_t SectionHeaderSize = 40;
    constexpr std::size_t SectionNameSize = 8;
    constexpr uint16 OptionalHeader64Magic = 0x20B;
    constexpr uint16 FileCharacteristicDll = 0x2000;

    constexpr std::size_t ExportDirectoryIndex = 0;
    constexpr std::size_t ImportDirectoryIndex = 1;
    constexpr std::size_t ExceptionDirectoryIndex = 3;
    constexpr std::size_t RelocationDirectoryIndex = 5;
    constexpr std::size_t TlsDirectoryIndex = 9;

    constexpr uint32 ExportDirectorySize = 40;
    constexpr uint32 ImportDescriptorSize = 20;
    constexpr uint32 ThunkSize = 8;
    constexpr uint32 TlsDirectorySize = 40;
    constexpr uint32 RuntimeFunctionSize = 12;
    constexpr uint32 RelocationBlockHeaderSize = 8;
    constexpr uint32 UnwindInfoHeaderSize = 4;
    constexpr uint8 UnwindFlagChainInfo = 0x4;
    constexpr uint32 MaxChainHops = 32;
    constexpr uint64 ImportByOrdinalFlag = 0x8000000000000000ull;
    constexpr uint64 ImportOrdinalReservedBits = 0x7FFFFFFFFFFF0000ull;
    constexpr uint64 ImportNameReservedBits = 0x7FFFFFFF80000000ull;
    constexpr std::size_t MaxNameLength = 4096;
    constexpr uint64 MaxRva = std::numeric_limits<uint32>::max();

    uint16 Get16(uint8 const* data)
    {
        return static_cast<uint16>(data[0] | (data[1] << 8));
    }

    uint32 Get32(uint8 const* data)
    {
        return uint32{ data[0] } | (uint32{ data[1] } << 8) | (uint32{ data[2] } << 16) | (uint32{ data[3] } << 24);
    }

    uint64 Get64(uint8 const* data)
    {
        return uint64{ Get32(data) } | (uint64{ Get32(data + 4) } << 32);
    }

    uint64 SectionExtent(PeSection const& section)
    {
        return std::max(section.VirtualSize, section.RawSize);
    }

    std::span<uint8 const> FileBackedFrom(PeImage const& image, uint32 rva)
    {
        std::span<uint8 const> const bytes = image.GetBytes();
        if (rva < image.GetSizeOfHeaders())
        {
            uint64 const headerEnd = std::min<uint64>(image.GetSizeOfHeaders(), bytes.size());
            if (rva >= headerEnd)
                return {};
            return bytes.subspan(rva, static_cast<std::size_t>(headerEnd - rva));
        }
        for (PeSection const& section : image.GetSections())
        {
            if (rva < section.VirtualAddress || rva - section.VirtualAddress >= SectionExtent(section))
                continue;
            uint32 const into = rva - section.VirtualAddress;
            if (into >= section.RawSize)
                continue;
            return bytes.subspan(std::size_t{ section.RawOffset } + into, section.RawSize - into);
        }
        return {};
    }
}

struct PeImageParser
{
public:
    PeImageParser(PeImage& image, std::string& error) : _image(image), _error(error)
    {
    }

    bool Parse()
    {
        return ParseHeaders() && ParseExports() && ParseImports() && ParseRelocations() && ParseTls() && ParseExceptions();
    }

private:
    struct DataDirectory
    {
        uint32 Rva = 0;
        uint32 Size = 0;

        bool IsPresent() const noexcept { return Rva != 0 && Size != 0; }
    };

    bool Fail(std::string message)
    {
        _error = std::move(message);
        return false;
    }

    std::span<uint8 const> ReadArray(uint32 rva, uint32 count, uint32 width) const
    {
        uint64 const size = uint64{ count } * width;
        if (size > MaxRva)
            return {};
        return _image.ReadRva(rva, static_cast<uint32>(size));
    }

    std::optional<std::string> ReadString(uint32 rva) const
    {
        std::span<uint8 const> const available = FileBackedFrom(_image, rva);
        std::size_t const limit = std::min(available.size(), MaxNameLength + 1);
        if (limit == 0)
            return std::nullopt;
        void const* const terminator = std::memchr(available.data(), 0, limit);
        if (terminator == nullptr)
            return std::nullopt;
        std::size_t const length = static_cast<std::size_t>(static_cast<uint8 const*>(terminator) - available.data());
        return std::string(reinterpret_cast<char const*>(available.data()), length);
    }

    bool ParseHeaders()
    {
        std::span<uint8 const> const bytes = _image._bytes;
        if (bytes.size() < DosHeaderSize)
            return Fail(fmt::format("the file is {} bytes, too short for a DOS header", bytes.size()));
        if (bytes[0] != 'M' || bytes[1] != 'Z')
            return Fail("the file does not start with a DOS header (MZ)");
        uint32 const ntOffset = Get32(bytes.data() + NtHeaderOffsetField);
        uint64 const fileHeaderOffset = uint64{ ntOffset } + SignatureSize;
        if (fileHeaderOffset + FileHeaderSize > bytes.size())
            return Fail(fmt::format("the PE header offset {:#x} leaves no room for a PE signature and file header in the {}-byte file", ntOffset, bytes.size()));
        if (std::memcmp(bytes.data() + ntOffset, "PE\0\0", SignatureSize) != 0)
            return Fail(fmt::format("there is no PE signature at offset {:#x}", ntOffset));

        uint8 const* const file = bytes.data() + fileHeaderOffset;
        _image._machine = Get16(file);
        uint16 const sectionCount = Get16(file + 2);
        _image._timeDateStamp = Get32(file + 4);
        uint16 const optionalSize = Get16(file + 16);
        _image._characteristics = Get16(file + 18);

        uint64 const optionalOffset = fileHeaderOffset + FileHeaderSize;
        if (optionalSize < 2 || optionalOffset + 2 > bytes.size())
            return Fail(fmt::format("the optional header at offset {:#x} is cut off", optionalOffset));
        uint16 const magic = Get16(bytes.data() + optionalOffset);
        if (magic != OptionalHeader64Magic)
            return Fail(fmt::format("not a PE32+ image: the optional header magic is {:#x}", magic));
        if (optionalSize < OptionalHeader64FixedSize)
            return Fail(fmt::format("not a PE32+ image: the optional header is {} bytes, too short for PE32+", optionalSize));
        if (optionalOffset + optionalSize > bytes.size())
            return Fail(fmt::format("the {}-byte optional header at offset {:#x} runs past the end of the {}-byte file", optionalSize, optionalOffset, bytes.size()));

        uint8 const* const optional = bytes.data() + optionalOffset;
        _image._entryPointRva = Get32(optional + 16);
        _image._imageBase = Get64(optional + 24);
        _image._sizeOfImage = Get32(optional + 56);
        _image._sizeOfHeaders = Get32(optional + 60);
        uint32 const rvaAndSizes = Get32(optional + 108);
        std::size_t const directoryCount = std::min<std::size_t>(rvaAndSizes, MaxDataDirectories);
        if (OptionalHeader64FixedSize + directoryCount * DataDirectoryEntrySize > optionalSize)
            return Fail(fmt::format("the optional header lists {} data directories but has room for {}", rvaAndSizes, (optionalSize - OptionalHeader64FixedSize) / DataDirectoryEntrySize));
        for (std::size_t index = 0; index < directoryCount; ++index)
        {
            uint8 const* const entry = optional + OptionalHeader64FixedSize + index * DataDirectoryEntrySize;
            _directories[index] = { Get32(entry), Get32(entry + 4) };
        }

        uint64 const sectionTable = optionalOffset + optionalSize;
        if (sectionTable + uint64{ sectionCount } * SectionHeaderSize > bytes.size())
            return Fail(fmt::format("the table of {} sections at offset {:#x} runs past the end of the {}-byte file", sectionCount, sectionTable, bytes.size()));
        _image._sections.reserve(sectionCount);
        for (std::size_t index = 0; index < sectionCount; ++index)
        {
            uint8 const* const header = bytes.data() + sectionTable + index * SectionHeaderSize;
            PeSection section;
            std::size_t nameLength = 0;
            while (nameLength < SectionNameSize && header[nameLength] != 0)
                ++nameLength;
            section.Name.assign(reinterpret_cast<char const*>(header), nameLength);
            section.VirtualSize = Get32(header + 8);
            section.VirtualAddress = Get32(header + 12);
            section.RawSize = Get32(header + 16);
            section.RawOffset = Get32(header + 20);
            section.Characteristics = Get32(header + 36);
            if (section.RawSize != 0 && uint64{ section.RawOffset } + section.RawSize > bytes.size())
                return Fail(fmt::format("section {} raw data at offset {:#x} with {} bytes runs past the end of the {}-byte file", section.Name, section.RawOffset, section.RawSize, bytes.size()));
            _image._sections.push_back(std::move(section));
        }
        return true;
    }

    bool ParseExports()
    {
        DataDirectory const& directory = _directories[ExportDirectoryIndex];
        if (!directory.IsPresent())
            return true;
        std::span<uint8 const> const header = _image.ReadRva(directory.Rva, ExportDirectorySize);
        if (header.size() != ExportDirectorySize)
            return Fail(fmt::format("the export directory at RVA {:#x} is not inside the file", directory.Rva));
        uint32 const ordinalBase = Get32(header.data() + 16);
        uint32 const functionCount = Get32(header.data() + 20);
        uint32 const nameCount = Get32(header.data() + 24);
        uint32 const functionsRva = Get32(header.data() + 28);
        uint32 const namesRva = Get32(header.data() + 32);
        uint32 const ordinalsRva = Get32(header.data() + 36);

        std::span<uint8 const> functions;
        if (functionCount != 0)
        {
            functions = ReadArray(functionsRva, functionCount, 4);
            if (functions.empty())
                return Fail(fmt::format("the export directory's table of {} functions at RVA {:#x} is not inside the file", functionCount, functionsRva));
        }
        std::vector<std::pair<uint32, std::string>> named;
        if (nameCount != 0)
        {
            std::span<uint8 const> const names = ReadArray(namesRva, nameCount, 4);
            if (names.empty())
                return Fail(fmt::format("the export directory's table of {} names at RVA {:#x} is not inside the file", nameCount, namesRva));
            std::span<uint8 const> const ordinals = ReadArray(ordinalsRva, nameCount, 2);
            if (ordinals.empty())
                return Fail(fmt::format("the export directory's table of {} name ordinals at RVA {:#x} is not inside the file", nameCount, ordinalsRva));
            named.reserve(nameCount);
            for (std::size_t index = 0; index < nameCount; ++index)
            {
                uint16 const functionIndex = Get16(ordinals.data() + index * 2);
                if (functionIndex >= functionCount)
                    return Fail(fmt::format("the export directory's name {} refers to function {} of {}", index, functionIndex, functionCount));
                uint32 const nameRva = Get32(names.data() + index * 4);
                std::optional<std::string> name = ReadString(nameRva);
                if (!name)
                    return Fail(fmt::format("the export directory's name {} at RVA {:#x} is not a terminated string inside the file", index, nameRva));
                named.emplace_back(functionIndex, std::move(*name));
            }
            std::stable_sort(named.begin(), named.end(), [](auto const& a, auto const& b) { return a.first < b.first; });
        }

        std::size_t cursor = 0;
        for (uint32 index = 0; index < functionCount; ++index)
        {
            uint32 const rva = Get32(functions.data() + std::size_t{ index } * 4);
            std::size_t const firstName = cursor;
            while (cursor < named.size() && named[cursor].first == index)
                ++cursor;
            if (rva == 0 && firstName == cursor)
                continue;
            uint64 const ordinal = uint64{ ordinalBase } + index;
            if (ordinal > std::numeric_limits<uint16>::max())
                return Fail(fmt::format("the export directory's ordinal {} does not fit in 16 bits", ordinal));
            PeExport entry;
            entry.Ordinal = static_cast<uint16>(ordinal);
            if (rva >= directory.Rva && rva - directory.Rva < directory.Size)
            {
                std::optional<std::string> forwarder = ReadString(rva);
                if (!forwarder || forwarder->find('.') == std::string::npos)
                    return Fail(fmt::format("the export directory's ordinal {} forwards through RVA {:#x}, which holds no dll.name text", ordinal, rva));
                entry.Forwarder = std::move(*forwarder);
            }
            else
            {
                if (rva >= _image._sizeOfImage)
                    return Fail(fmt::format("the export directory's ordinal {} points at RVA {:#x}, outside the {:#x}-byte image", ordinal, rva, _image._sizeOfImage));
                entry.Rva = rva;
            }
            if (firstName == cursor)
            {
                _image._exports.push_back(std::move(entry));
                continue;
            }
            for (std::size_t name = firstName; name < cursor; ++name)
            {
                PeExport copy = entry;
                copy.Name = named[name].second;
                _image._exports.push_back(std::move(copy));
            }
        }
        std::sort(_image._exports.begin(), _image._exports.end(), [](PeExport const& a, PeExport const& b) { return a.Name != b.Name ? a.Name < b.Name : a.Ordinal < b.Ordinal; });
        return true;
    }

    bool ParseImports()
    {
        DataDirectory const& directory = _directories[ImportDirectoryIndex];
        if (!directory.IsPresent())
            return true;
        struct AddressTable
        {
            uint64 End = 0;
            std::size_t FirstImport = 0;
        };
        std::map<uint32, AddressTable> addressTables;
        uint64 const maxImports = _image._bytes.size() / ThunkSize;
        uint64 nameBudget = _image._bytes.size();
        auto const chargeNames = [this, &nameBudget](std::string const& dll, uint64 cost)
        {
            if (cost > nameBudget)
                return Fail(fmt::format("the import directory's names for {} repeat past the size of the {}-byte file", dll, _image._bytes.size()));
            nameBudget -= cost;
            return true;
        };
        for (uint64 at = directory.Rva;; at += ImportDescriptorSize)
        {
            std::span<uint8 const> const descriptor = at + ImportDescriptorSize <= MaxRva ? _image.ReadRva(static_cast<uint32>(at), ImportDescriptorSize) : std::span<uint8 const>{};
            if (descriptor.size() != ImportDescriptorSize)
                return Fail(fmt::format("the import directory at RVA {:#x} runs out of the file at RVA {:#x} before its terminating entry", directory.Rva, at));
            if (std::all_of(descriptor.begin(), descriptor.end(), [](uint8 byte) { return byte == 0; }))
                return true;
            uint32 const originalFirstThunk = Get32(descriptor.data());
            uint32 const nameRva = Get32(descriptor.data() + 12);
            uint32 const firstThunk = Get32(descriptor.data() + 16);
            std::optional<std::string> dll = ReadString(nameRva);
            if (!dll)
                return Fail(fmt::format("the import directory's entry at RVA {:#x} names its DLL at RVA {:#x}, which is not a terminated string inside the file", at, nameRva));
            if (!chargeNames(*dll, uint64{ dll->size() } + 1))
                return false;
            if (firstThunk == 0)
                return Fail(fmt::format("the import directory's entry for {} has no import address table", *dll));
            uint32 const thunkRva = originalFirstThunk != 0 ? originalFirstThunk : firstThunk;
            auto const following = addressTables.upper_bound(firstThunk);
            auto const preceding = following == addressTables.begin() ? addressTables.end() : std::prev(following);
            std::size_t const firstImport = _image._imports.size();
            for (uint64 index = 0;; ++index)
            {
                uint64 const thunkAt = thunkRva + index * ThunkSize;
                uint64 const slot = firstThunk + index * ThunkSize;
                std::span<uint8 const> const thunkBytes = thunkAt + ThunkSize <= MaxRva ? _image.ReadRva(static_cast<uint32>(thunkAt), ThunkSize) : std::span<uint8 const>{};
                if (thunkBytes.size() != ThunkSize)
                    return Fail(fmt::format("the import directory's thunks for {} run out of the file at RVA {:#x}", *dll, thunkAt));
                uint64 const thunk = Get64(thunkBytes.data());
                if (thunk == 0)
                    break;
                if (slot + ThunkSize > _image._sizeOfImage)
                    return Fail(fmt::format("the import directory's slot {} for {} at RVA {:#x} lies outside the {:#x}-byte image", index, *dll, slot, _image._sizeOfImage));
                if (_image._imports.size() >= maxImports)
                    return Fail(fmt::format("the import directory lists more than {} imports, more than the {}-byte file can hold", maxImports, _image._bytes.size()));
                auto overlapped = addressTables.end();
                if (preceding != addressTables.end() && preceding->second.End > slot)
                    overlapped = preceding;
                else if (following != addressTables.end() && slot + ThunkSize > following->first)
                    overlapped = following;
                if (overlapped != addressTables.end())
                    return Fail(fmt::format("the import directory's slot {} for {} at RVA {:#x} overlaps the {}-slot import address table for {} at RVA {:#x}", index, *dll, slot, (overlapped->second.End - overlapped->first) / ThunkSize, _image._imports[overlapped->second.FirstImport].Dll, overlapped->first));
                PeImport entry;
                entry.SlotRva = static_cast<uint32>(slot);
                if ((thunk & ImportByOrdinalFlag) != 0)
                {
                    if ((thunk & ImportOrdinalReservedBits) != 0)
                        return Fail(fmt::format("the import directory's thunk {} for {} is a malformed ordinal {:#x}", index, *dll, thunk));
                    entry.Ordinal = static_cast<uint16>(thunk);
                }
                else
                {
                    if ((thunk & ImportNameReservedBits) != 0)
                        return Fail(fmt::format("the import directory's thunk {} for {} is a malformed name reference {:#x}", index, *dll, thunk));
                    uint64 const nameAt = thunk + 2;
                    std::optional<std::string> name = nameAt <= MaxRva ? ReadString(static_cast<uint32>(nameAt)) : std::nullopt;
                    if (!name)
                        return Fail(fmt::format("the import directory's thunk {} for {} names a function at RVA {:#x}, which is not a terminated string inside the file", index, *dll, thunk));
                    entry.Name = std::move(*name);
                }
                if (!chargeNames(*dll, uint64{ dll->size() } + 1 + (entry.Ordinal ? 0 : uint64{ entry.Name.size() } + 3)))
                    return false;
                entry.Dll = *dll;
                _image._imports.push_back(std::move(entry));
            }
            std::size_t const count = _image._imports.size() - firstImport;
            if (count != 0)
                addressTables.emplace(firstThunk, AddressTable{ uint64{ firstThunk } + count * ThunkSize, firstImport });
        }
    }

    bool ParseRelocations()
    {
        DataDirectory const& directory = _directories[RelocationDirectoryIndex];
        if (!directory.IsPresent())
            return true;
        std::span<uint8 const> const data = _image.ReadRva(directory.Rva, directory.Size);
        if (data.size() != directory.Size)
            return Fail(fmt::format("the base relocation directory at RVA {:#x} with {} bytes is not inside the file", directory.Rva, directory.Size));
        _image._relocations.reserve(data.size() / 2);
        std::size_t at = 0;
        while (at < data.size())
        {
            if (data.size() - at < RelocationBlockHeaderSize)
                return Fail(fmt::format("the base relocation directory ends with {} stray bytes at offset {:#x}", data.size() - at, at));
            uint32 const page = Get32(data.data() + at);
            uint32 const blockSize = Get32(data.data() + at + 4);
            if (blockSize < RelocationBlockHeaderSize || blockSize % 2 != 0 || blockSize > data.size() - at)
                return Fail(fmt::format("the base relocation block at offset {:#x} for page {:#x} has a bad size of {} bytes", at, page, blockSize));
            for (std::size_t entryAt = at + RelocationBlockHeaderSize; entryAt < at + blockSize; entryAt += 2)
            {
                uint16 const entry = Get16(data.data() + entryAt);
                uint8 const type = static_cast<uint8>(entry >> 12);
                if (type == PeImage::RelocationAbsolute)
                    continue;
                uint64 const rva = uint64{ page } + (entry & 0xFFF);
                uint64 const width = type == PeImage::RelocationDir64 ? 8 : type == PeImage::RelocationHighLow ? 4 : 2;
                if (rva + width > _image._sizeOfImage)
                    return Fail(fmt::format("the base relocation of type {} at RVA {:#x} lies outside the {:#x}-byte image", type, rva, _image._sizeOfImage));
                _image._relocations.push_back({ static_cast<uint32>(rva), type });
            }
            at += blockSize;
        }
        return true;
    }

    bool ParseTls()
    {
        DataDirectory const& directory = _directories[TlsDirectoryIndex];
        if (!directory.IsPresent())
            return true;
        std::span<uint8 const> const data = _image.ReadRva(directory.Rva, TlsDirectorySize);
        if (data.size() != TlsDirectorySize)
            return Fail(fmt::format("the TLS directory at RVA {:#x} is not inside the file", directory.Rva));
        PeTls tls;
        tls.StartAddressOfRawData = Get64(data.data());
        tls.EndAddressOfRawData = Get64(data.data() + 8);
        tls.AddressOfIndex = Get64(data.data() + 16);
        tls.AddressOfCallBacks = Get64(data.data() + 24);
        tls.SizeOfZeroFill = Get32(data.data() + 32);
        _image._tls = tls;
        return true;
    }

    bool ParseExceptions()
    {
        DataDirectory const& directory = _directories[ExceptionDirectoryIndex];
        if (!directory.IsPresent())
            return true;
        if (directory.Size % RuntimeFunctionSize != 0)
            return Fail(fmt::format("the exception table is {} bytes, not a whole number of {}-byte entries", directory.Size, RuntimeFunctionSize));
        std::span<uint8 const> const data = _image.ReadRva(directory.Rva, directory.Size);
        if (data.size() != directory.Size)
            return Fail(fmt::format("the exception table at RVA {:#x} with {} bytes is not inside the file", directory.Rva, directory.Size));
        std::size_t const count = data.size() / RuntimeFunctionSize;
        _image._functions.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            uint8 const* const entry = data.data() + index * RuntimeFunctionSize;
            PeFunction function{ Get32(entry), Get32(entry + 4), Get32(entry + 8) };
            if (function.Begin >= function.End || function.End > _image._sizeOfImage || function.UnwindRva >= _image._sizeOfImage)
                return Fail(fmt::format("the exception table's entry {} ({:#x}-{:#x}, unwind info {:#x}) lies outside the {:#x}-byte image", index, function.Begin, function.End, function.UnwindRva, _image._sizeOfImage));
            _image._functions.push_back(function);
        }
        auto const byBegin = [](PeFunction const& a, PeFunction const& b) { return a.Begin < b.Begin; };
        if (!std::is_sorted(_image._functions.begin(), _image._functions.end(), byBegin))
            std::stable_sort(_image._functions.begin(), _image._functions.end(), byBegin);
        return true;
    }

    PeImage& _image;
    std::string& _error;
    std::array<DataDirectory, MaxDataDirectories> _directories{};
};

PeImage::PeImage(std::vector<uint8> bytes) : _bytes(std::move(bytes))
{
}

std::unique_ptr<PeImage> PeImage::Parse(std::vector<uint8> bytes, std::string& error)
{
    try
    {
        std::unique_ptr<PeImage> image(new PeImage(std::move(bytes)));
        PeImageParser parser(*image, error);
        if (!parser.Parse())
            return nullptr;
        return image;
    }
    catch (std::exception const& exception)
    {
        error = fmt::format("cannot parse the PE image: {}", exception.what());
        return nullptr;
    }
}

std::unique_ptr<PeImage> PeImage::Load(std::filesystem::path const& path, std::string& error)
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
        if (fileSize > std::numeric_limits<uint32>::max())
        {
            error = fmt::format("{}: the file is {} bytes, larger than a PE image can be", ConfigMgr::PathToUtf8(path), fileSize);
            return nullptr;
        }
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            error = fmt::format("{}: cannot open the file", ConfigMgr::PathToUtf8(path));
            return nullptr;
        }
        std::vector<uint8> bytes(static_cast<std::size_t>(fileSize));
        stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (static_cast<uint64>(stream.gcount()) != fileSize)
        {
            error = fmt::format("{}: could read only {} of its {} bytes", ConfigMgr::PathToUtf8(path), stream.gcount(), fileSize);
            return nullptr;
        }
        std::string parseError;
        std::unique_ptr<PeImage> image = Parse(std::move(bytes), parseError);
        if (!image)
            error = fmt::format("{}: {}", ConfigMgr::PathToUtf8(path), parseError);
        return image;
    }
    catch (std::exception const& exception)
    {
        error = fmt::format("{}: {}", ConfigMgr::PathToUtf8(path), exception.what());
        return nullptr;
    }
}

std::span<uint8 const> PeImage::GetBytes() const noexcept
{
    return _bytes;
}

uint16 PeImage::GetMachine() const noexcept
{
    return _machine;
}

uint32 PeImage::GetTimeDateStamp() const noexcept
{
    return _timeDateStamp;
}

uint64 PeImage::GetImageBase() const noexcept
{
    return _imageBase;
}

uint32 PeImage::GetSizeOfImage() const noexcept
{
    return _sizeOfImage;
}

uint32 PeImage::GetSizeOfHeaders() const noexcept
{
    return _sizeOfHeaders;
}

uint32 PeImage::GetEntryPointRva() const noexcept
{
    return _entryPointRva;
}

bool PeImage::IsDll() const noexcept
{
    return (_characteristics & FileCharacteristicDll) != 0;
}

std::vector<PeSection> const& PeImage::GetSections() const noexcept
{
    return _sections;
}

PeSection const* PeImage::FindSection(std::string_view name) const
{
    auto const found = std::find_if(_sections.begin(), _sections.end(), [name](PeSection const& section) { return section.Name == name; });
    return found == _sections.end() ? nullptr : &*found;
}

PeSection const* PeImage::SectionOfRva(uint32 rva) const
{
    auto const found = std::find_if(_sections.begin(), _sections.end(), [rva](PeSection const& section) { return rva >= section.VirtualAddress && rva - section.VirtualAddress < SectionExtent(section); });
    return found == _sections.end() ? nullptr : &*found;
}

std::optional<uint32> PeImage::RvaToOffset(uint32 rva) const
{
    std::span<uint8 const> const available = FileBackedFrom(*this, rva);
    if (available.empty())
        return std::nullopt;
    return static_cast<uint32>(available.data() - _bytes.data());
}

std::optional<uint32> PeImage::OffsetToRva(uint32 offset) const
{
    if (offset < std::min<uint64>(_sizeOfHeaders, _bytes.size()))
        return offset;
    for (PeSection const& section : _sections)
    {
        if (section.RawSize == 0 || offset < section.RawOffset || offset - section.RawOffset >= section.RawSize)
            continue;
        uint64 const rva = uint64{ section.VirtualAddress } + (offset - section.RawOffset);
        if (rva <= MaxRva)
            return static_cast<uint32>(rva);
    }
    return std::nullopt;
}

std::span<uint8 const> PeImage::ReadRva(uint32 rva, uint32 size) const
{
    std::span<uint8 const> const available = FileBackedFrom(*this, rva);
    if (size == 0 || available.size() < size)
        return {};
    return available.first(size);
}

std::vector<PeExport> const& PeImage::GetExports() const noexcept
{
    return _exports;
}

PeExport const* PeImage::FindExport(std::string_view name) const
{
    if (name.empty())
        return nullptr;
    auto const found = std::lower_bound(_exports.begin(), _exports.end(), name, [](PeExport const& entry, std::string_view value) { return std::string_view(entry.Name) < value; });
    return found != _exports.end() && found->Name == name ? &*found : nullptr;
}

PeExport const* PeImage::FindExportByOrdinal(uint16 ordinal) const
{
    auto const found = std::find_if(_exports.begin(), _exports.end(), [ordinal](PeExport const& entry) { return entry.Ordinal == ordinal; });
    return found == _exports.end() ? nullptr : &*found;
}

std::vector<PeImport> const& PeImage::GetImports() const noexcept
{
    return _imports;
}

std::vector<PeRelocation> const& PeImage::GetRelocations() const noexcept
{
    return _relocations;
}

std::optional<PeTls> const& PeImage::GetTls() const noexcept
{
    return _tls;
}

std::vector<PeFunction> const& PeImage::GetFunctions() const noexcept
{
    return _functions;
}

PeFunction const* PeImage::FunctionOfRva(uint32 rva) const
{
    auto found = std::upper_bound(_functions.begin(), _functions.end(), rva, [](uint32 value, PeFunction const& function) { return value < function.Begin; });
    if (found == _functions.begin())
        return nullptr;
    --found;
    return rva < found->End ? &*found : nullptr;
}

std::optional<uint32> PeImage::PrimaryFunctionStart(uint32 rva) const
{
    PeFunction const* const function = FunctionOfRva(rva);
    if (function == nullptr)
        return std::nullopt;
    uint32 begin = function->Begin;
    uint32 unwind = function->UnwindRva;
    for (uint32 hop = 0;; ++hop)
    {
        std::span<uint8 const> const info = ReadRva(unwind, UnwindInfoHeaderSize);
        if (info.size() != UnwindInfoHeaderSize)
            return std::nullopt;
        if (((info[0] >> 3) & UnwindFlagChainInfo) == 0)
            return begin;
        if (hop == MaxChainHops)
            return std::nullopt;
        uint64 const chainAt = uint64{ unwind } + UnwindInfoHeaderSize + ((uint64{ info[2] } + 1) & ~uint64{ 1 }) * 2;
        if (chainAt > MaxRva)
            return std::nullopt;
        std::span<uint8 const> const chained = ReadRva(static_cast<uint32>(chainAt), RuntimeFunctionSize);
        if (chained.size() != RuntimeFunctionSize)
            return std::nullopt;
        begin = Get32(chained.data());
        uint32 const end = Get32(chained.data() + 4);
        unwind = Get32(chained.data() + 8);
        if (begin >= end || end > _sizeOfImage)
            return std::nullopt;
    }
}

std::optional<uint32> PeImage::FunctionEnd(uint32 rva) const
{
    std::optional<uint32> const primary = PrimaryFunctionStart(rva);
    if (!primary)
        return std::nullopt;
    auto region = std::upper_bound(_functions.begin(), _functions.end(), rva, [](uint32 value, PeFunction const& function) { return value < function.Begin; }) - 1;
    uint32 end = region->End;
    for (++region; region != _functions.end() && region->Begin == end && PrimaryFunctionStart(region->Begin) == primary; ++region)
        end = region->End;
    return end;
}

std::vector<uint32> PeImage::FindTerminatedString(std::string_view text) const
{
    std::vector<uint32> found;
    if (text.empty())
        return found;
    uint8 const first = static_cast<uint8>(text.front());
    for (PeSection const& section : _sections)
    {
        if (section.RawSize <= text.size())
            continue;
        uint8 const* const begin = _bytes.data() + section.RawOffset;
        std::size_t const candidates = section.RawSize - text.size();
        std::size_t position = 0;
        while (position < candidates)
        {
            void const* const hit = std::memchr(begin + position, first, candidates - position);
            if (hit == nullptr)
                break;
            position = static_cast<std::size_t>(static_cast<uint8 const*>(hit) - begin);
            if ((position == 0 || begin[position - 1] == 0) && begin[position + text.size()] == 0 && std::memcmp(begin + position, text.data(), text.size()) == 0)
            {
                uint64 const rva = uint64{ section.VirtualAddress } + position;
                if (rva <= MaxRva)
                    found.push_back(static_cast<uint32>(rva));
            }
            ++position;
        }
    }
    return found;
}
