/*
 * Project Ambrose by Imjustchico
 * Tests the Windows layer through its stubs: heap blocks, TLS slots, last error, SLists, code page 1252, 437 and UTF-8 conversion with short buffers and broken UTF-8 replaced as Windows does, character types and case mapping from Windows' tables, module and export lookup, host-independent module file names with short buffers under a folder outside code page 1252, one-time initialization that runs its callback once, refused exits, and deterministic time values that agree with each other.
 */

#include "ConfigMgr.h"
#include "GuestProcess.h"
#include "LogTestDirectory.h"
#include "PeBuilder.h"
#include "WindowsApi.h"

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    struct Api
    {
        LogTestDirectory directory;
        std::unique_ptr<GuestProcess> process;

        explicit Api(std::filesystem::path const& folderName = "Bin", std::string const& programName = "WizardGraphicalClient.exe")
        {
            PeBuilder builder(0x140000000, false);
            std::vector<uint8> code = {
                0xB8, 0x01, 0x00, 0x00, 0x00,
                0x48, 0x83, 0x05, 0, 0, 0, 0, 0x01,
                0xC3
            };
            uint32 const codeRva = builder.NextSectionRva();
            uint32 const dataRva = codeRva + 0x1000;
            int32 const disp = static_cast<int32>(static_cast<int64>(dataRva) - static_cast<int64>(codeRva + 13));
            for (int i = 0; i < 4; ++i)
                code[8 + static_cast<std::size_t>(i)] = static_cast<uint8>(static_cast<uint32>(disp) >> (8 * i));
            builder.AddSection(".text", code, PeBuilder::CodeCharacteristics);
            builder.AddSection(".data", std::vector<uint8>(0x10, 0), PeBuilder::DataCharacteristics);
            builder.AddExport("Callback", codeRva);
            std::vector<uint8> const image = builder.Build();
            std::filesystem::path const folder = directory.Path() / folderName;
            std::filesystem::create_directories(folder);
            std::ofstream(folder / ConfigMgr::PathFromUtf8(programName), std::ios::binary).write(reinterpret_cast<char const*>(image.data()), static_cast<std::streamsize>(image.size()));
            GuestProcess::Options options;
            options.Folder = folder;
            options.HeapSize = 0x2000000;
            process = std::make_unique<GuestProcess>(options);
            WindowsApi::Register(*process);
            process->LoadMain(programName);
        }

        uint64 Call(std::string_view name, std::initializer_list<uint64> arguments)
        {
            return process->Call(process->ResolveImport("kernel32.dll", name, std::nullopt), arguments, 1000000);
        }

        Machine& M()
        {
            return process->GetMachine();
        }

        uint64 Buffer(std::size_t size)
        {
            return process->GetHeap().Allocate(size, true);
        }

        uint64 Filled(std::size_t size)
        {
            uint64 const address = Buffer(size);
            M().Write(address, std::vector<uint8>(size, 0xFF));
            return address;
        }

        std::u16string ReadUnits(uint64 address, std::size_t count)
        {
            std::u16string units;
            for (std::size_t i = 0; i < count; ++i)
                units.push_back(static_cast<char16_t>(M().ReadU16(address + 2 * i)));
            return units;
        }

        std::string ReadNarrowBytes(uint64 address, std::size_t count)
        {
            std::string bytes;
            for (std::size_t i = 0; i < count; ++i)
                bytes.push_back(static_cast<char>(M().ReadU8(address + i)));
            return bytes;
        }
    };
}

