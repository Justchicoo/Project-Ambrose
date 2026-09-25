/*
 * Project Ambrose by Imjustchico
 * Tests the PE32+ reader over PeBuilder images: headers, sections, RVA and offset mapping, exports with forwarders, imports, relocations, TLS, functions, the end of each through the regions that continue it, and chained unwind limits, string search, loading from disk, and refusal of truncated or hostile headers and directories, including import directories that share thunk arrays, repeat names past the file size or overlap address tables.
 */

#include "PeBuilder.h"
#include "PeImage.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr uint64 DllBase = 0x180000000;
    constexpr std::size_t DataDirectoriesOffset = 0x108;
    constexpr std::size_t SectionTableOffset = 0x188;
    constexpr std::size_t ExportDirectory = 0;
    constexpr std::size_t ImportDirectory = 1;
    constexpr std::size_t ExceptionDirectory = 3;
    constexpr std::size_t RelocationDirectory = 5;
    constexpr std::size_t TlsDirectory = 9;

    struct Sample
    {
        std::vector<uint8> Bytes;
        uint32 Text = 0;
        uint32 Rdata = 0;
        uint32 Data = 0;
        uint32 TickSlot = 0;
        uint32 SleepSlot = 0;
        uint32 OrdinalSlot = 0;
        uint32 SizeOfImage = 0;
    };

    std::vector<uint8> SampleRdata()
    {
        std::vector<uint8> data(0x200, 0);
        std::string const first = "enum eRace";
        std::memcpy(data.data(), first.data(), first.size());
        std::string const second = "xenum eRace";
        std::memcpy(data.data() + 0x20, second.data(), second.size());
        std::string const third = "enum eRaceX";
        std::memcpy(data.data() + 0x40, third.data(), third.size());
        std::memcpy(data.data() + 0x60, first.data(), first.size());
        std::memcpy(data.data() + 0x200 - first.size(), first.data(), first.size());
        return data;
    }

    Sample BuildSample()
    {
        Sample sample;
        PeBuilder builder(DllBase, true);
        std::vector<uint8> code(0x40, 0xCC);
        code[0] = 0xC3;
        sample.Text = builder.AddSection(".text", code, PeBuilder::CodeCharacteristics);
        sample.Rdata = builder.AddSection(".rdata", SampleRdata(), PeBuilder::ReadOnlyCharacteristics);
        std::vector<uint8> data(0x10, 0);
        std::string const inData = "enum eRace";
        std::memcpy(data.data() + 1, inData.data(), inData.size());
        sample.Data = builder.AddSection(".data", data, PeBuilder::DataCharacteristics, 0x3000);
        builder.SetEntryPoint(sample.Text + 4);
        builder.SetTimeDateStamp(0x5E5E5E5E);
        builder.SetExportName("sample.dll");
        builder.AddExport("Alpha", sample.Text);
        builder.AddExport("Beta", sample.Text + 4);
        builder.AddForwarder("Gamma", "other.Delta");
        builder.AddForwarder("Hash", "other.#12");
        builder.AddImport("KERNEL32.dll", "GetTickCount");
        builder.AddImport("KERNEL32.dll", "Sleep");
        builder.AddImportByOrdinal("WS2_32.dll", 115);
        builder.AddRelocation(sample.Data + 8);
        builder.AddRelocation(sample.Data + 0x10);
        builder.SetTls(sample.Data, 0x10, sample.Data + 0x20, { sample.Text + 8 }, 0x40);
        builder.AddFunction(sample.Text, sample.Text + 0x10);
        builder.AddFunction(sample.Text + 0x20, sample.Text + 0x30);
        builder.AddChainedFunction(sample.Text + 0x30, sample.Text + 0x38, sample.Text + 0x20, sample.Text + 0x30);
        sample.Bytes = builder.Build();
        sample.TickSlot = builder.ImportSlotRva("KERNEL32.dll", "GetTickCount");
        sample.SleepSlot = builder.ImportSlotRva("KERNEL32.dll", "Sleep");
        sample.OrdinalSlot = builder.ImportSlotRva("WS2_32.dll", uint16{ 115 });
        sample.SizeOfImage = builder.NextSectionRva();
        return sample;
    }

    uint32 Get32(std::vector<uint8> const& bytes, std::size_t offset)
    {
        return uint32{ bytes[offset] } | (uint32{ bytes[offset + 1] } << 8) | (uint32{ bytes[offset + 2] } << 16) | (uint32{ bytes[offset + 3] } << 24);
    }

    void Put16(std::vector<uint8>& bytes, std::size_t offset, uint16 value)
    {
        bytes[offset] = static_cast<uint8>(value);
        bytes[offset + 1] = static_cast<uint8>(value >> 8);
    }

    void Put32(std::vector<uint8>& bytes, std::size_t offset, uint32 value)
    {
        for (std::size_t i = 0; i < 4; ++i)
            bytes[offset + i] = static_cast<uint8>(value >> (8 * i));
    }

    void Put64(std::vector<uint8>& bytes, std::size_t offset, uint64 value)
    {
        for (std::size_t i = 0; i < 8; ++i)
            bytes[offset + i] = static_cast<uint8>(value >> (8 * i));
    }

    std::size_t DirectoryField(std::size_t index)
    {
        return DataDirectoriesOffset + index * 8;
    }

    std::size_t SectionField(std::size_t index)
    {
        return SectionTableOffset + index * 40;
    }

    std::unique_ptr<PeImage> ParseOk(std::vector<uint8> bytes)
    {
        std::string error;
        std::unique_ptr<PeImage> image = PeImage::Parse(std::move(bytes), error);
        EXPECT_NE(image, nullptr) << error;
        return image;
    }

    std::string ParseError(std::vector<uint8> bytes)
    {
        std::string error;
        std::unique_ptr<PeImage> const image = PeImage::Parse(std::move(bytes), error);
        EXPECT_EQ(image, nullptr);
        EXPECT_FALSE(error.empty());
        return error;
    }

    std::size_t DirectoryOffset(std::vector<uint8> const& bytes, std::size_t index)
    {
        std::string error;
        std::unique_ptr<PeImage> const image = PeImage::Parse(bytes, error);
        if (image == nullptr)
            return 0;
        std::optional<uint32> const offset = image->RvaToOffset(Get32(bytes, DirectoryField(index)));
        return offset ? *offset : 0;
    }

    std::size_t RvaOffset(std::vector<uint8> const& bytes, uint32 rva)
    {
        std::string error;
        std::unique_ptr<PeImage> const image = PeImage::Parse(bytes, error);
        if (image == nullptr)
            return 0;
        std::optional<uint32> const offset = image->RvaToOffset(rva);
        return offset ? *offset : 0;
    }

    std::string Text(std::span<uint8 const> bytes)
    {
        return std::string(bytes.begin(), bytes.end());
    }

    void ExpectRefused(std::vector<uint8> bytes, std::string const& fragment)
    {
        std::string const error = ParseError(std::move(bytes));
        EXPECT_NE(error.find(fragment), std::string::npos) << "expected \"" << fragment << "\" in: " << error;
    }

    void ExpectRefusedQuickly(std::vector<uint8> bytes, std::string const& fragment)
    {
        std::chrono::steady_clock::time_point const start = std::chrono::steady_clock::now();
        ExpectRefused(std::move(bytes), fragment);
        EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(1));
    }

    class ImportSection
    {
    public:
        ImportSection(uint32 rva, std::size_t descriptors) : _rva(rva), _bytes((descriptors + 1) * 20, 0)
        {
        }

        uint32 AddText(std::string_view text)
        {
            std::size_t const at = _bytes.size();
            _bytes.insert(_bytes.end(), text.begin(), text.end());
            _bytes.push_back(0);
            return _rva + static_cast<uint32>(at);
        }

        uint32 AddHintName(std::string_view name)
        {
            std::size_t const at = _bytes.size();
            _bytes.resize(at + 2, 0);
            AddText(name);
            return _rva + static_cast<uint32>(at);
        }

        uint32 AddThunks(uint64 thunk, std::size_t count)
        {
            std::size_t const at = _bytes.size();
            _bytes.resize(at + (count + 1) * 8, 0);
            for (std::size_t index = 0; index < count; ++index)
                Put64(_bytes, at + index * 8, thunk);
            return _rva + static_cast<uint32>(at);
        }

        void SetDescriptor(std::size_t index, uint32 lookup, uint32 name, uint32 addressTable)
        {
            Put32(_bytes, index * 20, lookup);
            Put32(_bytes, index * 20 + 12, name);
            Put32(_bytes, index * 20 + 16, addressTable);
        }

        std::vector<uint8> const& GetBytes() const noexcept
        {
            return _bytes;
        }

    private:
        uint32 _rva;
        std::vector<uint8> _bytes;
    };

    template <typename Layout>
    std::vector<uint8> BuildImportImage(uint32 addressTableSpace, std::size_t descriptors, Layout const& layout)
    {
        PeBuilder builder;
        builder.AddSection(".text", std::vector<uint8>(0x10, 0xC3), PeBuilder::CodeCharacteristics);
        uint32 const addressTables = builder.AddSection(".bss", std::vector<uint8>(0x10, 0), PeBuilder::DataCharacteristics, addressTableSpace);
        ImportSection section(builder.NextSectionRva(), descriptors);
        layout(section, addressTables);
        uint32 const importRva = builder.AddSection(".idata", section.GetBytes(), PeBuilder::DataCharacteristics);
        std::vector<uint8> bytes = builder.Build();
        Put32(bytes, DirectoryField(ImportDirectory), importRva);
        Put32(bytes, DirectoryField(ImportDirectory) + 4, static_cast<uint32>((descriptors + 1) * 20));
        return bytes;
    }

    struct AddressTable
    {
        std::string Dll;
        std::size_t Slots = 0;
        uint32 Offset = 0;
    };

    std::vector<uint8> BuildAddressTables(std::vector<AddressTable> const& tables, uint32& addressTables)
    {
        return BuildImportImage(0x1000, tables.size(), [&tables, &addressTables](ImportSection& section, uint32 base)
        {
            addressTables = base;
            uint32 const name = section.AddHintName("Sleep");
            for (std::size_t index = 0; index < tables.size(); ++index)
            {
                uint32 const lookup = section.AddThunks(name, tables[index].Slots);
                uint32 const dll = section.AddText(tables[index].Dll);
                section.SetDescriptor(index, lookup, dll, base + tables[index].Offset);
            }
        });
    }
}

