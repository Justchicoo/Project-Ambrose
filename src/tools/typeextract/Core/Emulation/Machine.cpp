/*
 * Project Ambrose by Imjustchico
 * Drives Unicorn's x86-64 engine: page-aligned mappings checked against the engine's regions, bounds-checked reads, writes and strings, full-width XMM register access, code hooks whose exceptions stop emulation and resurface in the caller, and budgeted calls that return through a sentinel page or report where and why they stopped.
 */

#include "Machine.h"

#include <fmt/format.h>
#include <unicorn/unicorn.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <limits>
#include <string_view>
#include <utility>

namespace
{
    constexpr uint64 MaxAddress = std::numeric_limits<uint64>::max();
    constexpr uint64 PageMask = Machine::PageSize - 1;
    constexpr uint64 HomeSpaceSize = 0x20;
    constexpr uint64 MaxStackArguments = (Machine::CallFrameHeadroom - HomeSpaceSize) / 8;
    constexpr std::size_t InitialStringChunk = 256;
    constexpr uint8 ReturnOpcode = 0xC3;
    constexpr std::array<GuestRegister, 4> ArgumentRegisters = { GuestRegister::Rcx, GuestRegister::Rdx, GuestRegister::R8, GuestRegister::R9 };

    struct RegionList
    {
        uc_mem_region* Regions = nullptr;
        uint32 Count = 0;

        RegionList() = default;
        RegionList(RegionList const&) = delete;
        RegionList& operator=(RegionList const&) = delete;

        ~RegionList()
        {
            if (Regions)
                uc_free(Regions);
        }
    };

    std::vector<uc_mem_region> GetRegions(uc_engine* engine)
    {
        RegionList list;
        uc_err const result = uc_mem_regions(engine, &list.Regions, &list.Count);
        if (result != UC_ERR_OK)
            throw EmulationError(fmt::format("cannot list the guest memory regions: {}", uc_strerror(result)));
        std::vector<uc_mem_region> regions;
        if (list.Regions && list.Count > 0)
            regions.assign(list.Regions, list.Regions + list.Count);
        std::sort(regions.begin(), regions.end(), [](uc_mem_region const& left, uc_mem_region const& right) { return left.begin < right.begin; });
        return regions;
    }

    bool FitsAddressSpace(uint64 address, uint64 size) noexcept
    {
        return size == 0 || size - 1 <= MaxAddress - address;
    }

    int ToUnicornRegister(GuestRegister reg) noexcept
    {
        switch (reg)
        {
            case GuestRegister::Rax: return UC_X86_REG_RAX;
            case GuestRegister::Rbx: return UC_X86_REG_RBX;
            case GuestRegister::Rcx: return UC_X86_REG_RCX;
            case GuestRegister::Rdx: return UC_X86_REG_RDX;
            case GuestRegister::Rsi: return UC_X86_REG_RSI;
            case GuestRegister::Rdi: return UC_X86_REG_RDI;
            case GuestRegister::Rbp: return UC_X86_REG_RBP;
            case GuestRegister::Rsp: return UC_X86_REG_RSP;
            case GuestRegister::R8: return UC_X86_REG_R8;
            case GuestRegister::R9: return UC_X86_REG_R9;
            case GuestRegister::R10: return UC_X86_REG_R10;
            case GuestRegister::R11: return UC_X86_REG_R11;
            case GuestRegister::R12: return UC_X86_REG_R12;
            case GuestRegister::R13: return UC_X86_REG_R13;
            case GuestRegister::R14: return UC_X86_REG_R14;
            case GuestRegister::R15: return UC_X86_REG_R15;
            case GuestRegister::Rip: return UC_X86_REG_RIP;
            case GuestRegister::Rflags: return UC_X86_REG_RFLAGS;
            case GuestRegister::GsBase: return UC_X86_REG_GS_BASE;
            case GuestRegister::FsBase: return UC_X86_REG_FS_BASE;
            case GuestRegister::Xmm0: return UC_X86_REG_XMM0;
            case GuestRegister::Xmm1: return UC_X86_REG_XMM1;
            case GuestRegister::Xmm2: return UC_X86_REG_XMM2;
            case GuestRegister::Xmm3: return UC_X86_REG_XMM3;
        }
        return UC_X86_REG_INVALID;
    }

