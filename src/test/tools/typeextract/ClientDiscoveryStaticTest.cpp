/*
 * Project Ambrose by Imjustchico
 * Tests static client discovery over hand-assembled PeBuilder programs: the C and C++ initializer tables behind _initterm_e and _initterm thunk and slot calls with its filters and ambiguity errors, the race adder from the RaceManager strings, and the callers of a function; with AMBROSE_CLIENT_DIR set, runs the same discovery on the user's own client and prints what it found and how long it took.
 */

#include "ClientDiscovery.h"
#include "CodeIndex.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "PeBuilder.h"
#include "PeImage.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr uint64 Base = 0x140000000;
    constexpr uint32 Text = 0x1000;
    constexpr uint32 Rdata = 0x2000;
    constexpr uint32 Data = 0x3000;
    constexpr char const* RuntimeDll = "api-ms-win-crt-runtime-l1-1-0.dll";

    class CodeBuffer
    {
    public:
        CodeBuffer(uint32 rva, std::size_t size) : _rva(rva), _bytes(size, 0xCC)
        {
        }

        void Put(uint32 at, std::initializer_list<uint8> bytes)
        {
            std::size_t offset = at - _rva;
            for (uint8 const byte : bytes)
                _bytes.at(offset++) = byte;
        }

        void Displacement(uint32 at, uint32 next, uint32 target)
        {
            uint32 const value = target - next;
            Put(at, { static_cast<uint8>(value), static_cast<uint8>(value >> 8), static_cast<uint8>(value >> 16), static_cast<uint8>(value >> 24) });
        }

        void Lea(uint32 at, uint8 rex, uint8 modrm, uint32 target)
        {
            Put(at, { rex, 0x8D, modrm });
            Displacement(at + 3, at + 7, target);
        }

        void LeaRcx(uint32 at, uint32 target) { Lea(at, 0x48, 0x0D, target); }
        void LeaRdx(uint32 at, uint32 target) { Lea(at, 0x48, 0x15, target); }

        void Call(uint32 at, uint32 target)
        {
            Put(at, { 0xE8 });
            Displacement(at + 1, at + 5, target);
        }

        void CallSlot(uint32 at, uint32 slot)
        {
            Put(at, { 0xFF, 0x15 });
            Displacement(at + 2, at + 6, slot);
        }

        void JumpSlot(uint32 at, uint32 slot)
        {
            Put(at, { 0xFF, 0x25 });
            Displacement(at + 2, at + 6, slot);
        }

        std::vector<uint8> const& Bytes() const noexcept { return _bytes; }

    private:
        uint32 _rva;
        std::vector<uint8> _bytes;
    };

    struct Slots
    {
        uint32 Initterm = 0;
        uint32 InittermE = 0;
    };

    struct ProgramSpec
    {
        std::function<void(CodeBuffer&, Slots const&)> Emit;
        std::vector<std::pair<uint32, uint32>> Functions;
        std::vector<std::array<uint32, 4>> Chained;
        std::vector<uint8> Rdata = std::vector<uint8>(0x100, 0);
        bool ImportInitterm = true;
    };

    struct Program
    {
        std::unique_ptr<PeImage> Image;
        std::unique_ptr<CodeIndex> Code;
    };

    std::vector<uint8> BuildBytes(ProgramSpec const& spec, Slots const& slots, Slots& learned)
    {
        CodeBuffer code(Text, 0x200);
        spec.Emit(code, slots);
        PeBuilder builder(Base);
        EXPECT_EQ(builder.AddSection(".text", code.Bytes(), PeBuilder::CodeCharacteristics), Text);
        EXPECT_EQ(builder.AddSection(".rdata", spec.Rdata, PeBuilder::ReadOnlyCharacteristics), Rdata);
        EXPECT_EQ(builder.AddSection(".data", std::vector<uint8>(0x100, 0), PeBuilder::DataCharacteristics), Data);
        builder.AddImport(RuntimeDll, "_exit");
        if (spec.ImportInitterm)
        {
            builder.AddImport(RuntimeDll, "_initterm_e");
            builder.AddImport(RuntimeDll, "_initterm");
        }
        for (auto const& [begin, end] : spec.Functions)
            builder.AddFunction(Text + begin, Text + end);
        for (std::array<uint32, 4> const& chained : spec.Chained)
            builder.AddChainedFunction(Text + chained[0], Text + chained[1], Text + chained[2], Text + chained[3]);
        std::vector<uint8> bytes = builder.Build();
        if (spec.ImportInitterm)
        {
            learned.Initterm = builder.ImportSlotRva(RuntimeDll, "_initterm");
            learned.InittermE = builder.ImportSlotRva(RuntimeDll, "_initterm_e");
        }
        return bytes;
    }

    Program BuildProgram(ProgramSpec const& spec)
    {
        Slots slots;
        BuildBytes(spec, Slots{}, slots);
        Slots unused;
        Program program;
        std::string error;
        program.Image = PeImage::Parse(BuildBytes(spec, slots, unused), error);
        EXPECT_NE(program.Image, nullptr) << error;
        if (program.Image != nullptr)
            program.Code = std::make_unique<CodeIndex>(*program.Image);
        return program;
    }

    uint32 Startup(CodeBuffer& code, uint32 at, uint32 first, uint32 last, uint32 thunk)
    {
        code.Put(at, { 0x48, 0x83, 0xEC, 0x28 });
        code.LeaRdx(at + 0x04, last);
        code.LeaRcx(at + 0x0B, first);
        code.Call(at + 0x12, thunk);
        code.Put(at + 0x17, { 0x48, 0x83, 0xC4, 0x28, 0xC3 });
        return at + 0x1C;
    }

    uint32 SlotStartup(CodeBuffer& code, uint32 at, uint32 first, uint32 last, uint32 slot)
    {
        code.Put(at, { 0x48, 0x83, 0xEC, 0x28 });
        code.LeaRcx(at + 0x04, first);
        code.LeaRdx(at + 0x0B, last);
        code.CallSlot(at + 0x12, slot);
        code.Put(at + 0x18, { 0x48, 0x83, 0xC4, 0x28, 0xC3 });
        return at + 0x1D;
    }

    std::optional<InitializerTable> FindTable(Program const& program, std::string& error)
    {
        if (program.Image == nullptr || program.Code == nullptr)
            return std::nullopt;
        return ClientDiscovery::FindInitializerTable(*program.Image, *program.Code, error);
    }

    void PutText(std::vector<uint8>& bytes, std::size_t offset, std::string_view text)
    {
        std::memcpy(bytes.data() + offset, text.data(), text.size());
    }

    std::vector<uint8> RaceRdata(bool withEnum)
    {
        std::vector<uint8> rdata(0x100, 0);
        PutText(rdata, 0x10, "RaceManager::InitializeRaces");
        PutText(rdata, 0x40, "xenum eRace");
        if (withEnum)
        {
            PutText(rdata, 0x01, "enum eRace");
            PutText(rdata, 0x30, "enum eRace");
        }
        return rdata;
    }

    ProgramSpec RaceSpec(bool withEnum, bool secondAdder, bool withAdder)
    {
        ProgramSpec spec;
        spec.Rdata = RaceRdata(withEnum);
        spec.ImportInitterm = false;
        spec.Emit = [secondAdder, withAdder](CodeBuffer& code, Slots const&)
        {
            code.LeaRcx(Text + 0x00, Rdata + 0x10);
            code.LeaRdx(Text + 0x07, Rdata + 0x01);
            code.Put(Text + 0x0E, { 0xC3 });
            if (withAdder)
            {
                code.LeaRdx(Text + 0x20, Rdata + 0x30);
                code.Put(Text + 0x27, { 0xC3 });
                code.Lea(Text + 0x80, 0x4C, 0x05, Rdata + 0x01);
                code.Put(Text + 0x87, { 0xC3 });
            }
            code.LeaRcx(Text + 0x40, Rdata + 0x41);
            code.Put(Text + 0x47, { 0xC3 });
            if (secondAdder)
            {
                code.LeaRcx(Text + 0x60, Rdata + 0x30);
                code.Put(Text + 0x67, { 0xC3 });
            }
            code.LeaRcx(Text + 0xC0, Rdata + 0x01);
        };
        spec.Functions = { { 0x00, 0x20 }, { 0x20, 0x40 }, { 0x40, 0x60 }, { 0x60, 0x80 } };
        spec.Chained = { { 0x80, 0x90, 0x20, 0x40 } };
        return spec;
    }
}