TEST(PeImageTest, HeadersAndSectionsAreRead)
{
    Sample const sample = BuildSample();
    std::unique_ptr<PeImage> const image = ParseOk(sample.Bytes);
    ASSERT_NE(image, nullptr);
    EXPECT_EQ(image->GetMachine(), PeImage::MachineAmd64);
    EXPECT_EQ(image->GetTimeDateStamp(), 0x5E5E5E5Eu);
    EXPECT_EQ(image->GetImageBase(), DllBase);
    EXPECT_EQ(image->GetSizeOfImage(), sample.SizeOfImage);
    EXPECT_EQ(image->GetSizeOfHeaders(), PeBuilder::HeadersSize);
    EXPECT_EQ(image->GetEntryPointRva(), sample.Text + 4);
    EXPECT_TRUE(image->IsDll());
    EXPECT_EQ(image->GetBytes().size(), sample.Bytes.size());

    std::vector<PeSection> const& sections = image->GetSections();
    ASSERT_GE(sections.size(), 3u);
    EXPECT_EQ(sections[0].Name, ".text");
    EXPECT_EQ(sections[0].VirtualAddress, sample.Text);
    EXPECT_EQ(sections[0].VirtualSize, 0x40u);
    EXPECT_EQ(sections[0].RawOffset, PeBuilder::HeadersSize);
    EXPECT_EQ(sections[0].RawSize, PeBuilder::FileAlignment);
    EXPECT_EQ(sections[0].Characteristics, PeBuilder::CodeCharacteristics);
    EXPECT_TRUE(sections[0].IsExecutable());
    EXPECT_EQ(sections[1].Name, ".rdata");
    EXPECT_FALSE(sections[1].IsExecutable());
    EXPECT_EQ(sections[2].Name, ".data");
    EXPECT_EQ(sections[2].VirtualSize, 0x3000u);
    EXPECT_EQ(sections[2].RawSize, PeBuilder::FileAlignment);
    std::vector<std::string> names;
    for (PeSection const& section : sections)
        names.push_back(section.Name);
    EXPECT_EQ(names, (std::vector<std::string>{ ".text", ".rdata", ".data", ".xdata", ".pdata", ".idata", ".edata", ".tls", ".reloc" }));

    ASSERT_NE(image->FindSection(".rdata"), nullptr);
    EXPECT_EQ(image->FindSection(".rdata")->VirtualAddress, sample.Rdata);
    EXPECT_EQ(image->FindSection(".bss"), nullptr);
    EXPECT_EQ(image->FindSection(".rdat"), nullptr);

    PeBuilder exe;
    exe.AddSection("longname", std::vector<uint8>(4, 0xC3), PeBuilder::CodeCharacteristics);
    std::unique_ptr<PeImage> const program = ParseOk(exe.Build());
    ASSERT_NE(program, nullptr);
    EXPECT_FALSE(program->IsDll());
    EXPECT_EQ(program->GetImageBase(), 0x140000000u);
    ASSERT_EQ(program->GetSections().size(), 1u);
    EXPECT_EQ(program->GetSections()[0].Name, "longname");
    EXPECT_TRUE(program->GetExports().empty());
    EXPECT_TRUE(program->GetImports().empty());
    EXPECT_TRUE(program->GetRelocations().empty());
    EXPECT_FALSE(program->GetTls());
    EXPECT_TRUE(program->GetFunctions().empty());
    EXPECT_EQ(program->FunctionOfRva(0x1000), nullptr);
    EXPECT_FALSE(program->PrimaryFunctionStart(0x1000));
}