    std::string_view GetRegisterName(GuestRegister reg) noexcept
    {
        switch (reg)
        {
            case GuestRegister::Rax: return "rax";
            case GuestRegister::Rbx: return "rbx";
            case GuestRegister::Rcx: return "rcx";
            case GuestRegister::Rdx: return "rdx";
            case GuestRegister::Rsi: return "rsi";
            case GuestRegister::Rdi: return "rdi";
            case GuestRegister::Rbp: return "rbp";
            case GuestRegister::Rsp: return "rsp";
            case GuestRegister::R8: return "r8";
            case GuestRegister::R9: return "r9";
            case GuestRegister::R10: return "r10";
            case GuestRegister::R11: return "r11";
            case GuestRegister::R12: return "r12";
            case GuestRegister::R13: return "r13";
            case GuestRegister::R14: return "r14";
            case GuestRegister::R15: return "r15";
            case GuestRegister::Rip: return "rip";
            case GuestRegister::Rflags: return "rflags";
            case GuestRegister::GsBase: return "gs base";
            case GuestRegister::FsBase: return "fs base";
            case GuestRegister::Xmm0: return "xmm0";
            case GuestRegister::Xmm1: return "xmm1";
            case GuestRegister::Xmm2: return "xmm2";
            case GuestRegister::Xmm3: return "xmm3";
        }
        return "unknown register";
    }

    bool IsXmmRegister(GuestRegister reg) noexcept
    {
        return reg == GuestRegister::Xmm0 || reg == GuestRegister::Xmm1 || reg == GuestRegister::Xmm2 || reg == GuestRegister::Xmm3;
    }

    char const* GetInvalidAccessKind(uc_mem_type type) noexcept
    {
        switch (type)
        {
            case UC_MEM_READ_UNMAPPED: return "read of unmapped memory";
            case UC_MEM_WRITE_UNMAPPED: return "write to unmapped memory";
            case UC_MEM_FETCH_UNMAPPED: return "fetch of unmapped memory";
            case UC_MEM_READ_PROT: return "read of protected memory";
            case UC_MEM_WRITE_PROT: return "write to protected memory";
            case UC_MEM_FETCH_PROT: return "fetch of protected memory";
            default: return "invalid memory access";
        }
    }

    template <typename T>
    T ReadInteger(Machine const& machine, uint64 address)
    {
        std::array<uint8, sizeof(T)> bytes{};
        machine.Read(address, bytes);
        uint64 value = 0;
        for (std::size_t index = sizeof(T); index-- > 0;)
            value = (value << 8) | bytes[index];
        return static_cast<T>(value);
    }

    template <typename T>
    void WriteInteger(Machine& machine, uint64 address, T value)
    {
        std::array<uint8, sizeof(T)> bytes{};
        for (std::size_t index = 0; index < sizeof(T); ++index)
            bytes[index] = static_cast<uint8>(static_cast<uint64>(value) >> (8 * index));
        machine.Write(address, bytes);
    }

    std::size_t NextStringChunk(uint64 cursor, std::size_t hint) noexcept
    {
        return static_cast<std::size_t>(std::min<uint64>(Machine::PageSize - (cursor & PageMask), hint));
    }
}

struct Machine::State
{
    struct CodeHookEntry
    {
        State* Owner = nullptr;
        CodeHook Hook;
        uc_hook Handle = 0;
    };

    uc_engine* Engine = nullptr;
    uc_hook InvalidMemoryHook = 0;
    bool InvalidMemoryHooked = false;
    std::vector<std::unique_ptr<CodeHookEntry>> CodeHooks;
    AddressDescriber Describer;
    std::optional<GuestFault> LastFault;
    std::exception_ptr HookException;
    uint32 HookDepth = 0;
    bool Running = false;
    bool HasRun = false;
    bool TranslationStale = false;
    bool CallRegionsReady = false;

    State() = default;
    State(State const&) = delete;
    State& operator=(State const&) = delete;

    ~State()
    {
        if (!Engine)
            return;
        for (std::unique_ptr<CodeHookEntry> const& entry : CodeHooks)
            uc_hook_del(Engine, entry->Handle);
        if (InvalidMemoryHooked)
            uc_hook_del(Engine, InvalidMemoryHook);
        uc_close(Engine);
    }

