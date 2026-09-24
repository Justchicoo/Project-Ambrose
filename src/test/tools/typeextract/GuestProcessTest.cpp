/*
 * Project Ambrose by Imjustchico
 * Tests the guest process loader on synthetic images written to a folder: a main program importing runtime, forwarded, kernel and other functions, a relocated runtime DLL whose TLS callback and entry point run on attach, stub calls dispatched to handlers or counted as unhandled, handler failures reported under the API's name, a refusing entry point, folders and files named outside the ANSI code page, missing runtime DLLs and exports named with what needs them, the api-ms-win-crt fallback to ucrtbase, and address descriptions.
 */

#include "ClientLocator.h"
#include "GuestProcess.h"
#include "LogTestDirectory.h"
#include "PeBuilder.h"

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    void Save(std::filesystem::path const& path, std::vector<uint8> const& bytes)
    {
        std::ofstream(path, std::ios::binary).write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    void PutDisp(std::vector<uint8>& code, std::size_t at, int64 value)
    {
        uint32 const disp = static_cast<uint32>(static_cast<int32>(value));
        for (int i = 0; i < 4; ++i)
            code[at + static_cast<std::size_t>(i)] = static_cast<uint8>(disp >> (8 * i));
    }

    std::vector<uint8> RuntimeDll(bool refuse)
    {
        constexpr uint64 PreferredBase = 0x10000000;
        PeBuilder builder(PreferredBase, true);
        std::vector<uint8> code = {
            0x48, 0x8D, 0x04, 0x11, 0xC3,
            0xC7, 0x05, 0, 0, 0, 0, 0x2A, 0, 0, 0, 0xC3,
            0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3,
            0x31, 0xC0, 0xC3,
            0x48, 0x8B, 0x05, 0, 0, 0, 0, 0xC3
        };
        uint32 const codeRva = builder.NextSectionRva();
        uint32 const dataRva = codeRva + 0x1000;
        PutDisp(code, 7, static_cast<int64>(dataRva + 0x18) - static_cast<int64>(codeRva + 15));
        PutDisp(code, 28, static_cast<int64>(dataRva + 0x20) - static_cast<int64>(codeRva + 32));
        builder.AddSection(".text", code, PeBuilder::CodeCharacteristics);
        std::vector<uint8> data(0x40, 0);
        uint64 const pointer = PreferredBase + dataRva + 0x08;
        for (int i = 0; i < 8; ++i)
            data[0x20 + static_cast<std::size_t>(i)] = static_cast<uint8>(pointer >> (8 * i));
        builder.AddSection(".data", data, PeBuilder::DataCharacteristics);
        builder.AddRelocation(dataRva + 0x20);
        builder.SetTls(dataRva, 0x08, dataRva + 0x10, { codeRva + 5 });
        builder.SetEntryPoint(refuse ? codeRva + 22 : codeRva + 16);
        builder.SetExportName("ucrtbase.dll");
        builder.AddExport("Add", codeRva);
        builder.AddExport("DataPointer", codeRva + 25);
        builder.AddForwarder("Sum", "ucrtbase.Add");
        return builder.Build();
    }

    std::vector<uint8> ForwarderDll(std::string const& name = "api-ms-win-crt-math-l1-1-0.dll", std::string const& function = "Add", std::string const& target = "ucrtbase.Add")
    {
        PeBuilder builder(0x180000000, true);
        builder.AddSection(".text", { 0xC3 }, PeBuilder::CodeCharacteristics);
        builder.SetExportName(name);
        builder.AddForwarder(function, target);
        return builder.Build();
    }

    std::vector<uint8> Importer(std::vector<std::pair<std::string, std::string>> const& imports)
    {
        PeBuilder builder(0x140000000, false);
        builder.AddSection(".text", { 0xC3 }, PeBuilder::CodeCharacteristics);
        for (auto const& [dll, function] : imports)
            builder.AddImport(dll, function);
        return builder.Build();
    }

    std::string LoadFailure(GuestProcess::Options const& options)
    {
        GuestProcess process(options);
        try
        {
            process.LoadMain("WizardGraphicalClient.exe");
        }
        catch (EmulationError const& error)
        {
            return error.what();
        }
        return "the program loaded";
    }

    std::vector<uint8> MainProgram(uint32& codeRva)
    {
        auto build = [](std::vector<uint8> code, uint32* slotAcp, uint32* slotBox, uint32* slotAdd, uint32* slotSum)
        {
            PeBuilder builder(0x140000000, false);
            builder.AddSection(".text", std::move(code), PeBuilder::CodeCharacteristics);
            builder.AddImport("api-ms-win-crt-math-l1-1-0.dll", "Add");
            builder.AddImport("ucrtbase.dll", "Sum");
            builder.AddImport("api-ms-win-core-sysinfo-l1-1-0.dll", "GetACP");
            builder.AddImport("user32.dll", "MessageBoxW");
            std::vector<uint8> image = builder.Build();
            if (slotAcp)
                *slotAcp = builder.ImportSlotRva("api-ms-win-core-sysinfo-l1-1-0.dll", "GetACP");
            if (slotBox)
                *slotBox = builder.ImportSlotRva("user32.dll", "MessageBoxW");
            if (slotAdd)
                *slotAdd = builder.ImportSlotRva("api-ms-win-crt-math-l1-1-0.dll", "Add");
            if (slotSum)
                *slotSum = builder.ImportSlotRva("ucrtbase.dll", "Sum");
            return image;
        };
        std::vector<uint8> code = {
            0x48, 0x83, 0xEC, 0x28, 0xFF, 0x15, 0, 0, 0, 0, 0x48, 0x83, 0xC4, 0x28, 0xC3,
            0x48, 0x83, 0xEC, 0x28, 0xFF, 0x15, 0, 0, 0, 0, 0x48, 0x83, 0xC4, 0x28, 0xC3
        };
        uint32 slotAcp = 0;
        uint32 slotBox = 0;
        build(code, &slotAcp, &slotBox, nullptr, nullptr);
        codeRva = PeBuilder::SectionAlignment;
        PutDisp(code, 6, static_cast<int64>(slotAcp) - static_cast<int64>(codeRva + 10));
        PutDisp(code, 21, static_cast<int64>(slotBox) - static_cast<int64>(codeRva + 25));
        return build(code, nullptr, nullptr, nullptr, nullptr);
    }

    struct Folder
    {
        LogTestDirectory directory;
        std::filesystem::path bin;
        uint32 codeRva = 0;

        explicit Folder(bool refuse = false, std::filesystem::path const& name = "Bin")
        {
            bin = directory.Path() / name;
            std::filesystem::create_directories(bin);
            Save(bin / "WizardGraphicalClient.exe", MainProgram(codeRva));
            Save(bin / "UCRTBASE.dll", RuntimeDll(refuse));
            Save(bin / "api-ms-win-crt-math-l1-1-0.dll", ForwarderDll());
        }

        GuestProcess::Options Options() const
        {
            GuestProcess::Options options;
            options.Folder = bin;
            options.HeapSize = 0x1000000;
            return options;
        }
    };
}