TEST(PeImageTest, RvasAndOffsetsMapBothWays)
{
    Sample const sample = BuildSample();
    std::unique_ptr<PeImage> const image = ParseOk(sample.Bytes);
    ASSERT_NE(image, nullptr);
    PeSection const& data = image->GetSections()[2];

    EXPECT_EQ(image->RvaToOffset(0x10), 0x10u);
    EXPECT_EQ(image->RvaToOffset(PeBuilder::HeadersSize - 1), PeBuilder::HeadersSize - 1);
    EXPECT_FALSE(image->RvaToOffset(PeBuilder::HeadersSize));
    EXPECT_EQ(image->RvaToOffset(sample.Text + 5), PeBuilder::HeadersSize + 5);
    EXPECT_EQ(image->RvaToOffset(sample.Data + 0x1FF), data.RawOffset + 0x1FF);
    EXPECT_FALSE(image->RvaToOffset(sample.Data + 0x200));
    EXPECT_FALSE(image->RvaToOffset(sample.Data + 0x2FFF));
    EXPECT_FALSE(image->RvaToOffset(sample.SizeOfImage));
    EXPECT_FALSE(image->RvaToOffset(0xFFFFFFFF));

    EXPECT_EQ(image->OffsetToRva(0x10), 0x10u);
    EXPECT_EQ(image->OffsetToRva(PeBuilder::HeadersSize + 5), sample.Text + 5);
    EXPECT_EQ(image->OffsetToRva(data.RawOffset + 0x20), sample.Data + 0x20);
    EXPECT_FALSE(image->OffsetToRva(static_cast<uint32>(sample.Bytes.size())));
    EXPECT_FALSE(image->OffsetToRva(0xFFFFFFFF));

    ASSERT_NE(image->SectionOfRva(sample.Data + 0x2FFF), nullptr);
    EXPECT_EQ(image->SectionOfRva(sample.Data + 0x2FFF)->Name, ".data");
    ASSERT_NE(image->SectionOfRva(sample.Data + 0x3000), nullptr);
    EXPECT_EQ(image->SectionOfRva(sample.Data + 0x3000)->Name, ".xdata");
    EXPECT_EQ(image->SectionOfRva(0x10), nullptr);
    ASSERT_NE(image->SectionOfRva(sample.Text + 0x100), nullptr);
    EXPECT_EQ(image->SectionOfRva(sample.Text + 0x100)->Name, ".text");

    EXPECT_EQ(Text(image->ReadRva(0, 2)), "MZ");
    EXPECT_EQ(image->ReadRva(PeBuilder::HeadersSize - 1, 1).size(), 1u);
    EXPECT_TRUE(image->ReadRva(PeBuilder::HeadersSize - 1, 2).empty());
    EXPECT_EQ(Text(image->ReadRva(sample.Rdata, 10)), "enum eRace");
    EXPECT_EQ(image->ReadRva(sample.Data + 0x1F0, 0x10).size(), 0x10u);
    EXPECT_TRUE(image->ReadRva(sample.Data + 0x1F0, 0x11).empty());
    EXPECT_TRUE(image->ReadRva(sample.Data + 0x800, 1).empty());
    EXPECT_TRUE(image->ReadRva(sample.Text, 0).empty());
    EXPECT_TRUE(image->ReadRva(0xFFFFFFF0, 0x20).empty());
    std::span<uint8 const> const code = image->ReadRva(sample.Text, 1);
    ASSERT_EQ(code.size(), 1u);
    EXPECT_EQ(code[0], 0xC3);
}

TEST(PeImageTest, ExportsListNamesOrdinalsAndForwarders)
{
    Sample const sample = BuildSample();
    std::unique_ptr<PeImage> const image = ParseOk(sample.Bytes);
    ASSERT_NE(image, nullptr);
    ASSERT_EQ(image->GetExports().size(), 4u);

    PeExport const* const alpha = image->FindExport("Alpha");
    ASSERT_NE(alpha, nullptr);
    EXPECT_EQ(alpha->Ordinal, 1);
    EXPECT_EQ(alpha->Rva, sample.Text);
    EXPECT_TRUE(alpha->Forwarder.empty());

    PeExport const* const beta = image->FindExport("Beta");
    ASSERT_NE(beta, nullptr);
    EXPECT_EQ(beta->Ordinal, 2);
    EXPECT_EQ(beta->Rva, sample.Text + 4);

    PeExport const* const gamma = image->FindExport("Gamma");
    ASSERT_NE(gamma, nullptr);
    EXPECT_EQ(gamma->Ordinal, 3);
    EXPECT_EQ(gamma->Rva, 0u);
    EXPECT_EQ(gamma->Forwarder, "other.Delta");

    PeExport const* const hash = image->FindExportByOrdinal(4);
    ASSERT_NE(hash, nullptr);
    EXPECT_EQ(hash->Name, "Hash");
    EXPECT_EQ(hash->Forwarder, "other.#12");
    EXPECT_EQ(image->FindExportByOrdinal(2), beta);

    EXPECT_EQ(image->FindExport("Delta"), nullptr);
    EXPECT_EQ(image->FindExport("alpha"), nullptr);
    EXPECT_EQ(image->FindExport(""), nullptr);
    EXPECT_EQ(image->FindExportByOrdinal(0), nullptr);
    EXPECT_EQ(image->FindExportByOrdinal(5), nullptr);

    std::vector<uint8> unnamed = sample.Bytes;
    std::size_t const directory = DirectoryOffset(unnamed, ExportDirectory);
    ASSERT_NE(directory, 0u);
    Put32(unnamed, directory + 16, 10);
    Put32(unnamed, directory + 24, 0);
    std::unique_ptr<PeImage> const byOrdinal = ParseOk(unnamed);
    ASSERT_NE(byOrdinal, nullptr);
    ASSERT_EQ(byOrdinal->GetExports().size(), 4u);
    EXPECT_EQ(byOrdinal->FindExport("Alpha"), nullptr);
    ASSERT_NE(byOrdinal->FindExportByOrdinal(11), nullptr);
    EXPECT_TRUE(byOrdinal->FindExportByOrdinal(11)->Name.empty());
    EXPECT_EQ(byOrdinal->FindExportByOrdinal(11)->Rva, sample.Text + 4);
    EXPECT_EQ(byOrdinal->FindExportByOrdinal(13)->Forwarder, "other.#12");
}

TEST(PeImageTest, ImportsListSlotsByNameAndOrdinal)
{
    Sample const sample = BuildSample();
    std::unique_ptr<PeImage> const image = ParseOk(sample.Bytes);
    ASSERT_NE(image, nullptr);
    std::vector<PeImport> const& imports = image->GetImports();
    ASSERT_EQ(imports.size(), 3u);
    EXPECT_EQ(imports[0].Dll, "KERNEL32.dll");
    EXPECT_EQ(imports[0].Name, "GetTickCount");
    EXPECT_FALSE(imports[0].Ordinal);
    EXPECT_EQ(imports[0].SlotRva, sample.TickSlot);
    EXPECT_EQ(imports[1].Name, "Sleep");
    EXPECT_EQ(imports[1].SlotRva, sample.SleepSlot);
    EXPECT_EQ(imports[1].SlotRva, imports[0].SlotRva + 8);
    EXPECT_EQ(imports[2].Dll, "WS2_32.dll");
    EXPECT_TRUE(imports[2].Name.empty());
    EXPECT_EQ(imports[2].Ordinal, uint16{ 115 });
    EXPECT_EQ(imports[2].SlotRva, sample.OrdinalSlot);

    std::vector<uint8> bound = sample.Bytes;
    std::size_t const descriptor = DirectoryOffset(bound, ImportDirectory);
    ASSERT_NE(descriptor, 0u);
    uint32 const lookupRva = Get32(bound, descriptor);
    std::size_t const lookup = RvaOffset(bound, lookupRva);
    ASSERT_NE(lookup, 0u);
    std::size_t const addressTable = RvaOffset(bound, sample.TickSlot);
    ASSERT_NE(addressTable, 0u);
    Put64(bound, addressTable, 0x7FFE0000AAAAull);
    Put64(bound, addressTable + 8, 0x7FFE0000BBBBull);
    std::unique_ptr<PeImage> const fromLookup = ParseOk(bound);
    ASSERT_NE(fromLookup, nullptr);
    ASSERT_EQ(fromLookup->GetImports().size(), 3u);
    EXPECT_EQ(fromLookup->GetImports()[1].Name, "Sleep");

    std::vector<uint8> unbound = sample.Bytes;
    Put32(unbound, descriptor, 0);
    Put64(unbound, lookup, 0x7FFFFF00);
    std::unique_ptr<PeImage> const fromAddressTable = ParseOk(unbound);
    ASSERT_NE(fromAddressTable, nullptr);
    ASSERT_EQ(fromAddressTable->GetImports().size(), 3u);
    EXPECT_EQ(fromAddressTable->GetImports()[0].Name, "GetTickCount");
    EXPECT_EQ(fromAddressTable->GetImports()[0].SlotRva, sample.TickSlot);
}