    static void OnCode(uc_engine* engine, uint64 address, uint32, void* userData)
    {
        CodeHookEntry& entry = *static_cast<CodeHookEntry*>(userData);
        State& state = *entry.Owner;
        if (state.HookException)
            return;
        ++state.HookDepth;
        try
        {
            entry.Hook(address);
        }
        catch (...)
        {
            state.HookException = std::current_exception();
            uc_emu_stop(engine);
        }
        --state.HookDepth;
    }

    static bool OnInvalidMemory(uc_engine* engine, uc_mem_type type, uint64 address, int, int64, void* userData)
    {
        State& state = *static_cast<State*>(userData);
        if (state.LastFault)
            return false;
        try
        {
            uint64 rip = 0;
            uc_reg_read(engine, UC_X86_REG_RIP, &rip);
            state.LastFault = GuestFault{ rip, address, GetInvalidAccessKind(type) };
        }
        catch (...)
        {
            if (!state.HookException)
                state.HookException = std::current_exception();
        }
        return false;
    }
};

Machine::Machine()
    : _state(std::make_unique<State>())
{
    uc_err const opened = uc_open(UC_ARCH_X86, UC_MODE_64, &_state->Engine);
    if (opened != UC_ERR_OK)
    {
        _state->Engine = nullptr;
        throw EmulationError(fmt::format("cannot create the x86-64 emulator: {}", uc_strerror(opened)));
    }
    uc_cb_eventmem_t const onInvalidMemory = &State::OnInvalidMemory;
    uc_err const hooked = uc_hook_add(_state->Engine, &_state->InvalidMemoryHook, UC_HOOK_MEM_INVALID, reinterpret_cast<void*>(onInvalidMemory), _state.get(), 1, 0);
    if (hooked != UC_ERR_OK)
        throw EmulationError(fmt::format("cannot watch the emulator for invalid memory access: {}", uc_strerror(hooked)));
    _state->InvalidMemoryHooked = true;
}

Machine::~Machine() = default;

void Machine::Map(uint64 address, uint64 size)
{
    if (size == 0)
        throw EmulationError(fmt::format("cannot map guest memory at {:#x}: the size is zero", address));
    if (!FitsAddressSpace(address, size))
        throw EmulationError(fmt::format("cannot map {:#x} bytes of guest memory at {:#x}: the range passes the end of the address space", size, address));
    uint64 const begin = address & ~PageMask;
    uint64 const lastPage = (address + size - 1) & ~PageMask;
    uint64 const last = lastPage + PageMask;
    if (lastPage - begin > MaxAddress - PageSize)
        throw EmulationError(fmt::format("cannot map guest memory {:#x}-{:#x}: the range covers the whole address space", begin, last));
    uint64 const length = lastPage - begin + PageSize;
    for (uc_mem_region const& region : GetRegions(_state->Engine))
    {
        if (region.begin <= last && region.end >= begin)
            throw EmulationError(fmt::format("cannot map guest memory {:#x}-{:#x}: it overlaps the mapped region {:#x}-{:#x} ({})", begin, last, region.begin, region.end, uc_strerror(UC_ERR_MAP)));
    }
    uc_err const mapped = uc_mem_map(_state->Engine, begin, length, UC_PROT_ALL);
    if (mapped != UC_ERR_OK)
        throw EmulationError(fmt::format("cannot map guest memory {:#x}-{:#x}: {}", begin, last, uc_strerror(mapped)));
}

bool Machine::IsMapped(uint64 address, uint64 size) const
{
    uint64 const length = std::max<uint64>(size, 1);
    if (!FitsAddressSpace(address, length))
        return false;
    uint64 const last = address + length - 1;
    uint64 cursor = address;
    for (uc_mem_region const& region : GetRegions(_state->Engine))
    {
        if (region.end < cursor)
            continue;
        if (region.begin > cursor)
            return false;
        if (region.end >= last)
            return true;
        cursor = region.end + 1;
    }
    return false;
}