TEST(WindowsApiTest, HeapTlsErrorsAndSLists)
{
    Api api;
    uint64 const heap = api.Call("GetProcessHeap", {});
    uint64 const block = api.Call("HeapAlloc", { heap, 8, 24 });
    EXPECT_TRUE(api.process->GetHeap().Contains(block));
    EXPECT_EQ(api.Call("HeapSize", { heap, 0, block }), 24u);
    api.M().WriteU64(block, 0x1122334455667788ull);
    uint64 const grown = api.Call("HeapReAlloc", { heap, 0, block, 64 });
    EXPECT_EQ(api.M().ReadU64(grown), 0x1122334455667788ull);
    EXPECT_EQ(api.Call("HeapSize", { heap, 0, grown }), 64u);
    EXPECT_EQ(api.Call("HeapFree", { heap, 0, grown }), 1u);

    uint64 const slot = api.Call("TlsAlloc", {});
    EXPECT_EQ(api.Call("TlsGetValue", { slot }), 0u);
    EXPECT_EQ(api.Call("TlsSetValue", { slot, 99 }), 1u);
    EXPECT_EQ(api.Call("FlsGetValue", { slot }), 99u);
    EXPECT_NE(api.Call("FlsAlloc", { 0 }), slot);

    api.Call("SetLastError", { 5 });
    EXPECT_EQ(api.Call("GetLastError", {}), 5u);

    uint64 const head = api.Buffer(16);
    api.Call("InitializeSListHead", { head });
    uint64 const first = api.Buffer(16);
    uint64 const second = api.Buffer(16);
    EXPECT_EQ(api.Call("InterlockedPushEntrySList", { head, first }), 0u);
    EXPECT_EQ(api.Call("InterlockedPushEntrySList", { head, second }), first);
    EXPECT_EQ(api.Call("QueryDepthSList", { head }), 2u);
    EXPECT_EQ(api.Call("InterlockedPopEntrySList", { head }), second);
    EXPECT_EQ(api.Call("InterlockedFlushSList", { head }), first);
    EXPECT_EQ(api.Call("InterlockedPopEntrySList", { head }), 0u);
}

TEST(WindowsApiTest, CodePagesConvertLikeWindows)
{
    Api api;
    uint64 const narrow = api.process->StoreBytes(std::vector<uint8>{ 'A', 0x80, 0xE9, 0 });
    uint64 const wide = api.Buffer(32);
    EXPECT_EQ(api.Call("MultiByteToWideChar", { 1252, 0, narrow, 0xFFFFFFFF, 0, 0 }), 4u);
    EXPECT_EQ(api.Call("MultiByteToWideChar", { 0, 0, narrow, 0xFFFFFFFF, wide, 16 }), 4u);
    EXPECT_EQ(api.M().ReadWideString(wide, 8), std::u16string(u"A\u20AC\u00E9"));
    EXPECT_EQ(api.Call("MultiByteToWideChar", { 437, 0, narrow, 3, wide, 16 }), 3u);
    EXPECT_EQ(api.M().ReadU16(wide + 2), 0x00C7u);
    EXPECT_EQ(api.Call("MultiByteToWideChar", { 1252, 0, narrow, 0xFFFFFFFF, wide, 2 }), 0u);
    EXPECT_EQ(api.Call("GetLastError", {}), 122u);

    uint64 const utf8 = api.process->StoreBytes(std::vector<uint8>{ 0xE2, 0x82, 0xAC, 0xF0, 0x9F, 0x98, 0x80, 0 });
    EXPECT_EQ(api.Call("MultiByteToWideChar", { 65001, 0, utf8, 0xFFFFFFFF, wide, 16 }), 4u);
    EXPECT_EQ(api.M().ReadU16(wide), 0x20ACu);
    EXPECT_EQ(api.M().ReadU16(wide + 2), 0xD83Du);
    uint64 const broken = api.process->StoreBytes(std::vector<uint8>{ 0xC3, 0x28, 0 });
    EXPECT_EQ(api.Call("MultiByteToWideChar", { 65001, 8, broken, 0xFFFFFFFF, wide, 16 }), 0u);
    EXPECT_EQ(api.Call("GetLastError", {}), 1113u);

    uint64 const source = api.process->StoreWideString(u"\u20AC\u4E2D");
    uint64 const out = api.Buffer(16);
    uint64 const usedDefault = api.Buffer(4);
    EXPECT_EQ(api.Call("WideCharToMultiByte", { 1252, 0, source, 0xFFFFFFFF, out, 16, 0, usedDefault }), 3u);
    EXPECT_EQ(api.M().ReadU8(out), 0x80u);
    EXPECT_EQ(api.M().ReadU8(out + 1), static_cast<uint8>('?'));
    EXPECT_EQ(api.M().ReadU32(usedDefault), 1u);
    EXPECT_EQ(api.Call("WideCharToMultiByte", { 65001, 0, source, 0xFFFFFFFF, 0, 0, 0, 0 }), 7u);

    uint64 const info = api.Buffer(20);
    EXPECT_EQ(api.Call("GetCPInfo", { 1252, info }), 1u);
    EXPECT_EQ(api.M().ReadU32(info), 1u);
    EXPECT_EQ(api.Call("GetCPInfo", { 932, info }), 0u);
    EXPECT_EQ(api.Call("IsValidCodePage", { 65001 }), 1u);
    EXPECT_EQ(api.Call("GetACP", {}), 1252u);
}