TEST(PeImageTest, RelocationsSkipPaddingAndTlsIsReadAsStored)
{
    Sample const sample = BuildSample();
    std::unique_ptr<PeImage> const image = ParseOk(sample.Bytes);
    ASSERT_NE(image, nullptr);
    std::vector<PeRelocation> const& relocations = image->GetRelocations();
    ASSERT_EQ(relocations.size(), 7u);
    EXPECT_EQ(relocations[0].Rva, sample.Data + 8);
    EXPECT_EQ(relocations[1].Rva, sample.Data + 0x10);
    for (PeRelocation const& relocation : relocations)
        EXPECT_EQ(relocation.Type, PeImage::RelocationDir64);
    PeSection const* const tlsSection = image->FindSection(".tls");
    ASSERT_NE(tlsSection, nullptr);
    EXPECT_EQ(relocations[2].Rva, tlsSection->VirtualAddress);
    EXPECT_EQ(relocations[6].Rva, tlsSection->VirtualAddress + 40);

    ASSERT_TRUE(image->GetTls());
    PeTls const& tls = *image->GetTls();
    EXPECT_EQ(tls.StartAddressOfRawData, DllBase + sample.Data);
    EXPECT_EQ(tls.EndAddressOfRawData, DllBase + sample.Data + 0x10);
    EXPECT_EQ(tls.AddressOfIndex, DllBase + sample.Data + 0x20);
    EXPECT_EQ(tls.AddressOfCallBacks, DllBase + tlsSection->VirtualAddress + 40);
    EXPECT_EQ(tls.SizeOfZeroFill, 0x40u);
    std::span<uint8 const> const callbacks = image->ReadRva(static_cast<uint32>(tls.AddressOfCallBacks - DllBase), 16);
    ASSERT_EQ(callbacks.size(), 16u);
    EXPECT_EQ(Get32(std::vector<uint8>(callbacks.begin(), callbacks.end()), 0), static_cast<uint32>(DllBase + sample.Text + 8));

    std::vector<uint8> highLow = sample.Bytes;
    std::size_t const block = DirectoryOffset(highLow, RelocationDirectory);
    ASSERT_NE(block, 0u);
    Put16(highLow, block + 8, static_cast<uint16>((PeImage::RelocationHighLow << 12) | 0xFFC));
    std::unique_ptr<PeImage> const patched = ParseOk(highLow);
    ASSERT_NE(patched, nullptr);
    EXPECT_EQ(patched->GetRelocations()[0].Type, PeImage::RelocationHighLow);
    EXPECT_EQ(patched->GetRelocations()[0].Rva, sample.Data + 0xFFC);
}

TEST(PeImageTest, FewerDataDirectoriesHideTheLaterOnes)
{
    Sample const sample = BuildSample();
    std::vector<uint8> bytes = sample.Bytes;
    Put32(bytes, DataDirectoriesOffset - 4, 2);
    std::unique_ptr<PeImage> const image = ParseOk(bytes);
    ASSERT_NE(image, nullptr);
    EXPECT_EQ(image->GetExports().size(), 4u);
    EXPECT_EQ(image->GetImports().size(), 3u);
    EXPECT_TRUE(image->GetRelocations().empty());
    EXPECT_FALSE(image->GetTls());
    EXPECT_TRUE(image->GetFunctions().empty());

    Put32(bytes, DataDirectoriesOffset - 4, 0x1000);
    std::unique_ptr<PeImage> const many = ParseOk(bytes);
    ASSERT_NE(many, nullptr);
    EXPECT_EQ(many->GetFunctions().size(), 3u);
}

TEST(PeImageTest, FunctionsAreFoundAndChainsLeadToThePrimaryStart)
{
    Sample const sample = BuildSample();
    std::unique_ptr<PeImage> const image = ParseOk(sample.Bytes);
    ASSERT_NE(image, nullptr);
    std::vector<PeFunction> const& functions = image->GetFunctions();
    ASSERT_EQ(functions.size(), 3u);
    EXPECT_EQ(functions[0].Begin, sample.Text);
    EXPECT_EQ(functions[0].End, sample.Text + 0x10);
    EXPECT_NE(functions[0].UnwindRva, 0u);

    ASSERT_NE(image->FunctionOfRva(sample.Text + 0xF), nullptr);
    EXPECT_EQ(image->FunctionOfRva(sample.Text + 0xF)->Begin, sample.Text);
    EXPECT_EQ(image->FunctionOfRva(sample.Text + 0x10), nullptr);
    EXPECT_EQ(image->FunctionOfRva(sample.Text + 0x1F), nullptr);
    EXPECT_EQ(image->FunctionOfRva(sample.Text - 1), nullptr);
    ASSERT_NE(image->FunctionOfRva(sample.Text + 0x37), nullptr);
    EXPECT_EQ(image->FunctionOfRva(sample.Text + 0x37)->Begin, sample.Text + 0x30);
    EXPECT_EQ(image->FunctionOfRva(sample.Text + 0x38), nullptr);

    EXPECT_EQ(image->PrimaryFunctionStart(sample.Text + 2), sample.Text);
    EXPECT_EQ(image->PrimaryFunctionStart(sample.Text + 0x25), sample.Text + 0x20);
    EXPECT_EQ(image->PrimaryFunctionStart(sample.Text + 0x34), sample.Text + 0x20);
    EXPECT_FALSE(image->PrimaryFunctionStart(sample.Text + 0x15));
    EXPECT_FALSE(image->PrimaryFunctionStart(0));

    EXPECT_EQ(image->FunctionEnd(sample.Text + 2), sample.Text + 0x10);
    EXPECT_EQ(image->FunctionEnd(sample.Text + 0x25), sample.Text + 0x38) << "a function ends where the last region continuing it ends";
    EXPECT_EQ(image->FunctionEnd(sample.Text + 0x34), sample.Text + 0x38);
    EXPECT_FALSE(image->FunctionEnd(sample.Text + 0x15));
    EXPECT_FALSE(image->FunctionEnd(0));

    std::vector<uint8> unsorted = sample.Bytes;
    std::size_t const table = DirectoryOffset(unsorted, ExceptionDirectory);
    ASSERT_NE(table, 0u);
    std::vector<uint8> const first(unsorted.begin() + static_cast<std::ptrdiff_t>(table), unsorted.begin() + static_cast<std::ptrdiff_t>(table + 12));
    std::copy(unsorted.begin() + static_cast<std::ptrdiff_t>(table + 24), unsorted.begin() + static_cast<std::ptrdiff_t>(table + 36), unsorted.begin() + static_cast<std::ptrdiff_t>(table));
    std::copy(first.begin(), first.end(), unsorted.begin() + static_cast<std::ptrdiff_t>(table + 24));
    std::unique_ptr<PeImage> const sorted = ParseOk(unsorted);
    ASSERT_NE(sorted, nullptr);
    ASSERT_EQ(sorted->GetFunctions().size(), 3u);
    EXPECT_EQ(sorted->GetFunctions()[0].Begin, sample.Text);
    EXPECT_EQ(sorted->GetFunctions()[1].Begin, sample.Text + 0x20);
    EXPECT_EQ(sorted->GetFunctions()[2].Begin, sample.Text + 0x30);
    EXPECT_EQ(sorted->PrimaryFunctionStart(sample.Text + 0x34), sample.Text + 0x20);
}

