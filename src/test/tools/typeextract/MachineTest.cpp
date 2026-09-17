/*
 * Project Ambrose by Imjustchico
 * Tests the guest machine on hand-assembled x86-64 code: Windows x64 arguments and return values, instruction budgets, fault reports, code hooks, redirects and reentrancy, segment bases, XMM registers, string reads at mapping ends, and mapping, read and write errors.
 */

#include "Machine.h"

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    constexpr uint64 CodeBase = 0x140001000;
    constexpr uint64 DataBase = 0x20000000;
    constexpr uint64 UnmappedAddress = 0x5000000000;

    std::vector<uint8> MovRaxImm64(uint64 value)
    {
        std::vector<uint8> bytes = { 0x48, 0xB8 };
        for (int shift = 0; shift < 64; shift += 8)
            bytes.push_back(static_cast<uint8>(value >> shift));
        return bytes;
    }

    std::vector<uint8> Concat(std::vector<std::vector<uint8>> const& parts)
    {
        std::vector<uint8> bytes;
        for (std::vector<uint8> const& part : parts)
            bytes.insert(bytes.end(), part.begin(), part.end());
        return bytes;
    }

    void LoadCode(Machine& machine, uint64 address, std::vector<uint8> const& code)
    {
        if (!machine.IsMapped(address, code.size()))
            machine.Map(address, code.size());
        machine.Write(address, code);
    }

    std::string CallError(Machine& machine, uint64 function, std::span<uint64 const> arguments, uint64 budget)
    {
        try
        {
            machine.Call(function, arguments, budget);
        }
        catch (EmulationError const& error)
        {
            return error.what();
        }
        return {};
    }

    std::vector<uint8> WideBytes(std::u16string_view text)
    {
        std::vector<uint8> bytes;
        for (char16_t const unit : text)
        {
            bytes.push_back(static_cast<uint8>(unit));
            bytes.push_back(static_cast<uint8>(unit >> 8));
        }
        return bytes;
    }

    std::vector<uint8> TextBytes(std::string_view text)
    {
        return std::vector<uint8>(text.begin(), text.end());
    }

    uint64 CallFrameTop()
    {
        return (Machine::StackBase + Machine::StackSize - Machine::CallFrameHeadroom) & ~uint64{ 0xF };
    }
}

TEST(MachineTest, PassesSixArgumentsAndReturnsRax)
{
    Machine machine;
    LoadCode(machine, CodeBase, {
        0x48, 0x8B, 0x44, 0x24, 0x30,
        0x48, 0xC1, 0xE0, 0x08,
        0x48, 0x03, 0x44, 0x24, 0x28,
        0x48, 0xC1, 0xE0, 0x08,
        0x4C, 0x01, 0xC8,
        0x48, 0xC1, 0xE0, 0x08,
        0x4C, 0x01, 0xC0,
        0x48, 0xC1, 0xE0, 0x08,
        0x48, 0x01, 0xD0,
        0x48, 0xC1, 0xE0, 0x08,
        0x48, 0x01, 0xC8,
        0xC3 });

    uint64 entryRsp = 0;
    uint64 entryReturn = 0;
    machine.HookCode(CodeBase, CodeBase + 1, [&](uint64)
    {
        entryRsp = machine.GetRegister(GuestRegister::Rsp);
        entryReturn = machine.ReadU64(entryRsp);
    });

    std::array<uint64, 6> const arguments = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
    EXPECT_EQ(machine.Call(CodeBase, arguments, 100), uint64{ 0x060504030201 });
    EXPECT_EQ(entryRsp, CallFrameTop() - 8);
    EXPECT_EQ(entryRsp & 0xF, uint64{ 8 });
    EXPECT_EQ(entryReturn, Machine::SentinelAddress);
    EXPECT_EQ(machine.GetRegister(GuestRegister::Rip), Machine::SentinelAddress);
    EXPECT_EQ(machine.GetRegister(GuestRegister::Rsp), CallFrameTop());
    EXPECT_TRUE(machine.IsMapped(Machine::StackBase, Machine::StackSize));
    EXPECT_TRUE(machine.IsMapped(Machine::SentinelAddress, Machine::PageSize));
    EXPECT_EQ(machine.ReadU8(Machine::SentinelAddress + Machine::PageSize - 1), 0xC3);
    EXPECT_FALSE(machine.GetLastFault().has_value());
}