TEST(WindowsApiTest, CharacterTypesAndCaseMapping)
{
    Api api;
    uint64 const text = api.process->StoreWideString(u"Aa1 \t!\u00E9\u00D7");
    uint64 const types = api.Buffer(32);
    EXPECT_EQ(api.Call("GetStringTypeW", { 1, text, 8, types }), 1u);
    EXPECT_EQ(api.M().ReadU16(types), 0x381u);
    EXPECT_EQ(api.M().ReadU16(types + 2), 0x382u);
    EXPECT_EQ(api.M().ReadU16(types + 4), 0x284u);
    EXPECT_EQ(api.M().ReadU16(types + 6), 0x248u);
    EXPECT_EQ(api.M().ReadU16(types + 8), 0x268u);
    EXPECT_EQ(api.M().ReadU16(types + 10), 0x210u);
    EXPECT_EQ(api.M().ReadU16(types + 12), 0x302u);
    EXPECT_EQ(api.M().ReadU16(types + 14), 0x210u);

    uint64 const upper = api.Buffer(32);
    EXPECT_EQ(api.Call("LCMapStringW", { 0x409, 0x200, text, 8, upper, 16 }), 8u);
    EXPECT_EQ(api.M().ReadU16(upper + 2), static_cast<uint16>('A'));
    EXPECT_EQ(api.M().ReadU16(upper + 12), 0x00C9u);
    EXPECT_EQ(api.Call("LCMapStringEx", { 0, 0x100, text, 2, 0, 0 }), 2u);
    uint64 const left = api.process->StoreWideString(u"abc");
    uint64 const right = api.process->StoreWideString(u"ABD");
    EXPECT_EQ(api.Call("CompareStringW", { 0x409, 1, left, 0xFFFFFFFF, right, 0xFFFFFFFF }), 1u);
}