TEST(ClientDiscoveryStaticTest, FindsTheTableLoadedBeforeTheInittermThunkCall)
{
    ProgramSpec spec;
    spec.Emit = [](CodeBuffer& code, Slots const& slots)
    {
        code.JumpSlot(Text, slots.Initterm);
        Startup(code, Text + 0x10, Rdata + 0x10, Rdata + 0x28, Text);
        SlotStartup(code, Text + 0x40, Rdata + 0x40, Rdata + 0x58, slots.InittermE);
    };
    spec.Functions = { { 0x10, 0x2C }, { 0x40, 0x5D } };
    Program const program = BuildProgram(spec);
    std::string error;
    std::optional<InitializerTable> const table = FindTable(program, error);
    ASSERT_TRUE(table) << error;
    EXPECT_EQ(table->Begin, Base + Rdata + 0x10);
    EXPECT_EQ(table->End, Base + Rdata + 0x28);
    EXPECT_EQ(table->Count(), 3u);
    std::optional<InitializerTable> const cTable = ClientDiscovery::FindCInitializerTable(*program.Image, *program.Code, error);
    ASSERT_TRUE(cTable) << error;
    EXPECT_EQ(cTable->Begin, Base + Rdata + 0x40);
    EXPECT_EQ(cTable->End, Base + Rdata + 0x58);
}