TEST(MachineTest, ReadsEveryArgumentAtFunctionEntry)
{
    Machine machine;
    LoadCode(machine, CodeBase, { 0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3 });

    std::vector<uint64> seen;
    machine.HookCode(CodeBase, CodeBase + 1, [&](uint64 address)
    {
        EXPECT_EQ(address, CodeBase);
        for (std::size_t index = 0; index < 9; ++index)
            seen.push_back(machine.GetArgument(index));
    });

    std::array<uint64, 9> const arguments = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0xFFFFFFFFFFFFFFFF };
    EXPECT_EQ(machine.Call(CodeBase, arguments, 10), uint64{ 42 });
    EXPECT_EQ(seen, std::vector<uint64>(arguments.begin(), arguments.end()));
}

TEST(MachineTest, UnusedArgumentRegistersKeepTheirValues)
{
    Machine machine;
    LoadCode(machine, CodeBase, { 0x4C, 0x89, 0xC0, 0x4C, 0x01, 0xC8, 0x48, 0x01, 0xC8, 0xC3 });
    machine.SetRegister(GuestRegister::R8, 0x800);
    machine.SetRegister(GuestRegister::R9, 0x90);
    machine.SetRegister(GuestRegister::R12, 0x1212);

    std::array<uint64, 1> const arguments = { 0x7 };
    EXPECT_EQ(machine.Call(CodeBase, arguments, 10), uint64{ 0x897 });
    EXPECT_EQ(machine.GetRegister(GuestRegister::R12), uint64{ 0x1212 });
    EXPECT_EQ(machine.Call(CodeBase, {}, 10), uint64{ 0x897 });
}

TEST(MachineTest, RefusesMoreArgumentsThanTheCallFrameHolds)
{
    Machine machine;
    LoadCode(machine, CodeBase, { 0xC3 });
    std::vector<uint64> const fits(4 + (Machine::CallFrameHeadroom - 0x20) / 8, 1);
    EXPECT_NO_THROW(machine.Call(CodeBase, fits, 10));
    std::vector<uint64> const tooMany(fits.size() + 1, 1);
    std::string const message = CallError(machine, CodeBase, tooMany, 10);
    EXPECT_NE(message.find("8193 arguments"), std::string::npos) << message;
}

TEST(MachineTest, ReportsAnExhaustedBudgetAndKeepsWorking)
{
    Machine machine;
    LoadCode(machine, CodeBase, { 0xEB, 0xFE });
    LoadCode(machine, CodeBase + 0x10, { 0xB8, 0x07, 0x00, 0x00, 0x00, 0xC3 });

    std::string const message = CallError(machine, CodeBase, {}, 1000);
    EXPECT_EQ(message, "emulation of 0x140001000 ran out of its 1000 instruction budget at 0x140001000");
    EXPECT_FALSE(machine.GetLastFault().has_value());

    std::string const zero = CallError(machine, CodeBase + 0x10, {}, 0);
    EXPECT_EQ(zero, "emulation of 0x140001010 ran out of its 0 instruction budget at 0x140001010");

    EXPECT_EQ(machine.Call(CodeBase + 0x10, {}, 2), uint64{ 7 });
    EXPECT_NE(CallError(machine, CodeBase + 0x10, {}, 1).find("ran out of its 1 instruction budget at 0x140001015"), std::string::npos);
}

