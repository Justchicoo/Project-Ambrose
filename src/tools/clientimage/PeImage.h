/*
 * Project Ambrose by Imjustchico
 * Read-only PE32+ image parsed from the bytes of an executable or DLL: headers, sections, exports with forwarders, imports, base relocations, TLS, the exception table and each function's primary start, with RVA and file offset mapping and string search.
 */

#ifndef AMBROSE_PEIMAGE_H
#define AMBROSE_PEIMAGE_H

#include "Types.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct PeSection
{
    std::string Name;
    uint32 VirtualAddress = 0;
    uint32 VirtualSize = 0;
    uint32 RawOffset = 0;
    uint32 RawSize = 0;
    uint32 Characteristics = 0;

    bool IsExecutable() const noexcept { return (Characteristics & 0x20000000u) != 0; }
};

struct PeExport
{
    std::string Name;
    uint16 Ordinal = 0;
    uint32 Rva = 0;
    std::string Forwarder;
};

struct PeImport
{
    std::string Dll;
    std::string Name;
    std::optional<uint16> Ordinal;
    uint32 SlotRva = 0;
};

struct PeRelocation
{
    uint32 Rva = 0;
    uint8 Type = 0;
};

struct PeTls
{
    uint64 StartAddressOfRawData = 0;
    uint64 EndAddressOfRawData = 0;
    uint64 AddressOfIndex = 0;
    uint64 AddressOfCallBacks = 0;
    uint32 SizeOfZeroFill = 0;
};

struct PeFunction
{
    uint32 Begin = 0;
    uint32 End = 0;
    uint32 UnwindRva = 0;
};

class PeImage
{
public:
    static constexpr uint16 MachineAmd64 = 0x8664;
    static constexpr uint8 RelocationDir64 = 10;
    static constexpr uint8 RelocationHighLow = 3;
    static constexpr uint8 RelocationAbsolute = 0;

    static std::unique_ptr<PeImage> Parse(std::vector<uint8> bytes, std::string& error);
    static std::unique_ptr<PeImage> Load(std::filesystem::path const& path, std::string& error);

    PeImage(PeImage const&) = delete;
    PeImage& operator=(PeImage const&) = delete;

    std::span<uint8 const> GetBytes() const noexcept;
    uint16 GetMachine() const noexcept;
    uint32 GetTimeDateStamp() const noexcept;
    uint64 GetImageBase() const noexcept;
    uint32 GetSizeOfImage() const noexcept;
    uint32 GetSizeOfHeaders() const noexcept;
    uint32 GetEntryPointRva() const noexcept;
    bool IsDll() const noexcept;

    std::vector<PeSection> const& GetSections() const noexcept;
    PeSection const* FindSection(std::string_view name) const;
    PeSection const* SectionOfRva(uint32 rva) const;
    std::optional<uint32> RvaToOffset(uint32 rva) const;
    std::optional<uint32> OffsetToRva(uint32 offset) const;
    std::span<uint8 const> ReadRva(uint32 rva, uint32 size) const;

    std::vector<PeExport> const& GetExports() const noexcept;
    PeExport const* FindExport(std::string_view name) const;
    PeExport const* FindExportByOrdinal(uint16 ordinal) const;
    std::vector<PeImport> const& GetImports() const noexcept;
    std::vector<PeRelocation> const& GetRelocations() const noexcept;
    std::optional<PeTls> const& GetTls() const noexcept;

    std::vector<PeFunction> const& GetFunctions() const noexcept;
    PeFunction const* FunctionOfRva(uint32 rva) const;
    std::optional<uint32> PrimaryFunctionStart(uint32 rva) const;

    std::vector<uint32> FindTerminatedString(std::string_view text) const;

private:
    explicit PeImage(std::vector<uint8> bytes);

    std::vector<uint8> _bytes;
    uint16 _machine = 0;
    uint16 _characteristics = 0;
    uint32 _timeDateStamp = 0;
    uint64 _imageBase = 0;
    uint32 _sizeOfImage = 0;
    uint32 _sizeOfHeaders = 0;
    uint32 _entryPointRva = 0;
    std::vector<PeSection> _sections;
    std::vector<PeExport> _exports;
    std::vector<PeImport> _imports;
    std::vector<PeRelocation> _relocations;
    std::optional<PeTls> _tls;
    std::vector<PeFunction> _functions;

    friend struct PeImageParser;
};

#endif