void Machine::Write(uint64 address, std::span<uint8 const> data)
{
    if (data.empty())
        return;
    uc_err const result = FitsAddressSpace(address, data.size()) ? uc_mem_write(_state->Engine, address, data.data(), data.size()) : UC_ERR_WRITE_UNMAPPED;
    if (result != UC_ERR_OK)
        throw EmulationError(fmt::format("cannot write {} bytes of guest memory at {}: {}", data.size(), DescribeAddress(address), uc_strerror(result)));
}

void Machine::Read(uint64 address, std::span<uint8> out) const
{
    if (out.empty())
        return;
    uc_err const result = FitsAddressSpace(address, out.size()) ? uc_mem_read(_state->Engine, address, out.data(), out.size()) : UC_ERR_READ_UNMAPPED;
    if (result != UC_ERR_OK)
        throw EmulationError(fmt::format("cannot read {} bytes of guest memory at {}: {}", out.size(), DescribeAddress(address), uc_strerror(result)));
}

bool Machine::TryRead(uint64 address, std::span<uint8> out) const noexcept
{
    if (out.empty())
        return true;
    if (!FitsAddressSpace(address, out.size()))
        return false;
    return uc_mem_read(_state->Engine, address, out.data(), out.size()) == UC_ERR_OK;
}

std::vector<uint8> Machine::ReadBytes(uint64 address, std::size_t size) const
{
    if (size > PageSize && !IsMapped(address, size))
        throw EmulationError(fmt::format("cannot read {} bytes of guest memory at {}: {}", size, DescribeAddress(address), uc_strerror(UC_ERR_READ_UNMAPPED)));
    std::vector<uint8> bytes(size);
    Read(address, bytes);
    return bytes;
}

uint8 Machine::ReadU8(uint64 address) const
{
    return ReadInteger<uint8>(*this, address);
}

uint16 Machine::ReadU16(uint64 address) const
{
    return ReadInteger<uint16>(*this, address);
}

uint32 Machine::ReadU32(uint64 address) const
{
    return ReadInteger<uint32>(*this, address);
}

uint64 Machine::ReadU64(uint64 address) const
{
    return ReadInteger<uint64>(*this, address);
}

void Machine::WriteU8(uint64 address, uint8 value)
{
    WriteInteger(*this, address, value);
}

void Machine::WriteU16(uint64 address, uint16 value)
{
    WriteInteger(*this, address, value);
}

void Machine::WriteU32(uint64 address, uint32 value)
{
    WriteInteger(*this, address, value);
}

void Machine::WriteU64(uint64 address, uint64 value)
{
    WriteInteger(*this, address, value);
}

std::optional<std::string> Machine::ReadCString(uint64 address, std::size_t limit) const
{
    std::string text;
    std::array<uint8, PageSize> buffer{};
    std::size_t hint = InitialStringChunk;
    uint64 cursor = address;
    for (;;)
    {
        std::size_t chunk = NextStringChunk(cursor, hint);
        std::size_t const allowed = limit - text.size();
        if (allowed < chunk)
            chunk = allowed + 1;
        if (!TryRead(cursor, std::span<uint8>(buffer.data(), chunk)))
            return std::nullopt;
        uint8 const* const terminator = static_cast<uint8 const*>(std::memchr(buffer.data(), 0, chunk));
        if (terminator)
        {
            text.append(reinterpret_cast<char const*>(buffer.data()), static_cast<std::size_t>(terminator - buffer.data()));
            return text;
        }
        if (chunk > allowed || MaxAddress - cursor < chunk)
            return std::nullopt;
        text.append(reinterpret_cast<char const*>(buffer.data()), chunk);
        cursor += chunk;
        hint = std::min<std::size_t>(hint * 2, PageSize);
    }
}