TEST(PeImageTest, ChainsStopAtTheirHopLimitLoopsAndMalformedInfo)
{
    PeBuilder builder;
    uint32 const text = builder.AddSection(".text", std::vector<uint8>(0x100, 0xCC), PeBuilder::CodeCharacteristics);
    uint32 const unwind = builder.NextSectionRva();
    std::vector<uint8> infos(0x1000, 0);
    auto const chained = [&infos](std::size_t at, uint8 codes, uint32 begin, uint32 end, uint32 next)
    {
        infos[at] = 0x21;
        infos[at + 2] = codes;
        std::size_t const entry = at + 4 + ((std::size_t{ codes } + 1) & ~std::size_t{ 1 }) * 2;
        Put32(infos, entry, begin);
        Put32(infos, entry + 4, end);
        Put32(infos, entry + 8, next);
    };

    constexpr std::size_t Primary = 0;
    infos[Primary] = 0x01;
    constexpr std::size_t Ladder = 0x10;
    constexpr std::size_t Rungs = 33;
    for (std::size_t rung = 0; rung < Rungs; ++rung)
    {
        std::size_t const at = Ladder + rung * 16;
        uint32 const next = rung + 1 < Rungs ? unwind + static_cast<uint32>(at + 16) : unwind + static_cast<uint32>(Primary);
        chained(at, 0, text + 0x80 + static_cast<uint32>(rung), text + 0x100, next);
    }
    constexpr std::size_t LoopA = 0x300;
    constexpr std::size_t LoopB = 0x310;
    chained(LoopA, 0, text + 0x40, text + 0x50, unwind + LoopB);
    chained(LoopB, 0, text + 0x50, text + 0x60, unwind + LoopA);
    constexpr std::size_t CodedA = 0x340;
    constexpr std::size_t CodedB = 0x360;
    chained(CodedA, 3, text + 0x40, text + 0x50, unwind + CodedB);
    chained(CodedB, 1, text + 0x10, text + 0x20, unwind + Primary);
    constexpr std::size_t BadEntry = 0x380;
    chained(BadEntry, 0, text + 0x50, text + 0x50, unwind + Primary);
    constexpr std::size_t CutOff = 0xFF8;
    infos[CutOff] = 0x21;
    constexpr std::size_t PastImage = 0x390;
    chained(PastImage, 0, text, 0x7FFFFFFF, unwind + Primary);

    EXPECT_EQ(builder.AddSection(".unwind", infos, PeBuilder::ReadOnlyCharacteristics), unwind);
    uint32 const bss = builder.AddSection(".bss", std::vector<uint8>(0x10, 0), PeBuilder::DataCharacteristics, 0x4000);
    std::vector<uint32> const entries = {
        unwind + static_cast<uint32>(Ladder + 16),
        unwind + static_cast<uint32>(Ladder),
        unwind + static_cast<uint32>(LoopA),
        unwind + static_cast<uint32>(CodedA),
        unwind + static_cast<uint32>(BadEntry),
        unwind + static_cast<uint32>(CutOff),
        bss + 0x2000,
        unwind + static_cast<uint32>(PastImage),
    };
    for (std::size_t index = 0; index < entries.size(); ++index)
        builder.AddFunction(text + static_cast<uint32>(index * 8), text + static_cast<uint32>(index * 8 + 8));
    builder.AddFunction(text + 0xF0, text + 0xF8);
    std::vector<uint8> bytes = builder.Build();
    std::size_t const table = DirectoryOffset(bytes, ExceptionDirectory);
    ASSERT_NE(table, 0u);
    for (std::size_t index = 0; index < entries.size(); ++index)
        Put32(bytes, table + index * 12 + 8, entries[index]);

    std::unique_ptr<PeImage> const image = ParseOk(bytes);
    ASSERT_NE(image, nullptr);
    EXPECT_EQ(image->PrimaryFunctionStart(text + 0x01), text + 0x80 + 32);
    EXPECT_FALSE(image->PrimaryFunctionStart(text + 0x09));
    EXPECT_FALSE(image->PrimaryFunctionStart(text + 0x11));
    EXPECT_EQ(image->PrimaryFunctionStart(text + 0x19), text + 0x10);
    EXPECT_FALSE(image->PrimaryFunctionStart(text + 0x21));
    EXPECT_FALSE(image->PrimaryFunctionStart(text + 0x29));
    EXPECT_FALSE(image->PrimaryFunctionStart(text + 0x31));
    EXPECT_FALSE(image->PrimaryFunctionStart(text + 0x39));
    EXPECT_EQ(image->PrimaryFunctionStart(text + 0xF4), text + 0xF0);

    EXPECT_EQ(image->FunctionEnd(text + 0x01), text + 0x08) << "a region right after one whose chain leads elsewhere is another function";
    EXPECT_EQ(image->FunctionEnd(text + 0x19), text + 0x20);
    EXPECT_FALSE(image->FunctionEnd(text + 0x09));
}

TEST(PeImageTest, TerminatedStringsAreFoundInSectionData)
{
    Sample const sample = BuildSample();
    std::unique_ptr<PeImage> const image = ParseOk(sample.Bytes);
    ASSERT_NE(image, nullptr);
    std::vector<uint32> const found = image->FindTerminatedString("enum eRace");
    EXPECT_EQ(found, (std::vector<uint32>{ sample.Rdata, sample.Rdata + 0x60, sample.Data + 1 }));
    EXPECT_EQ(image->FindTerminatedString("Race"), std::vector<uint32>{});
    EXPECT_EQ(image->FindTerminatedString("xenum eRace"), std::vector<uint32>{ sample.Rdata + 0x20 });
    EXPECT_EQ(image->FindTerminatedString("enum eRaceX"), std::vector<uint32>{ sample.Rdata + 0x40 });
    EXPECT_TRUE(image->FindTerminatedString("").empty());
    EXPECT_TRUE(image->FindTerminatedString("not present").empty());
    EXPECT_TRUE(image->FindTerminatedString(std::string(0x300, 'a')).empty());
    std::vector<uint32> const dll = image->FindTerminatedString("sample.dll");
    ASSERT_EQ(dll.size(), 1u);
    EXPECT_EQ(image->SectionOfRva(dll[0])->Name, ".edata");
}