TEST(ClientDiscoveryStaticTest, FindsTheTableBeforeADirectCallThroughTheSlot)
{
    ProgramSpec spec;
    spec.Emit = [](CodeBuffer& code, Slots const& slots) { SlotStartup(code, Text + 0x10, Rdata + 0x08, Rdata + 0x10, slots.Initterm); };
    spec.Functions = { { 0x10, 0x2D } };
    Program const program = BuildProgram(spec);
    std::string error;
    std::optional<InitializerTable> const table = FindTable(program, error);
    ASSERT_TRUE(table) << error;
    EXPECT_EQ(table->Begin, Base + Rdata + 0x08);
    EXPECT_EQ(table->Count(), 1u);
}

TEST(ClientDiscoveryStaticTest, OneTableLoadedAtSeveralCallSitesIsOneTable)
{
    ProgramSpec spec;
    spec.Emit = [](CodeBuffer& code, Slots const& slots)
    {
        code.JumpSlot(Text, slots.Initterm);
        Startup(code, Text + 0x10, Rdata + 0x10, Rdata + 0x28, Text);
        Startup(code, Text + 0x40, Rdata + 0x10, Rdata + 0x28, Text);
        SlotStartup(code, Text + 0x80, Rdata + 0x10, Rdata + 0x28, slots.Initterm);
    };
    spec.Functions = { { 0x10, 0x2C }, { 0x40, 0x5C }, { 0x80, 0x9D } };
    Program const program = BuildProgram(spec);
    std::string error;
    std::optional<InitializerTable> const table = FindTable(program, error);
    ASSERT_TRUE(table) << error;
    EXPECT_EQ(table->Begin, Base + Rdata + 0x10);
    EXPECT_EQ(table->End, Base + Rdata + 0x28);
}

TEST(ClientDiscoveryStaticTest, TwoTablesAreAnError)
{
    ProgramSpec spec;
    spec.Emit = [](CodeBuffer& code, Slots const& slots)
    {
        code.JumpSlot(Text, slots.Initterm);
        Startup(code, Text + 0x10, Rdata + 0x10, Rdata + 0x28, Text);
        Startup(code, Text + 0x40, Rdata + 0x40, Rdata + 0x60, Text);
    };
    spec.Functions = { { 0x10, 0x2C }, { 0x40, 0x5C } };
    Program const program = BuildProgram(spec);
    std::string error;
    EXPECT_FALSE(FindTable(program, error));
    EXPECT_EQ(error, "found 2 C++ initializer tables where one was expected: 0x140002010-0x140002028 (3 entries), 0x140002040-0x140002060 (4 entries)");
}

