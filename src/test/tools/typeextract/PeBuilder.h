/*
 * Project Ambrose by Imjustchico
 * Builds small PE32+ images in memory for typeextract tests: sections with code or data, an entry point, named and forwarded exports, imports by name or ordinal, DIR64 relocations, a TLS directory with its relocations, and an exception table with primary and chained unwind info.
 */

#ifndef AMBROSE_PEBUILDER_H
#define AMBROSE_PEBUILDER_H

#include "Types.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class PeBuilder
{
public:
    static constexpr uint32 SectionAlignment = 0x1000;
    static constexpr uint32 FileAlignment = 0x200;
    static constexpr uint32 HeadersSize = 0x400;
    static constexpr uint32 CodeCharacteristics = 0x60000020;
    static constexpr uint32 DataCharacteristics = 0xC0000040;
    static constexpr uint32 ReadOnlyCharacteristics = 0x40000040;

    explicit PeBuilder(uint64 imageBase = 0x140000000, bool dll = false);

    uint32 AddSection(std::string name, std::vector<uint8> data, uint32 characteristics, uint32 virtualSize = 0);
    uint32 NextSectionRva() const;
    void SetEntryPoint(uint32 rva);
    void SetTimeDateStamp(uint32 stamp);

    void SetExportName(std::string dllName);
    void AddExport(std::string name, uint32 rva);
    void AddForwarder(std::string name, std::string target);
    void AddImport(std::string dll, std::string name);
    void AddImportByOrdinal(std::string dll, uint16 ordinal);
    void AddRelocation(uint32 rva);
    void SetTls(uint32 templateRva, uint32 templateSize, uint32 indexRva, std::vector<uint32> callbackRvas, uint32 zeroFill = 0);
    void AddFunction(uint32 begin, uint32 end);
    void AddChainedFunction(uint32 begin, uint32 end, uint32 primaryBegin, uint32 primaryEnd);

    std::vector<uint8> Build();

    uint32 ImportSlotRva(std::string_view dll, std::string_view name) const;
    uint32 ImportSlotRva(std::string_view dll, uint16 ordinal) const;
    uint64 GetImageBase() const noexcept { return _imageBase; }

private:
    struct Section
    {
        std::string Name;
        std::vector<uint8> Data;
        uint32 Characteristics = 0;
        uint32 VirtualSize = 0;
        uint32 Rva = 0;
    };

    struct Export
    {
        std::string Name;
        uint32 Rva = 0;
        std::string Forwarder;
    };

    struct Import
    {
        std::string Name;
        std::optional<uint16> Ordinal;
    };

    struct Function
    {
        uint32 Begin = 0;
        uint32 End = 0;
        std::optional<std::pair<uint32, uint32>> Primary;
    };

    struct Tls
    {
        uint32 TemplateRva = 0;
        uint32 TemplateSize = 0;
        uint32 IndexRva = 0;
        std::vector<uint32> Callbacks;
        uint32 ZeroFill = 0;
    };

    uint32 Append(std::string name, std::vector<uint8> data, uint32 characteristics, uint32 virtualSize);

    uint64 _imageBase;
    bool _dll;
    uint32 _entryPoint = 0;
    uint32 _timeDateStamp = 0x5F000000;
    std::vector<Section> _sections;
    std::string _exportName = "test.dll";
    std::vector<Export> _exports;
    std::vector<std::pair<std::string, std::vector<Import>>> _imports;
    std::vector<uint32> _relocations;
    std::optional<Tls> _tls;
    std::vector<Function> _functions;
    std::map<std::string, uint32> _slots;
};

#endif