TEST(PeImageTest, LoadReadsAFileAndNamesThePathOnFailure)
{
    Sample const sample = BuildSample();
    std::filesystem::path const directory = std::filesystem::temp_directory_path() / fmt::format("ambrose-peimage-{}", std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(directory);
    std::filesystem::path const path = directory / "sample.dll";
    {
        std::ofstream stream(path, std::ios::binary);
        stream.write(reinterpret_cast<char const*>(sample.Bytes.data()), static_cast<std::streamsize>(sample.Bytes.size()));
    }
    std::filesystem::path const text = directory / "notes.txt";
    {
        std::ofstream stream(text, std::ios::binary);
        stream << "not a program";
    }

    std::string error;
    std::unique_ptr<PeImage> const image = PeImage::Load(path, error);
    ASSERT_NE(image, nullptr) << error;
    EXPECT_EQ(image->GetBytes().size(), sample.Bytes.size());
    EXPECT_NE(image->FindExport("Alpha"), nullptr);

    std::string missingError;
    EXPECT_EQ(PeImage::Load(directory / "missing.dll", missingError), nullptr);
    EXPECT_NE(missingError.find("missing.dll"), std::string::npos) << missingError;

    std::string textError;
    EXPECT_EQ(PeImage::Load(text, textError), nullptr);
    EXPECT_NE(textError.find("notes.txt"), std::string::npos) << textError;
    EXPECT_NE(textError.find("DOS header"), std::string::npos) << textError;

    std::string directoryError;
    EXPECT_EQ(PeImage::Load(directory, directoryError), nullptr);
    EXPECT_FALSE(directoryError.empty());

    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
}

TEST(PeImageTest, TruncatedHeadersAreRefused)
{
    Sample const sample = BuildSample();
    auto const cut = [&sample](std::size_t size) { return std::vector<uint8>(sample.Bytes.begin(), sample.Bytes.begin() + static_cast<std::ptrdiff_t>(size)); };
    ExpectRefused(cut(0), "too short for a DOS header");
    ExpectRefused(cut(0x3F), "too short for a DOS header");
    ExpectRefused(cut(0x40), "leaves no room for a PE signature");
    ExpectRefused(cut(0x97), "leaves no room for a PE signature");
    ExpectRefused(cut(0x98), "optional header at offset 0x98 is cut off");
    ExpectRefused(cut(0x99), "optional header at offset 0x98 is cut off");
    ExpectRefused(cut(0x120), "runs past the end of the 288-byte file");
    ExpectRefused(cut(0x187), "optional header at offset 0x98 runs past the end");
    ExpectRefused(cut(0x188), "table of 9 sections at offset 0x188");
    ExpectRefused(cut(0x2EF), "table of 9 sections at offset 0x188");
    ExpectRefused(cut(0x2F0), "section .text raw data");
    ExpectRefused(cut(0x5FF), "section .text raw data");
}

TEST(PeImageTest, EveryTruncationIsRefusedWithoutCrashing)
{
    Sample const sample = BuildSample();
    for (std::size_t size = 0; size < sample.Bytes.size(); ++size)
    {
        std::string error;
        std::unique_ptr<PeImage> const image = PeImage::Parse(std::vector<uint8>(sample.Bytes.begin(), sample.Bytes.begin() + static_cast<std::ptrdiff_t>(size)), error);
        EXPECT_EQ(image, nullptr) << size;
        EXPECT_FALSE(error.empty()) << size;
    }
}

TEST(PeImageTest, CorruptedBytesNeverCrashTheParser)
{
    Sample const sample = BuildSample();
    uint32 state = 0x2545F491;
    auto const next = [&state]()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    };
    std::size_t refused = 0;
    for (int round = 0; round < 3000; ++round)
    {
        std::vector<uint8> bytes = sample.Bytes;
        int const flips = 1 + static_cast<int>(next() % 8);
        for (int flip = 0; flip < flips; ++flip)
        {
            std::size_t const at = next() % bytes.size();
            bytes[at] = static_cast<uint8>(next());
        }
        std::string error;
        std::unique_ptr<PeImage> const image = PeImage::Parse(std::move(bytes), error);
        if (image == nullptr)
        {
            ++refused;
            EXPECT_FALSE(error.empty());
            continue;
        }
        for (PeFunction const& function : image->GetFunctions())
            static_cast<void>(image->PrimaryFunctionStart(function.Begin));
        static_cast<void>(image->FindTerminatedString("enum eRace"));
        for (PeExport const& entry : image->GetExports())
            static_cast<void>(image->FindExport(entry.Name));
    }
    EXPECT_GT(refused, 0u);
}

TEST(PeImageTest, BadSignaturesAndOptionalHeadersAreRefused)
{
    Sample const sample = BuildSample();

    std::vector<uint8> noMz = sample.Bytes;
    noMz[1] = 'X';
    ExpectRefused(noMz, "does not start with a DOS header");

    std::vector<uint8> farLfanew = sample.Bytes;
    Put32(farLfanew, 0x3C, 0xFFFFFFF0);
    ExpectRefused(farLfanew, "PE header offset 0xfffffff0 leaves no room");

    std::vector<uint8> noSignature = sample.Bytes;
    Put32(noSignature, 0x3C, 0x100);
    ExpectRefused(noSignature, "no PE signature at offset 0x100");

    std::vector<uint8> pe32 = sample.Bytes;
    Put16(pe32, 0x98, 0x10B);
    ExpectRefused(pe32, "not a PE32+ image");

    std::vector<uint8> shortOptional = sample.Bytes;
    Put16(shortOptional, 0x94, 100);
    ExpectRefused(shortOptional, "too short for PE32+");

    std::vector<uint8> tinyOptional = sample.Bytes;
    Put16(tinyOptional, 0x94, 1);
    ExpectRefused(tinyOptional, "is cut off");

    std::vector<uint8> fewDirectorySlots = sample.Bytes;
    Put16(fewDirectorySlots, 0x94, 112 + 4 * 8);
    ExpectRefused(fewDirectorySlots, "lists 16 data directories but has room for 4");

    std::vector<uint8> manySections = sample.Bytes;
    Put16(manySections, 0x86, 0xFFFF);
    ExpectRefused(manySections, "table of 65535 sections");

    std::vector<uint8> rawPastEnd = sample.Bytes;
    Put32(rawPastEnd, SectionField(1) + 16, 0x100000);
    ExpectRefused(rawPastEnd, "section .rdata raw data at offset 0x600 with 1048576 bytes runs past the end");

    std::vector<uint8> rawOffsetPastEnd = sample.Bytes;
    Put32(rawOffsetPastEnd, SectionField(0) + 20, 0xFFFFFF00);
    ExpectRefused(rawOffsetPastEnd, "section .text raw data");
}