TEST(MachineTest, ReportsUnmappedReadsWithTheirAddress)
{
    Machine machine;
    LoadCode(machine, CodeBase, Concat({ MovRaxImm64(UnmappedAddress), { 0x48, 0x8B, 0x00, 0xC3 } }));

    std::string const message = CallError(machine, CodeBase, {}, 100);
    EXPECT_NE(message.find("emulation of 0x140001000 stopped at 0x14000100a: "), std::string::npos) << message;
    EXPECT_NE(message.find("UC_ERR_READ_UNMAPPED"), std::string::npos) << message;
    EXPECT_NE(message.find("(read of unmapped memory at 0x5000000000)"), std::string::npos) << message;

    ASSERT_TRUE(machine.GetLastFault().has_value());
    EXPECT_EQ(machine.GetLastFault()->Address, UnmappedAddress);
    EXPECT_EQ(machine.GetLastFault()->Rip, CodeBase + 10);
    EXPECT_EQ(machine.GetLastFault()->Kind, "read of unmapped memory");
}

TEST(MachineTest, ReportsUnmappedWritesAndFetches)
{
    Machine machine;
    LoadCode(machine, CodeBase, Concat({ MovRaxImm64(UnmappedAddress + 0x18), { 0x48, 0x89, 0x08, 0xC3 } }));
    LoadCode(machine, CodeBase + 0x40, Concat({ MovRaxImm64(UnmappedAddress), { 0xFF, 0xE0 } }));
    LoadCode(machine, CodeBase + 0x80, { 0xB8, 0x05, 0x00, 0x00, 0x00, 0xC3 });

    std::string const write = CallError(machine, CodeBase, {}, 100);
    EXPECT_NE(write.find("UC_ERR_WRITE_UNMAPPED"), std::string::npos) << write;
    EXPECT_NE(write.find("(write to unmapped memory at 0x5000000018)"), std::string::npos) << write;
    ASSERT_TRUE(machine.GetLastFault().has_value());
    EXPECT_EQ(machine.GetLastFault()->Kind, "write to unmapped memory");
    EXPECT_EQ(machine.GetLastFault()->Address, UnmappedAddress + 0x18);
    EXPECT_EQ(machine.GetLastFault()->Rip, CodeBase + 10);

    std::string const fetch = CallError(machine, CodeBase + 0x40, {}, 100);
    EXPECT_NE(fetch.find("UC_ERR_FETCH_UNMAPPED"), std::string::npos) << fetch;
    EXPECT_NE(fetch.find("(fetch of unmapped memory at 0x5000000000)"), std::string::npos) << fetch;
    ASSERT_TRUE(machine.GetLastFault().has_value());
    EXPECT_EQ(machine.GetLastFault()->Kind, "fetch of unmapped memory");
    EXPECT_EQ(machine.GetLastFault()->Address, UnmappedAddress);

    EXPECT_EQ(machine.Call(CodeBase + 0x80, {}, 10), uint64{ 5 });
    EXPECT_FALSE(machine.GetLastFault().has_value());
}

TEST(MachineTest, FaultReportsUseTheAddressDescriber)
{
    Machine machine;
    LoadCode(machine, CodeBase, Concat({ MovRaxImm64(UnmappedAddress), { 0x48, 0x8B, 0x00, 0xC3 } }));
    machine.SetAddressDescriber([](uint64 address)
    {
        if (address >= CodeBase && address < CodeBase + 0x1000)
            return "client.exe+" + std::to_string(address - CodeBase + 0x1000);
        throw std::runtime_error("not a module address");
    });

    EXPECT_EQ(machine.DescribeAddress(CodeBase + 1), "client.exe+4097");
    EXPECT_EQ(machine.DescribeAddress(UnmappedAddress), "0x5000000000");

    std::string const message = CallError(machine, CodeBase, {}, 100);
    EXPECT_NE(message.find("emulation of client.exe+4096 stopped at client.exe+4106"), std::string::npos) << message;

    machine.SetAddressDescriber({});
    EXPECT_EQ(machine.DescribeAddress(CodeBase), "0x140001000");
}