TEST(ClientDiscoveryStaticTest, ACallWithoutBothTableBoundsIsNoTable)
{
    ProgramSpec spec;
    spec.Emit = [](CodeBuffer& code, Slots const& slots)
    {
        code.JumpSlot(Text, slots.Initterm);
        code.LeaRcx(Text + 0x10, Rdata + 0x10);
        code.Call(Text + 0x17, Text);
        code.Put(Text + 0x1C, { 0xC3 });
    };
    spec.Functions = { { 0x10, 0x1D } };
    Program const program = BuildProgram(spec);
    std::string error;
    EXPECT_FALSE(FindTable(program, error));
    EXPECT_EQ(error, "none of the 1 calls through the program's 1 _initterm imports loads a C++ initializer table into rcx and rdx");
}

TEST(ClientDiscoveryStaticTest, CandidatesMustBeOrderedAlignedInOneSectionAndInTheWindow)
{
    ProgramSpec spec;
    spec.Emit = [](CodeBuffer& code, Slots const& slots)
    {
        code.JumpSlot(Text, slots.Initterm);
        Startup(code, Text + 0x10, Rdata + 0x28, Rdata + 0x10, Text);
        Startup(code, Text + 0x40, Rdata + 0x10, Rdata + 0x2C, Text);
        Startup(code, Text + 0x70, Rdata + 0xF8, Data + 0x08, Text);
        code.Put(Text + 0xA0, { 0x48, 0x83, 0xEC, 0x28 });
        code.LeaRdx(Text + 0xA4, Rdata + 0x28);
        code.LeaRcx(Text + 0xAB, Rdata + 0x10);
        code.Put(Text + 0xB2, { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 });
        code.Call(Text + 0xBA, Text);
        code.Put(Text + 0xBF, { 0xC3 });
        Startup(code, Text + 0xE0, Rdata + 0x10, Rdata + 0x28, Text);
    };
    spec.Functions = { { 0x10, 0x2C }, { 0x40, 0x5C }, { 0x70, 0x8C }, { 0xA0, 0xC0 } };
    Program const program = BuildProgram(spec);
    std::string error;
    EXPECT_FALSE(FindTable(program, error));
    EXPECT_EQ(error, "none of the 5 calls through the program's 1 _initterm imports loads a C++ initializer table into rcx and rdx");
}

TEST(ClientDiscoveryStaticTest, TheLatestLeaIntoEachRegisterWins)
{
    ProgramSpec spec;
    spec.Emit = [](CodeBuffer& code, Slots const& slots)
    {
        code.JumpSlot(Text, slots.Initterm);
        code.Put(Text + 0x10, { 0x48, 0x83, 0xEC, 0x28 });
        code.LeaRcx(Text + 0x14, Rdata + 0x08);
        code.LeaRdx(Text + 0x1B, Rdata + 0x28);
        code.LeaRcx(Text + 0x22, Rdata + 0x10);
        code.Call(Text + 0x29, Text);
        code.Put(Text + 0x2E, { 0x48, 0x83, 0xC4, 0x28, 0xC3 });
    };
    spec.Functions = { { 0x10, 0x33 } };
    Program const program = BuildProgram(spec);
    std::string error;
    std::optional<InitializerTable> const table = FindTable(program, error);
    ASSERT_TRUE(table) << error;
    EXPECT_EQ(table->Begin, Base + Rdata + 0x10);
    EXPECT_EQ(table->End, Base + Rdata + 0x28);
}

TEST(ClientDiscoveryStaticTest, AProgramWithoutAnInittermImportIsAnError)
{
    ProgramSpec spec;
    spec.ImportInitterm = false;
    spec.Emit = [](CodeBuffer& code, Slots const& slots)
    {
        code.JumpSlot(Text, slots.Initterm);
        Startup(code, Text + 0x10, Rdata + 0x10, Rdata + 0x28, Text);
    };
    spec.Functions = { { 0x10, 0x2C } };
    Program const program = BuildProgram(spec);
    std::string error;
    EXPECT_FALSE(FindTable(program, error));
    EXPECT_EQ(error, "the program does not import _initterm, so its C++ initializer table cannot be found");
    EXPECT_FALSE(ClientDiscovery::FindCInitializerTable(*program.Image, *program.Code, error));
    EXPECT_EQ(error, "the program does not import _initterm_e, so its C initializer table cannot be found");
}

