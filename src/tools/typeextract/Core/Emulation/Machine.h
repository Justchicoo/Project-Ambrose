/*
 * Project Ambrose by Imjustchico
 * An x86-64 guest machine over Unicorn: read-write-execute memory regions, typed reads and writes, registers and Windows x64 call arguments, code hooks over address ranges that may redirect execution, and calls that must return to a sentinel within an instruction budget or report the fault.
 */

#ifndef AMBROSE_MACHINE_H
#define AMBROSE_MACHINE_H

#include "Types.h"

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

enum class GuestRegister
{
    Rax,
    Rbx,
    Rcx,
    Rdx,
    Rsi,
    Rdi,
    Rbp,
    Rsp,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,
    Rip,
    Rflags,
    GsBase,
    FsBase,
    Xmm0,
    Xmm1,
    Xmm2,
    Xmm3
};

class EmulationError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

struct GuestFault
{
    uint64 Rip = 0;
    uint64 Address = 0;
    std::string Kind;
};

class Machine
{
public:
    static constexpr uint64 PageSize = 0x1000;
    static constexpr uint64 SentinelAddress = 0x7FFC00000000;
    static constexpr uint64 StackBase = 0x7FFE00000000;
    static constexpr uint64 StackSize = 0x2000000;
    static constexpr uint64 CallFrameHeadroom = 0x10000;

    using CodeHook = std::function<void(uint64 address)>;
    using AddressDescriber = std::function<std::string(uint64 address)>;

    Machine();
    ~Machine();

    Machine(Machine const&) = delete;
    Machine& operator=(Machine const&) = delete;

    void Map(uint64 address, uint64 size);
    bool IsMapped(uint64 address, uint64 size) const;

    void Write(uint64 address, std::span<uint8 const> data);
    void Read(uint64 address, std::span<uint8> out) const;
    bool TryRead(uint64 address, std::span<uint8> out) const noexcept;
    std::vector<uint8> ReadBytes(uint64 address, std::size_t size) const;

    uint8 ReadU8(uint64 address) const;
    uint16 ReadU16(uint64 address) const;
    uint32 ReadU32(uint64 address) const;
    uint64 ReadU64(uint64 address) const;
    void WriteU8(uint64 address, uint8 value);
    void WriteU16(uint64 address, uint16 value);
    void WriteU32(uint64 address, uint32 value);
    void WriteU64(uint64 address, uint64 value);

    std::optional<std::string> ReadCString(uint64 address, std::size_t limit) const;
    std::optional<std::u16string> ReadWideString(uint64 address, std::size_t limit) const;

    uint64 GetRegister(GuestRegister reg) const;
    void SetRegister(GuestRegister reg, uint64 value);
    uint64 GetArgument(std::size_t index) const;

    void HookCode(uint64 begin, uint64 end, CodeHook hook);
    void Redirect(uint64 address);
    void SetAddressDescriber(AddressDescriber describer);
    std::string DescribeAddress(uint64 address) const;

    uint64 Call(uint64 function, std::span<uint64 const> arguments, uint64 instructionBudget);
    std::optional<GuestFault> const& GetLastFault() const noexcept;

private:
    struct State;
    std::unique_ptr<State> _state;
};

#endif