TEST(MachineTest, HookSetsRaxAtAReturnStub)
{
    Machine machine;
    LoadCode(machine, CodeBase, { 0xC3 });

    std::vector<uint64> hits;
    machine.HookCode(CodeBase, CodeBase + 1, [&](uint64 address)
    {
        hits.push_back(address);
        machine.SetRegister(GuestRegister::Rax, 0x1234);
    });

    EXPECT_EQ(machine.Call(CodeBase, {}, 10), uint64{ 0x1234 });
    EXPECT_EQ(machine.Call(CodeBase, {}, 10), uint64{ 0x1234 });
    EXPECT_EQ(hits, (std::vector<uint64>{ CodeBase, CodeBase }));
}

TEST(MachineTest, HookCanStandInForAnImportedFunction)
{
    Machine machine;
    LoadCode(machine, CodeBase, {
        0xB9, 0x05, 0x00, 0x00, 0x00,
        0xE8, 0xF6, 0x00, 0x00, 0x00,
        0x48, 0x83, 0xC0, 0x01,
        0xC3 });
    LoadCode(machine, CodeBase + 0x100, { 0xCC });

    machine.HookCode(CodeBase + 0x100, CodeBase + 0x110, [&](uint64)
    {
        uint64 const rsp = machine.GetRegister(GuestRegister::Rsp);
        uint64 const returnAddress = machine.ReadU64(rsp);
        machine.SetRegister(GuestRegister::Rax, machine.GetArgument(0) + 36);
        machine.SetRegister(GuestRegister::Rsp, rsp + 8);
        machine.Redirect(returnAddress);
    });

    EXPECT_EQ(machine.Call(CodeBase, {}, 20), uint64{ 42 });
}

TEST(MachineTest, RedirectContinuesInAnotherFunction)
{
    Machine machine;
    LoadCode(machine, CodeBase, { 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3 });
    LoadCode(machine, CodeBase + 0x10, { 0xB8, 0x02, 0x00, 0x00, 0x00, 0xC3 });

    EXPECT_EQ(machine.Call(CodeBase, {}, 10), uint64{ 1 });
    machine.HookCode(CodeBase, CodeBase + 1, [&](uint64)
    {
        machine.Redirect(CodeBase + 0x10);
    });
    EXPECT_EQ(machine.Call(CodeBase, {}, 10), uint64{ 2 });
    EXPECT_EQ(machine.Call(CodeBase + 0x10, {}, 10), uint64{ 2 });

    EXPECT_THROW(machine.Redirect(CodeBase), EmulationError);
}

TEST(MachineTest, HooksFireOnlyInsideTheirRange)
{
    Machine machine;
    LoadCode(machine, CodeBase, { 0xB8, 0x01, 0x00, 0x00, 0x00, 0x83, 0xC0, 0x01, 0xC3 });

    std::vector<uint64> narrow;
    std::vector<uint64> wide;
    machine.HookCode(CodeBase + 5, CodeBase + 8, [&](uint64 address) { narrow.push_back(address); });
    machine.HookCode(CodeBase, CodeBase + 9, [&](uint64 address) { wide.push_back(address); });

    EXPECT_EQ(machine.Call(CodeBase, {}, 10), uint64{ 2 });
    EXPECT_EQ(narrow, std::vector<uint64>{ CodeBase + 5 });
    EXPECT_EQ(wide, (std::vector<uint64>{ CodeBase, CodeBase + 5, CodeBase + 8 }));

    EXPECT_THROW(machine.HookCode(CodeBase, CodeBase, [](uint64) {}), EmulationError);
    EXPECT_THROW(machine.HookCode(CodeBase, CodeBase + 1, Machine::CodeHook{}), EmulationError);
}