TEST(PeImageTest, HostileExportDirectoriesAreRefused)
{
    Sample const sample = BuildSample();
    std::size_t const directory = DirectoryOffset(sample.Bytes, ExportDirectory);
    ASSERT_NE(directory, 0u);
    auto const patched = [&sample](std::size_t offset, uint32 value)
    {
        std::vector<uint8> bytes = sample.Bytes;
        Put32(bytes, offset, value);
        return bytes;
    };

    ExpectRefused(patched(DirectoryField(ExportDirectory), 0x7FFFF000), "the export directory at RVA 0x7ffff000 is not inside the file");
    ExpectRefused(patched(directory + 28, 0x7FFFFF00), "table of 4 functions at RVA 0x7fffff00");
    ExpectRefused(patched(directory + 20, 0x40000000), "table of 1073741824 functions");
    ExpectRefused(patched(directory + 32, 0x7FFFFF00), "table of 4 names at RVA 0x7fffff00");
    ExpectRefused(patched(directory + 36, 0x7FFFFF00), "table of 4 name ordinals");
    ExpectRefused(patched(directory + 16, 0xFFFF), "does not fit in 16 bits");

    std::vector<uint8> badIndex = sample.Bytes;
    std::size_t const ordinals = RvaOffset(badIndex, Get32(badIndex, directory + 36));
    ASSERT_NE(ordinals, 0u);
    Put16(badIndex, ordinals, 7);
    ExpectRefused(badIndex, "name 0 refers to function 7 of 4");

    std::vector<uint8> badName = sample.Bytes;
    std::size_t const names = RvaOffset(badName, Get32(badName, directory + 32));
    ASSERT_NE(names, 0u);
    Put32(badName, names + 4, 0x7FFFFF00);
    ExpectRefused(badName, "name 1 at RVA 0x7fffff00 is not a terminated string");

    std::vector<uint8> badFunction = sample.Bytes;
    std::size_t const functions = RvaOffset(badFunction, Get32(badFunction, directory + 28));
    ASSERT_NE(functions, 0u);
    Put32(badFunction, functions, 0x7FFFFFF0);
    ExpectRefused(badFunction, "ordinal 1 points at RVA 0x7ffffff0, outside the");

    std::vector<uint8> badForwarder = sample.Bytes;
    std::string const forwarder = "other.Delta";
    auto const found = std::search(badForwarder.begin(), badForwarder.end(), forwarder.begin(), forwarder.end());
    ASSERT_NE(found, badForwarder.end());
    *(found + 5) = '_';
    ExpectRefused(badForwarder, "ordinal 3 forwards through RVA");

    std::vector<uint8> unterminated = sample.Bytes;
    PeSection const exportSection = [&sample]()
    {
        std::unique_ptr<PeImage> const image = ParseOk(sample.Bytes);
        return *image->FindSection(".edata");
    }();
    std::fill(unterminated.begin() + exportSection.RawOffset + 0x40, unterminated.begin() + exportSection.RawOffset + exportSection.RawSize, uint8{ 'A' });
    std::string const error = ParseError(unterminated);
    EXPECT_NE(error.find("export directory"), std::string::npos) << error;
}

TEST(PeImageTest, HostileImportDirectoriesAreRefused)
{
    Sample const sample = BuildSample();
    std::size_t const descriptor = DirectoryOffset(sample.Bytes, ImportDirectory);
    ASSERT_NE(descriptor, 0u);
    std::size_t const lookup = RvaOffset(sample.Bytes, Get32(sample.Bytes, descriptor));
    ASSERT_NE(lookup, 0u);
    auto const patched32 = [&sample](std::size_t offset, uint32 value)
    {
        std::vector<uint8> bytes = sample.Bytes;
        Put32(bytes, offset, value);
        return bytes;
    };
    auto const patched64 = [&sample](std::size_t offset, uint64 value)
    {
        std::vector<uint8> bytes = sample.Bytes;
        Put64(bytes, offset, value);
        return bytes;
    };

    ExpectRefused(patched32(DirectoryField(ImportDirectory), 0x7FFFF000), "the import directory at RVA 0x7ffff000 runs out of the file");
    ExpectRefused(patched32(descriptor + 12, 0x7FFFFF00), "names its DLL at RVA 0x7fffff00");
    ExpectRefused(patched32(descriptor + 16, 0), "entry for KERNEL32.dll has no import address table");
    ExpectRefused(patched32(descriptor, 0x7FFFFF00), "thunks for KERNEL32.dll run out of the file at RVA 0x7fffff00");
    ExpectRefused(patched32(descriptor + 16, sample.SizeOfImage - 4), "slot 0 for KERNEL32.dll");
    ExpectRefused(patched64(lookup, 0x7FFFFF00), "thunk 0 for KERNEL32.dll names a function at RVA 0x7fffff00");
    ExpectRefused(patched64(lookup + 8, 0x100000000ull | Get32(sample.Bytes, lookup + 8)), "thunk 1 for KERNEL32.dll is a malformed name reference");
    ExpectRefused(patched64(lookup, 0x8000000000010073ull), "thunk 0 for KERNEL32.dll is a malformed ordinal");

    std::vector<uint8> unterminated = sample.Bytes;
    uint32 const tail = static_cast<uint32>(sample.Bytes.size()) - 40;
    std::optional<uint32> const rva = [&sample, tail]() -> std::optional<uint32>
    {
        std::string error;
        std::unique_ptr<PeImage> const image = PeImage::Parse(sample.Bytes, error);
        return image ? image->OffsetToRva(tail) : std::nullopt;
    }();
    ASSERT_TRUE(rva);
    std::copy(sample.Bytes.begin() + static_cast<std::ptrdiff_t>(descriptor), sample.Bytes.begin() + static_cast<std::ptrdiff_t>(descriptor + 40), unterminated.begin() + static_cast<std::ptrdiff_t>(tail));
    Put32(unterminated, DirectoryField(ImportDirectory), *rva);
    ExpectRefused(unterminated, fmt::format("the import directory at RVA {:#x} runs out of the file at RVA {:#x} before its terminating entry", *rva, *rva + 40));
}

TEST(PeImageTest, DescriptorsSharingOneThunkArrayAreRefused)
{
    static constexpr std::size_t Descriptors = 300;
    static constexpr std::size_t Thunks = 1000;
    auto const lookupShared = [](std::size_t descriptors, std::size_t thunks, std::string const& dll, bool byOrdinal)
    {
        uint32 const tableSize = static_cast<uint32>((thunks + 1) * 8);
        return BuildImportImage(static_cast<uint32>(descriptors * tableSize), descriptors, [descriptors, thunks, tableSize, &dll, byOrdinal](ImportSection& section, uint32 addressTables)
        {
            uint32 const name = section.AddText(dll);
            uint64 const thunk = byOrdinal ? 0x8000000000000001ull : section.AddHintName("GetTickCount");
            uint32 const lookup = section.AddThunks(thunk, thunks);
            for (std::size_t index = 0; index < descriptors; ++index)
                section.SetDescriptor(index, lookup, name, addressTables + static_cast<uint32>(index * tableSize));
        });
    };

    std::unique_ptr<PeImage> const fits = ParseOk(lookupShared(2, 4, "KERNEL32.dll", false));
    ASSERT_NE(fits, nullptr);
    ASSERT_EQ(fits->GetImports().size(), 8u);
    EXPECT_EQ(fits->GetImports()[4].SlotRva, fits->GetImports()[0].SlotRva + 40);
    EXPECT_EQ(fits->GetImports().back().Name, "GetTickCount");

    std::vector<uint8> const byOrdinal = lookupShared(Descriptors, Thunks, "W.dll", true);
    ExpectRefusedQuickly(byOrdinal, fmt::format("the import directory lists more than {} imports, more than the {}-byte file can hold", byOrdinal.size() / 8, byOrdinal.size()));

    std::vector<uint8> const byName = lookupShared(Descriptors, Thunks, "KERNEL32.dll", false);
    ExpectRefusedQuickly(byName, fmt::format("the import directory's names for KERNEL32.dll repeat past the size of the {}-byte file", byName.size()));

    uint32 table = 0;
    std::vector<uint8> const tableShared = BuildImportImage(0x10, Descriptors, [&table](ImportSection& section, uint32)
    {
        uint32 const dll = section.AddText("W.dll");
        table = section.AddThunks(0x8000000000000001ull, Thunks);
        for (std::size_t index = 0; index < Descriptors; ++index)
            section.SetDescriptor(index, 0, dll, table);
    });
    ExpectRefusedQuickly(tableShared, fmt::format("the import directory's slot 0 for W.dll at RVA {:#x} overlaps the {}-slot import address table for W.dll at RVA {:#x}", table, Thunks, table));
}