TEST(WindowsApiTest, ModulesOnceCallbacksExitsAndTime)
{
    Api api;
    GuestModule& program = api.process->GetMain();
    EXPECT_EQ(api.Call("GetModuleHandleW", { 0 }), program.Base);
    uint64 const name = api.process->StoreWideString(u"WizardGraphicalClient.exe");
    EXPECT_EQ(api.Call("GetModuleHandleW", { name }), program.Base);
    uint64 const missing = api.process->StoreWideString(u"d3d9.dll");
    EXPECT_EQ(api.Call("LoadLibraryExW", { missing, 0, 0 }), 0u);
    EXPECT_EQ(api.Call("GetLastError", {}), 126u);
    uint64 const exportName = api.process->StoreCString("Callback");
    EXPECT_EQ(api.Call("GetProcAddress", { program.Base, exportName }), program.Base + PeBuilder::SectionAlignment);
    uint64 const path = api.Buffer(512);
    EXPECT_GT(api.Call("GetModuleFileNameW", { 0, path, 256 }), 0u);

    uint64 const once = api.Buffer(8);
    uint64 const callback = program.Base + PeBuilder::SectionAlignment;
    uint64 const counter = program.Base + PeBuilder::SectionAlignment * 2;
    EXPECT_EQ(api.Call("InitOnceExecuteOnce", { once, callback, 0, 0 }), 1u);
    EXPECT_EQ(api.Call("InitOnceExecuteOnce", { once, callback, 0, 0 }), 1u);
    EXPECT_EQ(api.M().ReadU64(counter), 1u);

    EXPECT_THROW(api.Call("ExitProcess", { 0 }), EmulationError);
    uint64 const time = api.Buffer(8);
    api.Call("GetSystemTimeAsFileTime", { time });
    uint64 const first = api.M().ReadU64(time);
    api.Call("GetSystemTimeAsFileTime", { time });
    EXPECT_EQ(api.M().ReadU64(time), first);
    EXPECT_EQ(api.Call("GetCurrentProcessId", {}), api.Call("GetCurrentProcessId", {}));
    EXPECT_TRUE(api.process->GetUnhandledApiCalls().empty());
}

TEST(WindowsApiTest, ModuleFileNamesDoNotDependOnTheHostFolder)
{
    Api api(std::filesystem::path(std::u16string(u"Wizard") + char16_t{ 0x30B2 }), "Client\xC3\xA9\xE3\x82\xB2.exe");
    std::u16string const widePath = std::u16string(u"C:\\Wizard101\\Bin\\Client") + char16_t{ 0x00E9 } + char16_t{ 0x30B2 } + u".exe";
    std::string const narrowPath = std::string("C:\\Wizard101\\Bin\\Client") + '\xE9' + "?.exe";
    ASSERT_EQ(widePath.size(), 29u);
    ASSERT_EQ(narrowPath.size(), 29u);

    uint64 const wide = api.Filled(64);
    api.Call("SetLastError", { 5 });
    EXPECT_EQ(api.Call("GetModuleFileNameW", { 0, wide, 30 }), 29u);
    EXPECT_EQ(api.Call("GetLastError", {}), 0u);
    EXPECT_EQ(api.ReadUnits(wide, 31), widePath + u'\0' + char16_t{ 0xFFFF });
    EXPECT_EQ(api.Call("GetModuleFileNameW", { api.process->GetMain().Base, wide, 64 }), 29u);

    uint64 const exact = api.Filled(64);
    EXPECT_EQ(api.Call("GetModuleFileNameW", { 0, exact, 29 }), 29u);
    EXPECT_EQ(api.Call("GetLastError", {}), 122u);
    EXPECT_EQ(api.ReadUnits(exact, 30), widePath.substr(0, 28) + u'\0' + char16_t{ 0xFFFF });

    uint64 const shortWide = api.Filled(16);
    EXPECT_EQ(api.Call("GetModuleFileNameW", { 0, shortWide, 5 }), 5u);
    EXPECT_EQ(api.Call("GetLastError", {}), 122u);
    EXPECT_EQ(api.ReadUnits(shortWide, 6), std::u16string(u"C:\\W") + u'\0' + char16_t{ 0xFFFF });

    uint64 const narrow = api.Filled(64);
    api.Call("SetLastError", { 5 });
    EXPECT_EQ(api.Call("GetModuleFileNameA", { 0, narrow, 64 }), 29u);
    EXPECT_EQ(api.Call("GetLastError", {}), 0u);
    EXPECT_EQ(api.ReadNarrowBytes(narrow, 31), narrowPath + '\0' + '\xFF');

    uint64 const shortNarrow = api.Filled(32);
    EXPECT_EQ(api.Call("GetModuleFileNameA", { 0, shortNarrow, 25 }), 25u);
    EXPECT_EQ(api.Call("GetLastError", {}), 122u);
    EXPECT_EQ(api.ReadNarrowBytes(shortNarrow, 26), narrowPath.substr(0, 24) + '\0' + '\xFF');

    EXPECT_EQ(api.Call("GetModuleFileNameW", { 0, wide, 0 }), 0u);
    EXPECT_EQ(api.Call("GetLastError", {}), 122u);
    EXPECT_EQ(api.Call("GetModuleFileNameA", { 0x1234, narrow, 64 }), 0u);
    EXPECT_EQ(api.Call("GetLastError", {}), 126u);

    uint64 const kernelName = api.process->StoreWideString(u"KERNEL32.dll");
    uint64 const kernel = api.Call("GetModuleHandleW", { kernelName });
    EXPECT_EQ(api.Call("GetModuleFileNameA", { kernel, narrow, 64 }), 32u);
    EXPECT_EQ(api.ReadNarrowBytes(narrow, 33), std::string("C:\\WINDOWS\\System32\\KERNEL32.DLL") + '\0');
    EXPECT_TRUE(api.process->GetUnhandledApiCalls().empty());
}