TEST(ClientDiscoveryStaticTest, TheRaceAdderReferencesTheEnumOutsideTheInitializer)
{
    Program const program = BuildProgram(RaceSpec(true, false, true));
    ASSERT_NE(program.Code, nullptr);
    std::string error;
    std::optional<uint64> const adder = ClientDiscovery::FindRaceAdder(*program.Code, error);
    ASSERT_TRUE(adder) << error;
    EXPECT_EQ(*adder, Base + Text + 0x20);
}

TEST(ClientDiscoveryStaticTest, NoOrSeveralRaceAddersAreErrors)
{
    Program const none = BuildProgram(RaceSpec(true, false, false));
    ASSERT_NE(none.Code, nullptr);
    std::string noneError;
    EXPECT_FALSE(ClientDiscovery::FindRaceAdder(*none.Code, noneError));
    EXPECT_EQ(noneError, "found no race adder: every function referencing \"enum eRace\" (0x140001000) also references \"RaceManager::InitializeRaces\"");

    Program const two = BuildProgram(RaceSpec(true, true, true));
    ASSERT_NE(two.Code, nullptr);
    std::string twoError;
    EXPECT_FALSE(ClientDiscovery::FindRaceAdder(*two.Code, twoError));
    EXPECT_EQ(twoError, "found 2 race adders where one was expected: 0x140001020, 0x140001060");

    Program const missing = BuildProgram(RaceSpec(false, false, true));
    ASSERT_NE(missing.Code, nullptr);
    std::string missingError;
    EXPECT_FALSE(ClientDiscovery::FindRaceAdder(*missing.Code, missingError));
    EXPECT_EQ(missingError, "the program holds no \"enum eRace\" string, so its race adder cannot be found");

    ProgramSpec unreferenced = RaceSpec(true, false, false);
    unreferenced.Emit = [](CodeBuffer& code, Slots const&) { code.Put(Text, { 0xC3 }); };
    Program const nobody = BuildProgram(unreferenced);
    ASSERT_NE(nobody.Code, nullptr);
    std::string nobodyError;
    EXPECT_FALSE(ClientDiscovery::FindRaceAdder(*nobody.Code, nobodyError));
    EXPECT_EQ(nobodyError, "found no race adder: no function references \"enum eRace\"");
}

TEST(ClientDiscoveryStaticTest, FunctionsCallingListsEachCallingFunctionOnce)
{
    ProgramSpec spec;
    spec.ImportInitterm = false;
    spec.Emit = [](CodeBuffer& code, Slots const&)
    {
        code.Call(Text + 0x00, Text + 0x60);
        code.Call(Text + 0x05, Text + 0x60);
        code.Put(Text + 0x0A, { 0xC3 });
        code.Call(Text + 0x20, Text + 0x60);
        code.Put(Text + 0x25, { 0xC3 });
        code.Put(Text + 0x60, { 0xC3 });
        code.Call(Text + 0x80, Text + 0x60);
        code.Call(Text + 0xC0, Text + 0x60);
    };
    spec.Functions = { { 0x00, 0x20 }, { 0x20, 0x40 }, { 0x60, 0x70 } };
    spec.Chained = { { 0x80, 0x90, 0x00, 0x20 } };
    Program const program = BuildProgram(spec);
    ASSERT_NE(program.Code, nullptr);
    EXPECT_EQ(ClientDiscovery::FunctionsCalling(*program.Code, Base + Text + 0x60), (std::vector<uint64>{ Base + Text, Base + Text + 0x20 }));
    EXPECT_TRUE(ClientDiscovery::FunctionsCalling(*program.Code, Base + Text + 0x61).empty());
    EXPECT_TRUE(ClientDiscovery::FunctionsCalling(*program.Code, 0).empty());
}