std::optional<std::u16string> Machine::ReadWideString(uint64 address, std::size_t limit) const
{
    std::u16string text;
    std::array<uint8, PageSize> buffer{};
    std::optional<uint8> low;
    std::size_t hint = InitialStringChunk;
    uint64 cursor = address;
    for (;;)
    {
        std::size_t chunk = NextStringChunk(cursor, hint);
        std::size_t const allowed = limit - text.size();
        if (allowed < chunk)
            chunk = std::min(chunk, (allowed + 1) * 2 - (low ? 1 : 0));
        if (!TryRead(cursor, std::span<uint8>(buffer.data(), chunk)))
            return std::nullopt;
        for (std::size_t index = 0; index < chunk; ++index)
        {
            if (!low)
            {
                low = buffer[index];
                continue;
            }
            char16_t const unit = static_cast<char16_t>(*low | (buffer[index] << 8));
            low.reset();
            if (unit == 0)
                return text;
            if (text.size() >= limit)
                return std::nullopt;
            text.push_back(unit);
        }
        if (MaxAddress - cursor < chunk)
            return std::nullopt;
        cursor += chunk;
        hint = std::min<std::size_t>(hint * 2, PageSize);
    }
}

uint64 Machine::GetRegister(GuestRegister reg) const
{
    int const id = ToUnicornRegister(reg);
    if (IsXmmRegister(reg))
    {
        std::array<uint64, 2> value{};
        std::size_t size = sizeof(value);
        uc_err const result = uc_reg_read2(_state->Engine, id, value.data(), &size);
        if (result != UC_ERR_OK)
            throw EmulationError(fmt::format("cannot read the guest register {}: {}", GetRegisterName(reg), uc_strerror(result)));
        return value[0];
    }
    uint64 value = 0;
    std::size_t size = sizeof(value);
    uc_err const result = uc_reg_read2(_state->Engine, id, &value, &size);
    if (result != UC_ERR_OK)
        throw EmulationError(fmt::format("cannot read the guest register {}: {}", GetRegisterName(reg), uc_strerror(result)));
    return value;
}

void Machine::SetRegister(GuestRegister reg, uint64 value)
{
    int const id = ToUnicornRegister(reg);
    if (IsXmmRegister(reg))
    {
        std::array<uint64, 2> full{};
        std::size_t size = sizeof(full);
        uc_err result = uc_reg_read2(_state->Engine, id, full.data(), &size);
        if (result == UC_ERR_OK)
        {
            full[0] = value;
            size = sizeof(full);
            result = uc_reg_write2(_state->Engine, id, full.data(), &size);
        }
        if (result != UC_ERR_OK)
            throw EmulationError(fmt::format("cannot write the guest register {}: {}", GetRegisterName(reg), uc_strerror(result)));
        return;
    }
    uc_err const result = uc_reg_write(_state->Engine, id, &value);
    if (result != UC_ERR_OK)
        throw EmulationError(fmt::format("cannot write the guest register {}: {}", GetRegisterName(reg), uc_strerror(result)));
}

uint64 Machine::GetArgument(std::size_t index) const
{
    if (index < ArgumentRegisters.size())
        return GetRegister(ArgumentRegisters[index]);
    uint64 const rsp = GetRegister(GuestRegister::Rsp);
    if (index >= MaxAddress / 8)
        throw EmulationError(fmt::format("cannot read argument {}: its stack slot lies past the end of the address space", index));
    uint64 const offset = 8 * (static_cast<uint64>(index) + 1);
    if (offset > MaxAddress - rsp || MaxAddress - rsp - offset < 7)
        throw EmulationError(fmt::format("cannot read argument {} above rsp {:#x}: its stack slot lies past the end of the address space", index, rsp));
    return ReadU64(rsp + offset);
}

void Machine::HookCode(uint64 begin, uint64 end, CodeHook hook)
{
    if (begin >= end)
        throw EmulationError(fmt::format("cannot hook the empty code range {:#x}-{:#x}", begin, end));
    if (!hook)
        throw EmulationError(fmt::format("cannot hook the code range {:#x}-{:#x} without a handler", begin, end));
    auto entry = std::make_unique<State::CodeHookEntry>();
    entry->Owner = _state.get();
    entry->Hook = std::move(hook);
    _state->CodeHooks.reserve(_state->CodeHooks.size() + 1);
    uc_cb_hookcode_t const onCode = &State::OnCode;
    uc_err const added = uc_hook_add(_state->Engine, &entry->Handle, UC_HOOK_CODE, reinterpret_cast<void*>(onCode), entry.get(), begin, end - 1);
    if (added != UC_ERR_OK)
        throw EmulationError(fmt::format("cannot hook the code range {:#x}-{:#x}: {}", begin, end, uc_strerror(added)));
    _state->CodeHooks.push_back(std::move(entry));
    if (_state->HasRun)
        _state->TranslationStale = true;
}