TEST(MachineTest, HookAddedAfterCodeRanStillFires)
{
    Machine machine;
    LoadCode(machine, CodeBase, { 0xB8, 0x07, 0x00, 0x00, 0x00, 0x83, 0xC0, 0x01, 0xC3 });
    EXPECT_EQ(machine.Call(CodeBase, {}, 10), uint64{ 8 });

    int hits = 0;
    machine.HookCode(CodeBase + 5, CodeBase + 6, [&](uint64)
    {
        ++hits;
        machine.Redirect(CodeBase + 8);
    });
    EXPECT_EQ(machine.Call(CodeBase, {}, 10), uint64{ 7 });
    EXPECT_EQ(hits, 1);
}

TEST(MachineTest, HookExceptionsStopEmulationAndReachTheCaller)
{
    Machine machine;
    LoadCode(machine, CodeBase, { 0xB8, 0x03, 0x00, 0x00, 0x00, 0xEB, 0xFE });
    LoadCode(machine, CodeBase + 0x10, { 0xB8, 0x09, 0x00, 0x00, 0x00, 0xC3 });

    int hits = 0;
    machine.HookCode(CodeBase + 5, CodeBase + 7, [&](uint64)
    {
        ++hits;
        throw std::runtime_error("stub handler failed");
    });

    try
    {
        machine.Call(CodeBase, {}, 1000);
        ADD_FAILURE() << "the hook's exception did not reach the caller";
    }
    catch (std::runtime_error const& error)
    {
        EXPECT_STREQ(error.what(), "stub handler failed");
    }
    EXPECT_EQ(hits, 1);
    EXPECT_EQ(machine.GetRegister(GuestRegister::Rax), uint64{ 3 });
    EXPECT_EQ(machine.Call(CodeBase + 0x10, {}, 10), uint64{ 9 });
}

TEST(MachineTest, CallsFromInsideAHookAreRefused)
{
    Machine machine;
    LoadCode(machine, CodeBase, { 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3 });
    LoadCode(machine, CodeBase + 0x10, { 0xB8, 0x02, 0x00, 0x00, 0x00, 0xC3 });

    std::string inner;
    machine.HookCode(CodeBase, CodeBase + 1, [&](uint64)
    {
        inner = CallError(machine, CodeBase + 0x10, {}, 10);
    });
    EXPECT_EQ(machine.Call(CodeBase, {}, 10), uint64{ 1 });
    EXPECT_EQ(inner, "cannot call 0x140001010 while emulation is running: calls are not reentrant");

    machine.HookCode(CodeBase + 0x10, CodeBase + 0x11, [&](uint64)
    {
        machine.Call(CodeBase, {}, 10);
    });
    std::string const outer = CallError(machine, CodeBase + 0x10, {}, 10);
    EXPECT_NE(outer.find("not reentrant"), std::string::npos) << outer;
    EXPECT_EQ(machine.Call(CodeBase, {}, 10), uint64{ 1 });
}

TEST(MachineTest, ReadsThroughTheGsAndFsBases)
{
    Machine machine;
    machine.Map(DataBase, Machine::PageSize);
    machine.WriteU64(DataBase + 0x30, 0x1122334455667788);
    machine.WriteU64(DataBase + 0x130, 0x99AABBCCDDEEFF00);
    machine.SetRegister(GuestRegister::GsBase, DataBase);
    machine.SetRegister(GuestRegister::FsBase, DataBase + 0x100);
    EXPECT_EQ(machine.GetRegister(GuestRegister::GsBase), DataBase);
    EXPECT_EQ(machine.GetRegister(GuestRegister::FsBase), DataBase + 0x100);

    LoadCode(machine, CodeBase, { 0x65, 0x48, 0x8B, 0x04, 0x25, 0x30, 0x00, 0x00, 0x00, 0xC3 });
    LoadCode(machine, CodeBase + 0x10, { 0x64, 0x48, 0x8B, 0x04, 0x25, 0x30, 0x00, 0x00, 0x00, 0xC3 });
    EXPECT_EQ(machine.Call(CodeBase, {}, 10), uint64{ 0x1122334455667788 });
    EXPECT_EQ(machine.Call(CodeBase + 0x10, {}, 10), uint64{ 0x99AABBCCDDEEFF00 });
}