TEST(GuestProcessTest, LoadsTheProgramAndItsRuntimeWithRelocationsForwardersAndTls)
{
    Folder folder;
    GuestProcess process(folder.Options());
    GuestModule& program = process.LoadMain("wizardgraphicalclient.EXE");
    EXPECT_EQ(program.Base, 0x140000000u);
    GuestModule* const runtime = process.FindModule("ucrtbase");
    ASSERT_NE(runtime, nullptr);
    GuestModule* const forwarder = process.FindModule("api-ms-win-crt-math-l1-1-0.dll");
    ASSERT_NE(forwarder, nullptr);
    EXPECT_EQ(forwarder->Base, GuestProcess::FirstDllBase);
    EXPECT_GT(runtime->Base, forwarder->Base);
    EXPECT_NE(runtime->Base, runtime->Image->GetImageBase());
    EXPECT_EQ(process.GetModules().size(), 3u);

    Machine& machine = process.GetMachine();
    std::optional<uint64> const add = process.ResolveExport(*runtime, "Add");
    ASSERT_TRUE(add);
    EXPECT_EQ(*add, runtime->Base + PeBuilder::SectionAlignment);
    EXPECT_EQ(process.ResolveExport(*runtime, "Sum"), add);
    EXPECT_FALSE(process.ResolveExport(*runtime, "Missing"));
    std::array<uint64, 2> const operands{ 40, 2 };
    EXPECT_EQ(process.Call(*add, operands, 1000), 42u);

    uint64 const dataRva = PeBuilder::SectionAlignment * 2;
    EXPECT_EQ(machine.ReadU64(runtime->Base + dataRva + 0x20), runtime->Base + dataRva + 0x08);
    ASSERT_TRUE(runtime->TlsIndex);
    EXPECT_EQ(*runtime->TlsIndex, 0u);
    EXPECT_EQ(machine.ReadU32(runtime->Base + dataRva + 0x10), 0u);
    ASSERT_EQ(runtime->TlsCallbacks.size(), 1u);
    EXPECT_EQ(runtime->TlsCallbacks[0], runtime->Base + PeBuilder::SectionAlignment + 5);
    EXPECT_NE(machine.ReadU64(GuestProcess::TlsArrayAddress), 0u);

    EXPECT_EQ(process.DescribeAddress(runtime->Base + 0x1234), "ucrtbase.dll+0x1234");
    EXPECT_EQ(process.ModuleAt(program.Base + 0x10), &program);
    EXPECT_EQ(process.ModuleAt(0x1000), nullptr);

    process.AttachRuntime();
    EXPECT_TRUE(runtime->Attached);
    EXPECT_EQ(machine.ReadU32(runtime->Base + dataRva + 0x18), 0x2Au);
}