TEST(PeImageTest, NamesRepeatedPastTheFileSizeAreRefused)
{
    std::string const longName(4096, 'N');
    auto const repeated = [&longName](std::size_t thunks)
    {
        return BuildImportImage(static_cast<uint32>((thunks + 1) * 8), 1, [&longName, thunks](ImportSection& section, uint32 addressTables)
        {
            uint32 const dll = section.AddText("KERNEL32.dll");
            uint32 const lookup = section.AddThunks(section.AddHintName(longName), thunks);
            section.SetDescriptor(0, lookup, dll, addressTables);
        });
    };

    std::unique_ptr<PeImage> const fits = ParseOk(repeated(1));
    ASSERT_NE(fits, nullptr);
    ASSERT_EQ(fits->GetImports().size(), 1u);
    EXPECT_EQ(fits->GetImports()[0].Name, longName);
    ExpectRefused(repeated(2), "the import directory's names for KERNEL32.dll repeat past the size of the");

    std::vector<uint8> const thousands = repeated(5000);
    ASSERT_GT(thousands.size() / 8, 5000u);
    ExpectRefusedQuickly(thousands, fmt::format("the import directory's names for KERNEL32.dll repeat past the size of the {}-byte file", thousands.size()));

    std::string const longDll = std::string(4000, 'D') + ".dll";
    static constexpr std::size_t Descriptors = 2000;
    std::vector<uint8> const emptyDescriptors = BuildImportImage(0x10, Descriptors, [&longDll](ImportSection& section, uint32 addressTables)
    {
        uint32 const dll = section.AddText(longDll);
        uint32 const lookup = section.AddThunks(0, 0);
        for (std::size_t index = 0; index < Descriptors; ++index)
            section.SetDescriptor(index, lookup, dll, addressTables);
    });
    ExpectRefusedQuickly(emptyDescriptors, fmt::format("the import directory's names for {} repeat past the size of the {}-byte file", longDll, emptyDescriptors.size()));
}

TEST(PeImageTest, OverlappingImportAddressTablesAreRefused)
{
    uint32 base = 0;
    std::unique_ptr<PeImage> const adjacent = ParseOk(BuildAddressTables({ { "A.dll", 2, 0 }, { "B.dll", 2, 16 }, { "C.dll", 1, 0x40 }, { "D.dll", 0, 8 } }, base));
    ASSERT_NE(adjacent, nullptr);
    std::vector<PeImport> const& imports = adjacent->GetImports();
    ASSERT_EQ(imports.size(), 5u);
    EXPECT_EQ(imports[0].SlotRva, base);
    EXPECT_EQ(imports[1].SlotRva, base + 8);
    EXPECT_EQ(imports[2].Dll, "B.dll");
    EXPECT_EQ(imports[2].SlotRva, base + 16);
    EXPECT_EQ(imports[3].SlotRva, base + 24);
    EXPECT_EQ(imports[4].Dll, "C.dll");
    EXPECT_EQ(imports[4].SlotRva, base + 0x40);

    std::vector<uint8> const inside = BuildAddressTables({ { "A.dll", 3, 0 }, { "B.dll", 2, 8 } }, base);
    ExpectRefused(inside, fmt::format("the import directory's slot 0 for B.dll at RVA {:#x} overlaps the 3-slot import address table for A.dll at RVA {:#x}", base + 8, base));

    std::vector<uint8> const sameStart = BuildAddressTables({ { "A.dll", 1, 0 }, { "B.dll", 1, 0 } }, base);
    ExpectRefused(sameStart, fmt::format("the import directory's slot 0 for B.dll at RVA {:#x} overlaps the 1-slot import address table for A.dll at RVA {:#x}", base, base));

    std::vector<uint8> const runsInto = BuildAddressTables({ { "A.dll", 2, 16 }, { "B.dll", 3, 0 } }, base);
    ExpectRefused(runsInto, fmt::format("the import directory's slot 2 for B.dll at RVA {:#x} overlaps the 2-slot import address table for A.dll at RVA {:#x}", base + 16, base + 16));

    std::vector<uint8> const misaligned = BuildAddressTables({ { "A.dll", 1, 12 }, { "B.dll", 2, 0 } }, base);
    ExpectRefused(misaligned, fmt::format("the import directory's slot 1 for B.dll at RVA {:#x} overlaps the 1-slot import address table for A.dll at RVA {:#x}", base + 8, base + 12));

    std::vector<uint8> const between = BuildAddressTables({ { "A.dll", 2, 0 }, { "C.dll", 2, 0x40 }, { "B.dll", 6, 24 } }, base);
    ExpectRefused(between, fmt::format("the import directory's slot 5 for B.dll at RVA {:#x} overlaps the 2-slot import address table for C.dll at RVA {:#x}", base + 0x40, base + 0x40));
}

TEST(PeImageTest, HostileRelocationsExceptionsAndTlsAreRefused)
{
    Sample const sample = BuildSample();
    std::size_t const relocations = DirectoryOffset(sample.Bytes, RelocationDirectory);
    std::size_t const exceptions = DirectoryOffset(sample.Bytes, ExceptionDirectory);
    ASSERT_NE(relocations, 0u);
    ASSERT_NE(exceptions, 0u);
    auto const patched = [&sample](std::size_t offset, uint32 value)
    {
        std::vector<uint8> bytes = sample.Bytes;
        Put32(bytes, offset, value);
        return bytes;
    };
    uint32 const relocationSize = Get32(sample.Bytes, DirectoryField(RelocationDirectory) + 4);
    uint32 const firstBlock = Get32(sample.Bytes, relocations + 4);

    ExpectRefused(patched(DirectoryField(RelocationDirectory), 0x7FFFF000), "base relocation directory at RVA 0x7ffff000");
    ExpectRefused(patched(relocations + 4, 4), "bad size of 4 bytes");
    ExpectRefused(patched(relocations + 4, 0), "bad size of 0 bytes");
    ExpectRefused(patched(relocations + 4, 13), "bad size of 13 bytes");
    ExpectRefused(patched(relocations + 4, relocationSize + 2), fmt::format("bad size of {} bytes", relocationSize + 2));
    ExpectRefused(patched(relocations, sample.SizeOfImage), "lies outside the");
    ExpectRefused(patched(relocations, sample.SizeOfImage - 0xC), fmt::format("type 10 at RVA {:#x} lies outside the", sample.SizeOfImage - 4));
    ExpectRefused(patched(DirectoryField(RelocationDirectory) + 4, relocationSize + 3), "ends with 3 stray bytes");
    ExpectRefused(patched(DirectoryField(RelocationDirectory) + 4, firstBlock + 6), "ends with 6 stray bytes");

    ExpectRefused(patched(DirectoryField(ExceptionDirectory) + 4, 13), "exception table is 13 bytes");
    ExpectRefused(patched(DirectoryField(ExceptionDirectory), 0x7FFFF000), "exception table at RVA 0x7ffff000");
    ExpectRefused(patched(exceptions + 4, sample.Text), "entry 0");
    ExpectRefused(patched(exceptions + 12 + 4, sample.SizeOfImage + 1), "entry 1");
    ExpectRefused(patched(exceptions + 24 + 8, sample.SizeOfImage), "entry 2");

    ExpectRefused(patched(DirectoryField(TlsDirectory), 0x7FFFF000), "TLS directory at RVA 0x7ffff000 is not inside the file");
}