void Machine::Redirect(uint64 address)
{
    if (_state->HookDepth == 0)
        throw EmulationError(fmt::format("cannot redirect execution to {} outside a code hook", DescribeAddress(address)));
    SetRegister(GuestRegister::Rip, address);
}

void Machine::SetAddressDescriber(AddressDescriber describer)
{
    _state->Describer = std::move(describer);
}

std::string Machine::DescribeAddress(uint64 address) const
{
    if (!_state->Describer)
        return fmt::format("{:#x}", address);
    try
    {
        return _state->Describer(address);
    }
    catch (std::exception const&)
    {
        return fmt::format("{:#x}", address);
    }
}

uint64 Machine::Call(uint64 function, std::span<uint64 const> arguments, uint64 instructionBudget)
{
    State& state = *_state;
    if (state.Running)
        throw EmulationError(fmt::format("cannot call {} while emulation is running: calls are not reentrant", DescribeAddress(function)));
    if (arguments.size() > ArgumentRegisters.size() + MaxStackArguments)
        throw EmulationError(fmt::format("cannot call {} with {} arguments: at most {} fit in the call frame", DescribeAddress(function), arguments.size(), ArgumentRegisters.size() + MaxStackArguments));

    if (!state.CallRegionsReady)
    {
        if (!IsMapped(StackBase, StackSize))
            Map(StackBase, StackSize);
        if (!IsMapped(SentinelAddress, PageSize))
            Map(SentinelAddress, PageSize);
        std::vector<uint8> const returns(PageSize, ReturnOpcode);
        Write(SentinelAddress, returns);
        state.CallRegionsReady = true;
    }

    uint64 rsp = (StackBase + StackSize - CallFrameHeadroom) & ~uint64{ 0xF };
    for (std::size_t index = ArgumentRegisters.size(); index < arguments.size(); ++index)
        WriteU64(rsp + HomeSpaceSize + 8 * (index - ArgumentRegisters.size()), arguments[index]);
    rsp -= 8;
    WriteU64(rsp, SentinelAddress);
    SetRegister(GuestRegister::Rsp, rsp);
    for (std::size_t index = 0; index < std::min(arguments.size(), ArgumentRegisters.size()); ++index)
        SetRegister(ArgumentRegisters[index], arguments[index]);
    state.LastFault.reset();
    state.HookException = nullptr;

    if (instructionBudget == 0)
        throw EmulationError(fmt::format("emulation of {} ran out of its 0 instruction budget at {}", DescribeAddress(function), DescribeAddress(function)));

    if (state.TranslationStale)
    {
        uc_err const flushed = uc_ctl_flush_tb(state.Engine);
        if (flushed != UC_ERR_OK)
            throw EmulationError(fmt::format("cannot discard translated code before calling {}: {}", DescribeAddress(function), uc_strerror(flushed)));
        state.TranslationStale = false;
    }

    state.Running = true;
    state.HasRun = true;
    uint64 const count = std::min<uint64>(instructionBudget, std::numeric_limits<std::size_t>::max());
    uc_err const result = uc_emu_start(state.Engine, function, SentinelAddress, 0, static_cast<std::size_t>(count));
    state.Running = false;

    if (state.HookException)
        std::rethrow_exception(std::exchange(state.HookException, nullptr));

    uint64 const rip = GetRegister(GuestRegister::Rip);
    if (result != UC_ERR_OK)
    {
        std::string message = fmt::format("emulation of {} stopped at {}: {}", DescribeAddress(function), DescribeAddress(rip), uc_strerror(result));
        if (state.LastFault)
            message += fmt::format(" ({} at {})", state.LastFault->Kind, DescribeAddress(state.LastFault->Address));
        throw EmulationError(message);
    }
    if (rip != SentinelAddress)
        throw EmulationError(fmt::format("emulation of {} ran out of its {} instruction budget at {}", DescribeAddress(function), instructionBudget, DescribeAddress(rip)));
    return GetRegister(GuestRegister::Rax);
}

std::optional<GuestFault> const& Machine::GetLastFault() const noexcept
{
    return _state->LastFault;
}