TEST(WindowsApiTest, BrokenUtf8IsReplacedAsWindowsDoes)
{
    struct Case
    {
        std::vector<uint8> Bytes;
        uint64 Length = 0;
        std::u16string Expected;
    };
    constexpr uint64 Terminated = 0xFFFFFFFF;
    constexpr char16_t Bad = 0xFFFD;
    std::vector<Case> const cases = {
        { { 0xF0, 0x41, 0x00 }, Terminated, { Bad, u'A', 0 } },
        { { 'c', 'a', 'f', 0xE9, '!' }, 5, { u'c', u'a', u'f', Bad, u'!' } },
        { { 'c', 'a', 'f', 0xE9, 0x00 }, Terminated, { u'c', u'a', u'f', Bad, 0 } },
        { { 0xE2, 0x82, 0x41 }, 3, { Bad, u'A' } },
        { { 'c', 'a', 'f', 0xC3 }, 4, { u'c', u'a', u'f', Bad } },
        { { 0xF0, 0x9F, 0x98, 0x41 }, 4, { Bad, u'A' } },
        { { 0xE2, 0x82 }, 2, { Bad } },
        { { 0xC0, 0x80 }, 2, { Bad, Bad } },
        { { 0xE0, 0x80, 0x80 }, 3, { Bad, Bad } },
        { { 0xED, 0xA0, 0x80 }, 3, { Bad, Bad } },
        { { 0xF4, 0x90, 0x80, 0x80 }, 4, { Bad, Bad, Bad } },
        { { 0x80, 0xBF, 0xF5, 0xFF, 0x41 }, 5, { Bad, Bad, Bad, Bad, u'A' } },
        { { 0xE0, 0xA0, 0x80, 0xF0, 0x9F, 0x98, 0x80, 0xF4, 0x8F, 0xBF, 0xBF }, 11, { 0x0800, 0xD83D, 0xDE00, 0xDBFF, 0xDFFF } }
    };
    Api api;
    for (std::size_t index = 0; index < cases.size(); ++index)
    {
        SCOPED_TRACE(index);
        Case const& test = cases[index];
        uint64 const source = api.process->StoreBytes(test.Bytes);
        EXPECT_EQ(api.Call("MultiByteToWideChar", { 65001, 0, source, test.Length, 0, 0 }), test.Expected.size());
        uint64 const out = api.Filled(64);
        EXPECT_EQ(api.Call("MultiByteToWideChar", { 65001, 0, source, test.Length, out, 32 }), test.Expected.size());
        EXPECT_EQ(api.ReadUnits(out, test.Expected.size() + 1), test.Expected + char16_t{ 0xFFFF });
        bool const broken = test.Expected.find(Bad) != std::u16string::npos;
        EXPECT_EQ(api.Call("MultiByteToWideChar", { 65001, 8, source, test.Length, out, 32 }), broken ? 0u : test.Expected.size());
    }
}