TEST(GuestProcessTest, StubsCallHandlersByNameAndCountUnhandledCalls)
{
    Folder folder;
    GuestProcess process(folder.Options());
    GuestModule& program = process.LoadMain("WizardGraphicalClient.exe");
    process.RegisterApi("GetACP", [](GuestProcess& p)
    {
        EXPECT_EQ(p.GetCurrentApiName(), "GetACP");
        return uint64{ 1252 };
    });
    uint64 const codeAddress = program.Base + folder.codeRva;
    EXPECT_EQ(process.Call(codeAddress, std::span<uint64 const>{}, 1000), 1252u);
    EXPECT_EQ(process.Call(codeAddress + 15, std::span<uint64 const>{}, 1000), 0u);
    EXPECT_EQ(process.GetApiCallCounts().at("GetACP"), 1u);
    EXPECT_EQ(process.GetUnhandledApiCalls().at("user32.dll!MessageBoxW"), 1u);
    EXPECT_FALSE(process.GetUnhandledApiCalls().contains("kernel32.dll!GetACP"));
    uint64 const stub = process.ResolveImport("KERNEL32.dll", "GetACP", std::nullopt);
    EXPECT_EQ(process.DescribeAddress(stub), "kernel32.dll!GetACP");
    EXPECT_EQ(process.ResolveImport("ntdll.dll", "GetACP", std::nullopt), stub);
    EXPECT_EQ(process.ResolveImport("ws2_32.dll", {}, uint16{ 115 }), process.ResolveImport("ws2_32.dll", {}, uint16{ 115 }));
    EXPECT_EQ(process.DescribeAddress(process.ResolveImport("ws2_32.dll", {}, uint16{ 115 })), "ws2_32.dll!#115");
}

TEST(GuestProcessTest, StoresStringsAndRejectsBadStarts)
{
    Folder refusing(true);
    GuestProcess process(refusing.Options());
    process.LoadMain("WizardGraphicalClient.exe");
    Machine& machine = process.GetMachine();
    uint64 const narrow = process.StoreCString("race");
    EXPECT_EQ(machine.ReadCString(narrow, 16), "race");
    uint64 const wide = process.StoreWideString(u"en-US");
    EXPECT_EQ(machine.ReadWideString(wide, 16), u"en-US");
    EXPECT_TRUE(process.GetHeap().Contains(narrow));
    try
    {
        process.AttachRuntime();
        FAIL() << "a runtime entry point that returns FALSE must stop the start";
    }
    catch (EmulationError const& error)
    {
        EXPECT_NE(std::string_view(error.what()).find("ucrtbase.dll refused to start"), std::string_view::npos) << error.what();
    }

    GuestProcess empty(refusing.Options());
    EXPECT_THROW(empty.LoadMain("Missing.exe"), EmulationError);
    EXPECT_THROW(empty.GetMain(), EmulationError);
}