TEST(MachineTest, XmmRegistersRoundTripAndKeepTheirHighHalf)
{
    Machine machine;
    machine.SetRegister(GuestRegister::Xmm0, 0x1122334455667788);
    machine.SetRegister(GuestRegister::Xmm3, 0x0102030405060708);
    EXPECT_EQ(machine.GetRegister(GuestRegister::Xmm0), uint64{ 0x1122334455667788 });
    EXPECT_EQ(machine.GetRegister(GuestRegister::Xmm3), uint64{ 0x0102030405060708 });

    LoadCode(machine, CodeBase, { 0x66, 0x48, 0x0F, 0x6E, 0xC9, 0x0F, 0x16, 0xC1, 0x66, 0x48, 0x0F, 0x7E, 0xC0, 0xC3 });
    LoadCode(machine, CodeBase + 0x20, { 0x0F, 0x12, 0xC8, 0x66, 0x48, 0x0F, 0x7E, 0xC8, 0xC3 });

    std::array<uint64, 1> const high = { 0xAABBCCDD00112233 };
    EXPECT_EQ(machine.Call(CodeBase, high, 10), uint64{ 0x1122334455667788 });
    machine.SetRegister(GuestRegister::Xmm0, 0x42);
    EXPECT_EQ(machine.GetRegister(GuestRegister::Xmm0), uint64{ 0x42 });
    EXPECT_EQ(machine.Call(CodeBase + 0x20, {}, 10), uint64{ 0xAABBCCDD00112233 });
    EXPECT_EQ(machine.GetRegister(GuestRegister::Xmm1), uint64{ 0xAABBCCDD00112233 });
}

TEST(MachineTest, GeneralRegistersRoundTrip)
{
    Machine machine;
    std::array<GuestRegister, 17> const registers = {
        GuestRegister::Rax, GuestRegister::Rbx, GuestRegister::Rcx, GuestRegister::Rdx, GuestRegister::Rsi, GuestRegister::Rdi,
        GuestRegister::Rbp, GuestRegister::Rsp, GuestRegister::R8, GuestRegister::R9, GuestRegister::R10, GuestRegister::R11,
        GuestRegister::R12, GuestRegister::R13, GuestRegister::R14, GuestRegister::R15, GuestRegister::Rip };
    uint64 value = 0x0123456789ABCDEF;
    for (GuestRegister const reg : registers)
    {
        machine.SetRegister(reg, value);
        EXPECT_EQ(machine.GetRegister(reg), value) << static_cast<int>(reg);
        value = value * 3 + 1;
    }
    machine.SetRegister(GuestRegister::Rflags, 0x203);
    EXPECT_EQ(machine.GetRegister(GuestRegister::Rflags), uint64{ 0x203 });
}

TEST(MachineTest, ReadsCStringsUpToTheEndOfAMapping)
{
    Machine machine;
    machine.Map(DataBase, 2 * Machine::PageSize);
    uint64 const end = DataBase + 2 * Machine::PageSize;

    machine.Write(end - 6, TextBytes(std::string_view("hello\0", 6)));
    EXPECT_EQ(machine.ReadCString(end - 6, 16), "hello");
    EXPECT_EQ(machine.ReadCString(end - 6, 5), "hello");
    EXPECT_FALSE(machine.ReadCString(end - 6, 4).has_value());
    EXPECT_EQ(machine.ReadCString(end - 1, 0), "");

    machine.Write(end - 3, TextBytes("abc"));
    EXPECT_FALSE(machine.ReadCString(end - 3, 100).has_value());
    EXPECT_FALSE(machine.ReadCString(end, 100).has_value());
    EXPECT_FALSE(machine.ReadCString(UnmappedAddress, 100).has_value());
    EXPECT_FALSE(machine.ReadCString(std::numeric_limits<uint64>::max(), 100).has_value());

    std::string const longText(300, 'x');
    uint64 const straddle = DataBase + Machine::PageSize - 100;
    machine.Write(straddle, TextBytes(longText));
    machine.WriteU8(straddle + longText.size(), 0);
    EXPECT_EQ(machine.ReadCString(straddle, 1000), longText);
    EXPECT_EQ(machine.ReadCString(straddle, 300), longText);
    EXPECT_FALSE(machine.ReadCString(straddle, 299).has_value());
    EXPECT_EQ(machine.ReadCString(straddle + 299, std::numeric_limits<std::size_t>::max()), "x");
}