TEST(WindowsApiTest, CharacterTypesAndCaseMappingUseWindowsTables)
{
    Api api;
    std::u16string const sample = { 0x0085, 0x00AA, 0x00AD, 0x0160, 0x0161, 0x0178, 0x00FF, 0x0192, 0x20AC, 0x00DF, 0x4E2D };
    std::array<uint16, 11> const types = { 0x228, 0x312, 0x230, 0x301, 0x302, 0x301, 0x302, 0x302, 0x200, 0x302, 0x300 };
    std::u16string const upper = { 0x0085, 0x00AA, 0x00AD, 0x0160, 0x0160, 0x0178, 0x0178, 0x0191, 0x20AC, 0x00DF, 0x4E2D };
    std::u16string const lower = { 0x0085, 0x00AA, 0x00AD, 0x0161, 0x0161, 0x00FF, 0x00FF, 0x0192, 0x20AC, 0x00DF, 0x4E2D };
    uint64 const text = api.process->StoreWideString(sample);
    uint64 const out = api.Buffer(64);
    EXPECT_EQ(api.Call("GetStringTypeW", { 1, text, sample.size(), out }), 1u);
    for (std::size_t i = 0; i < types.size(); ++i)
    {
        EXPECT_EQ(api.M().ReadU16(out + 2 * i), types[i]) << "character " << i;
    }
    EXPECT_EQ(api.Call("GetStringTypeExW", { 0x409, 1, text, 0xFFFFFFFF, out }), 1u);
    EXPECT_EQ(api.M().ReadU16(out), 0x228u);
    EXPECT_EQ(api.Call("LCMapStringEx", { 0, 0x200, text, sample.size(), out, 32 }), sample.size());
    EXPECT_EQ(api.ReadUnits(out, sample.size()), upper);
    EXPECT_EQ(api.Call("LCMapStringW", { 0x409, 0x100, text, sample.size(), out, 32 }), sample.size());
    EXPECT_EQ(api.ReadUnits(out, sample.size()), lower);
    uint64 const capitals = api.process->StoreWideString(std::u16string{ 0x0160, 0x0178 });
    uint64 const minuscules = api.process->StoreWideString(std::u16string{ 0x0161, 0x00FF });
    EXPECT_EQ(api.Call("CompareStringW", { 0x409, 1, capitals, 0xFFFFFFFF, minuscules, 0xFFFFFFFF }), 2u);
    EXPECT_EQ(api.Call("CompareStringW", { 0x409, 0, capitals, 0xFFFFFFFF, minuscules, 0xFFFFFFFF }), 1u);
}

TEST(WindowsApiTest, TimeValuesDescribeOneInstant)
{
    Api api;
    uint64 const fileTime = api.Buffer(8);
    api.Call("GetSystemTimeAsFileTime", { fileTime });
    EXPECT_EQ(api.M().ReadU64(fileTime), 133500000000000000ull);
    api.Call("GetSystemTimePreciseAsFileTime", { fileTime });
    EXPECT_EQ(api.M().ReadU64(fileTime), 133500000000000000ull);
    std::array<uint16, 8> const expected = { 2024, 1, 3, 17, 21, 20, 0, 0 };
    for (std::string_view const name : { "GetSystemTime", "GetLocalTime" })
    {
        uint64 const time = api.Filled(16);
        api.Call(name, { time });
        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            EXPECT_EQ(api.M().ReadU16(time + 2 * i), expected[i]) << name << " field " << i;
        }
    }
    uint64 const zone = api.Filled(172);
    EXPECT_EQ(api.Call("GetTimeZoneInformation", { zone }), 0u);
    EXPECT_EQ(api.M().ReadBytes(zone, 172), std::vector<uint8>(172, 0));
}