TEST(TypeExtractionClientTest, StaticDiscoveryFindsTheInitializerTableAndRaceAdder)
{
    std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
    if (!client || client->empty())
        GTEST_SKIP() << "AMBROSE_CLIENT_DIR is not set";
    std::filesystem::path const executable = ConfigMgr::PathFromUtf8(*client) / "Bin" / "WizardGraphicalClient.exe";
    using Clock = std::chrono::steady_clock;
    auto const milliseconds = [](Clock::duration duration) { return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(); };

    Clock::time_point const started = Clock::now();
    std::string error;
    std::unique_ptr<PeImage> const image = PeImage::Load(executable, error);
    Clock::time_point const loaded = Clock::now();
    ASSERT_NE(image, nullptr) << error;
    EXPECT_EQ(image->GetMachine(), PeImage::MachineAmd64);
    EXPECT_GT(image->GetFunctions().size(), 100000u);

    std::vector<uint32> const strings = image->FindTerminatedString("enum eRace");
    Clock::time_point const scanned = Clock::now();
    EXPECT_FALSE(strings.empty());

    CodeIndex const code(*image);
    Clock::time_point const indexed = Clock::now();

    std::optional<InitializerTable> const table = ClientDiscovery::FindInitializerTable(*image, code, error);
    Clock::time_point const tableFound = Clock::now();
    ASSERT_TRUE(table) << error;
    EXPECT_GT(table->Count(), 10000u);

    std::optional<uint64> const adder = ClientDiscovery::FindRaceAdder(code, error);
    Clock::time_point const adderFound = Clock::now();
    ASSERT_TRUE(adder) << error;

    std::cout << fmt::format("[ CLIENT   ] {}: {} bytes, image base {:#x}, {} functions, {} imports, {} exports, parsed in {} ms", ConfigMgr::PathToUtf8(executable), image->GetBytes().size(), image->GetImageBase(), image->GetFunctions().size(), image->GetImports().size(), image->GetExports().size(), milliseconds(loaded - started)) << std::endl;
    std::cout << fmt::format("[ CLIENT   ] string scan for \"enum eRace\" found {} in {} ms; code index built in {} ms", strings.size(), milliseconds(scanned - loaded), milliseconds(indexed - scanned)) << std::endl;
    std::cout << fmt::format("[ CLIENT   ] initializer table {:#x}-{:#x} ({} entries) found in {} ms", table->Begin, table->End, table->Count(), milliseconds(tableFound - indexed)) << std::endl;
    std::cout << fmt::format("[ CLIENT   ] race adder {:#x} found in {} ms", *adder, milliseconds(adderFound - tableFound)) << std::endl;
}

TEST(TypeExtractionClientTest, TheInstallsRuntimeLibrariesParse)
{
    std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
    if (!client || client->empty())
        GTEST_SKIP() << "AMBROSE_CLIENT_DIR is not set";
    std::filesystem::path const bin = ConfigMgr::PathFromUtf8(*client) / "Bin";
    std::array<std::string_view, 7> const names = { "ucrtbase.dll", "vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll", "concrt140.dll", "api-ms-win-crt-runtime-l1-1-0.dll", "api-ms-win-crt-stdio-l1-1-0.dll" };
    std::size_t parsed = 0;
    for (std::string_view const name : names)
    {
        std::filesystem::path const path = bin / std::string(name);
        std::error_code ignored;
        if (!std::filesystem::is_regular_file(path, ignored))
            continue;
        std::string error;
        std::unique_ptr<PeImage> const image = PeImage::Load(path, error);
        EXPECT_NE(image, nullptr) << error;
        if (image == nullptr)
            continue;
        ++parsed;
        EXPECT_EQ(image->GetMachine(), PeImage::MachineAmd64) << name;
        EXPECT_TRUE(image->IsDll()) << name;
        EXPECT_FALSE(image->GetExports().empty()) << name;
        if (name == "ucrtbase.dll")
        {
            PeExport const* const initterm = image->FindExport("_initterm");
            ASSERT_NE(initterm, nullptr);
            EXPECT_NE(initterm->Rva, 0u);
            EXPECT_TRUE(initterm->Forwarder.empty());
            EXPECT_FALSE(image->GetFunctions().empty());
            EXPECT_FALSE(image->GetRelocations().empty());
        }
        if (name == "api-ms-win-crt-runtime-l1-1-0.dll")
        {
            PeExport const* const initterm = image->FindExport("_initterm");
            ASSERT_NE(initterm, nullptr);
            EXPECT_EQ(initterm->Forwarder, "ucrtbase._initterm");
            EXPECT_EQ(initterm->Rva, 0u);
        }
        std::cout << fmt::format("[ CLIENT   ] {}: image base {:#x}, {} exports, {} imports, {} relocations, {} functions", name, image->GetImageBase(), image->GetExports().size(), image->GetImports().size(), image->GetRelocations().size(), image->GetFunctions().size()) << std::endl;
    }
    EXPECT_GT(parsed, 0u);
}