TEST(MachineTest, ReadsWideStringsUpToTheEndOfAMapping)
{
    Machine machine;
    machine.Map(DataBase, 2 * Machine::PageSize);
    uint64 const end = DataBase + 2 * Machine::PageSize;

    machine.Write(end - 10, WideBytes(std::u16string_view(u"wide\0", 5)));
    EXPECT_EQ(machine.ReadWideString(end - 10, 16), u"wide");
    EXPECT_EQ(machine.ReadWideString(end - 10, 4), u"wide");
    EXPECT_FALSE(machine.ReadWideString(end - 10, 3).has_value());
    EXPECT_EQ(machine.ReadWideString(end - 2, 0), u"");

    machine.Write(end - 4, WideBytes(u"qr"));
    EXPECT_FALSE(machine.ReadWideString(end - 4, 100).has_value());
    EXPECT_FALSE(machine.ReadWideString(end - 1, 100).has_value());
    EXPECT_FALSE(machine.ReadWideString(UnmappedAddress, 100).has_value());

    uint64 const odd = DataBase + Machine::PageSize - 3;
    std::u16string const text = { 0x0041, 0x4E2D, 0x0043 };
    std::vector<uint8> bytes = WideBytes(text);
    bytes.push_back(0);
    bytes.push_back(0);
    machine.Write(odd, bytes);
    EXPECT_EQ(machine.ReadWideString(odd, 100), text);
    EXPECT_EQ(machine.ReadWideString(odd, 3), text);
    EXPECT_FALSE(machine.ReadWideString(odd, 2).has_value());

    std::u16string const longText(700, u'y');
    uint64 const straddle = DataBase + Machine::PageSize - 201;
    std::vector<uint8> longBytes = WideBytes(longText);
    longBytes.push_back(0);
    longBytes.push_back(0);
    machine.Write(straddle, longBytes);
    EXPECT_EQ(machine.ReadWideString(straddle, 700), longText);
    EXPECT_FALSE(machine.ReadWideString(straddle, 699).has_value());
}

TEST(MachineTest, MapAlignsToPagesAndRefusesOverlaps)
{
    Machine machine;
    machine.Map(DataBase + 0x123, 1);
    EXPECT_TRUE(machine.IsMapped(DataBase, Machine::PageSize));
    EXPECT_TRUE(machine.IsMapped(DataBase + 0xFFF, 0));
    EXPECT_FALSE(machine.IsMapped(DataBase, Machine::PageSize + 1));
    EXPECT_FALSE(machine.IsMapped(DataBase + Machine::PageSize, 0));

    try
    {
        machine.Map(DataBase + 0xFFF, 2);
        ADD_FAILURE() << "an overlapping map was accepted";
    }
    catch (EmulationError const& error)
    {
        std::string const message = error.what();
        EXPECT_NE(message.find("cannot map guest memory 0x20000000-0x20001fff: it overlaps the mapped region 0x20000000-0x20000fff"), std::string::npos) << message;
        EXPECT_NE(message.find("UC_ERR_MAP"), std::string::npos) << message;
    }
    EXPECT_FALSE(machine.IsMapped(DataBase + Machine::PageSize, 1));

    machine.Map(DataBase + Machine::PageSize, Machine::PageSize);
    EXPECT_TRUE(machine.IsMapped(DataBase + 0x800, Machine::PageSize));
    EXPECT_TRUE(machine.IsMapped(DataBase, 2 * Machine::PageSize));
    EXPECT_FALSE(machine.IsMapped(DataBase - 1, 2));

    EXPECT_THROW(machine.Map(DataBase + 0x10000, 0), EmulationError);
    EXPECT_THROW(machine.Map(std::numeric_limits<uint64>::max() - 10, 100), EmulationError);
    EXPECT_THROW(machine.Map(0, std::numeric_limits<uint64>::max()), EmulationError);
    EXPECT_FALSE(machine.IsMapped(std::numeric_limits<uint64>::max(), 2));
}