TEST(GuestProcessTest, HandlerFailuresNameTheApi)
{
    Folder folder;
    GuestProcess process(folder.Options());
    GuestModule& program = process.LoadMain("WizardGraphicalClient.exe");
    uint64 const code = program.Base + folder.codeRva;
    auto failure = [&]() -> std::string
    {
        try
        {
            process.Call(code, std::span<uint64 const>{}, 1000);
        }
        catch (EmulationError const& error)
        {
            return error.what();
        }
        return "the call returned";
    };
    process.RegisterApi("GetACP", [](GuestProcess&) -> uint64 { throw std::runtime_error("no code page today"); });
    EXPECT_EQ(failure(), "the handler for kernel32.dll!GetACP failed: no code page today");
    process.RegisterApi("GetACP", [](GuestProcess&) -> uint64 { throw 42; });
    EXPECT_EQ(failure(), "the handler for kernel32.dll!GetACP failed with an exception that carries no message");
    process.RegisterApi("GetACP", [](GuestProcess&) -> uint64 { throw EmulationError("refused as itself"); });
    EXPECT_EQ(failure(), "refused as itself");
    process.RegisterApi("GetACP", [](GuestProcess&) { return uint64{ 1252 }; });
    EXPECT_EQ(process.Call(code, std::span<uint64 const>{}, 1000), 1252u);
}

TEST(GuestProcessTest, NamesOutsideTheAnsiCodePageStillLoadAndReport)
{
    Folder folder(false, std::filesystem::path(std::u16string(u"Bin") + char16_t{ 0x30B2 }));
    Save(folder.bin / std::filesystem::path(std::u16string(u"0") + char16_t{ 0x30B2 } + u".txt"), { 1 });
    Save(folder.bin / std::filesystem::path(std::u16string(u"Copy of ") + char16_t{ 0x30B2 } + u".txt"), { 1 });
    GuestProcess process(folder.Options());
    EXPECT_NO_THROW(process.LoadMain("WizardGraphicalClient.exe"));
    EXPECT_NE(process.FindModule("ucrtbase.dll"), nullptr);
    EXPECT_NE(process.FindModule("api-ms-win-crt-math-l1-1-0.dll"), nullptr);

    GuestProcess missing(folder.Options());
    try
    {
        missing.LoadMain("Missing.exe");
        FAIL() << "a missing program must stop the load";
    }
    catch (EmulationError const& error)
    {
        EXPECT_EQ(std::string(error.what()), "Missing.exe was not found in " + ClientLocator::PathText(folder.bin));
    }

    Save(folder.bin / "UCRTBASE.dll", { 'M', 'Z' });
    std::string const unreadable = LoadFailure(folder.Options());
    EXPECT_TRUE(unreadable.starts_with(ClientLocator::PathText(folder.bin / "UCRTBASE.dll") + " could not be read: ")) << unreadable;
}

TEST(GuestProcessTest, NamesTheHostCannotConvertAreSkipped)
{
    Folder folder;
#ifdef _WIN32
    std::filesystem::path const name(std::wstring{ L'0', static_cast<wchar_t>(0xD800), L'.', L't', L'x', L't' });
#else
    std::filesystem::path const name(std::string("0\xFF.txt"));
#endif
    Save(folder.bin / name, { 1 });
    GuestProcess process(folder.Options());
    EXPECT_NO_THROW(process.LoadMain("WizardGraphicalClient.exe"));
    EXPECT_NE(process.FindModule("ucrtbase.dll"), nullptr);
}