TEST(MachineTest, ReadAndWriteErrorsNameTheAddressAndSize)
{
    Machine machine;
    machine.Map(DataBase, Machine::PageSize);

    machine.WriteU32(DataBase, 0x11223344);
    EXPECT_EQ(machine.ReadU8(DataBase), 0x44);
    EXPECT_EQ(machine.ReadU16(DataBase + 1), 0x2233);
    EXPECT_EQ(machine.ReadU32(DataBase), uint32{ 0x11223344 });
    EXPECT_EQ(machine.ReadBytes(DataBase, 4), (std::vector<uint8>{ 0x44, 0x33, 0x22, 0x11 }));
    machine.WriteU16(DataBase + 8, 0xBEEF);
    machine.WriteU8(DataBase + 10, 0x7F);
    machine.WriteU64(DataBase + 0xFF8, 0xFEDCBA9876543210);
    EXPECT_EQ(machine.ReadU64(DataBase + 0xFF8), uint64{ 0xFEDCBA9876543210 });
    EXPECT_EQ(machine.ReadU32(DataBase + 8), uint32{ 0x7FBEEF });

    std::array<uint8, 8> buffer{};
    EXPECT_TRUE(machine.TryRead(DataBase + 0xFF8, buffer));
    EXPECT_FALSE(machine.TryRead(DataBase + 0xFFC, buffer));
    EXPECT_FALSE(machine.TryRead(std::numeric_limits<uint64>::max() - 2, buffer));
    EXPECT_TRUE(machine.TryRead(UnmappedAddress, std::span<uint8>()));

    try
    {
        machine.Read(DataBase + 0xFFC, buffer);
        ADD_FAILURE() << "a read past the mapping succeeded";
    }
    catch (EmulationError const& error)
    {
        std::string const message = error.what();
        EXPECT_NE(message.find("cannot read 8 bytes of guest memory at 0x20000ffc"), std::string::npos) << message;
    }

    try
    {
        std::array<uint8, 16> const data{};
        machine.Write(DataBase + 0xFF8, data);
        ADD_FAILURE() << "a write past the mapping succeeded";
    }
    catch (EmulationError const& error)
    {
        std::string const message = error.what();
        EXPECT_NE(message.find("cannot write 16 bytes of guest memory at 0x20000ff8"), std::string::npos) << message;
    }

    EXPECT_THROW(machine.ReadU64(UnmappedAddress), EmulationError);
    EXPECT_THROW(machine.WriteU64(UnmappedAddress, 1), EmulationError);
    EXPECT_THROW(machine.ReadBytes(DataBase, std::size_t{ 1 } << 40), EmulationError);
    EXPECT_THROW(machine.ReadU64(std::numeric_limits<uint64>::max() - 3), EmulationError);
    EXPECT_TRUE(machine.ReadBytes(UnmappedAddress, 0).empty());
}

TEST(MachineTest, ArgumentsPastTheAddressSpaceAreRefused)
{
    Machine machine;
    machine.SetRegister(GuestRegister::Rsp, std::numeric_limits<uint64>::max() - 0x10);
    EXPECT_THROW(machine.GetArgument(4), EmulationError);
    EXPECT_THROW(machine.GetArgument(std::numeric_limits<std::size_t>::max()), EmulationError);
    machine.SetRegister(GuestRegister::Rcx, 5);
    EXPECT_EQ(machine.GetArgument(0), uint64{ 5 });
}