TEST(GuestProcessTest, MissingRuntimeDllsAndExportsAreNamed)
{
    Folder folder;
    std::string const bin = ClientLocator::PathText(folder.bin);
    std::filesystem::path const program = folder.bin / "WizardGraphicalClient.exe";

    Save(program, Importer({ { "MSVCP140.dll", "?_Xlength_error@std@@YAXPEBD@Z" } }));
    EXPECT_EQ(LoadFailure(folder.Options()), "msvcp140.dll is missing from " + bin + ", which wizardgraphicalclient.exe needs");

    Save(program, Importer({ { "ucrtbase.dll", "Missing" } }));
    EXPECT_EQ(LoadFailure(folder.Options()), "ucrtbase.dll does not export Missing, which wizardgraphicalclient.exe needs");

    Save(program, Importer({ { "api-ms-win-crt-math-l1-1-0.dll", "Missing" } }));
    EXPECT_EQ(LoadFailure(folder.Options()), "ucrtbase.dll does not export Missing, which wizardgraphicalclient.exe needs");

    Save(folder.bin / "api-ms-win-crt-runtime-l1-1-0.dll", ForwarderDll("api-ms-win-crt-runtime-l1-1-0.dll", "Exit", "vcruntime140.Exit"));
    Save(program, Importer({ { "api-ms-win-crt-runtime-l1-1-0.dll", "Exit" } }));
    EXPECT_EQ(LoadFailure(folder.Options()), "vcruntime140.dll is missing from " + bin + ", which api-ms-win-crt-runtime-l1-1-0.dll!Exit needs");

    Save(program, Importer({ { "d3d9.dll", "Direct3DCreate9" }, { "USER32.dll", "MessageBoxW" }, { "steam_api64.dll", "SteamAPI_Init" } }));
    GuestProcess stubs(folder.Options());
    EXPECT_NO_THROW(stubs.LoadMain("WizardGraphicalClient.exe"));
    EXPECT_EQ(stubs.DescribeAddress(stubs.ResolveImport("d3d9.dll", "Direct3DCreate9", std::nullopt)), "d3d9.dll!Direct3DCreate9");
    EXPECT_EQ(stubs.DescribeAddress(stubs.ResolveImport("user32.dll", "MessageBoxW", std::nullopt)), "user32.dll!MessageBoxW");
    EXPECT_EQ(stubs.DescribeAddress(stubs.ResolveImport("steam_api64.dll", "SteamAPI_Init", std::nullopt)), "steam_api64.dll!SteamAPI_Init");
    EXPECT_EQ(stubs.GetModules().size(), 1u);
    try
    {
        stubs.ResolveImport("msvcp140.dll", "Anything", std::nullopt);
        FAIL() << "a missing runtime DLL must not become a stub";
    }
    catch (EmulationError const& error)
    {
        EXPECT_EQ(std::string(error.what()), "msvcp140.dll is missing from " + bin);
    }
    try
    {
        stubs.ResolveImport("api-ms-win-crt-heap-l1-1-0.dll", {}, uint16{ 5 });
        FAIL() << "an ordinal cannot fall back to ucrtbase";
    }
    catch (EmulationError const& error)
    {
        EXPECT_EQ(std::string(error.what()), "api-ms-win-crt-heap-l1-1-0.dll is missing from " + bin);
    }

    std::filesystem::remove(folder.bin / "UCRTBASE.dll");
    std::filesystem::remove(folder.bin / "api-ms-win-crt-math-l1-1-0.dll");
    Save(program, Importer({ { "api-ms-win-crt-string-l1-1-0.dll", "strlen" } }));
    EXPECT_EQ(LoadFailure(folder.Options()), "ucrtbase.dll is missing from " + bin + ", which wizardgraphicalclient.exe needs");
}

TEST(GuestProcessTest, UniversalRuntimeSetsFallBackToUcrtbase)
{
    Folder folder;
    GuestProcess process(folder.Options());
    process.LoadMain("WizardGraphicalClient.exe");
    GuestModule* const runtime = process.FindModule("ucrtbase.dll");
    ASSERT_NE(runtime, nullptr);
    std::optional<uint64> const pointer = process.ResolveExport(*runtime, "DataPointer");
    ASSERT_TRUE(pointer);
    EXPECT_EQ(process.ResolveImport("api-ms-win-crt-math-l1-1-0.dll", "DataPointer", std::nullopt), *pointer);
    EXPECT_EQ(process.ResolveImport("api-ms-win-crt-stdio-l1-1-0.dll", "DataPointer", std::nullopt), *pointer);
    EXPECT_EQ(process.ResolveImport("api-ms-win-crt-math-l1-1-0.dll", "Add", std::nullopt), process.ResolveExport(*runtime, "Add").value_or(0));
}
