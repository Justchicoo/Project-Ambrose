/*
 * Project Ambrose by Imjustchico
 * Indexes the executable sections of a PE image for discovery: RIP-relative lea references, direct call sites, RIP-relative indirect calls and jumps through a memory slot, instruction decoding with Zydis from a known instruction start, and the instructions of a function up to an address.
 */

#ifndef AMBROSE_CODEINDEX_H
#define AMBROSE_CODEINDEX_H

#include "Types.h"

#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

class PeImage;

enum class InstructionKind
{
    Call,
    Jump,
    Lea,
    Mov,
    Return,
    Other
};

struct DecodedInstruction
{
    uint64 Address = 0;
    uint8 Length = 0;
    InstructionKind Kind = InstructionKind::Other;
    std::optional<uint64> BranchTarget;
    std::optional<uint64> RipRelativeTarget;
    std::string FirstRegister;
    std::string SecondRegister;
    bool FirstOperandIsMemory = false;
};

class CodeIndex
{
public:
    explicit CodeIndex(PeImage const& image);

    std::span<uint64 const> LeaReferences(uint64 target) const;
    std::span<uint64 const> CallSites(uint64 target) const;
    std::span<uint64 const> IndirectBranchSites(uint64 slot) const;

    std::optional<uint64> FunctionStart(uint64 address) const;
    std::vector<uint64> FindStrings(std::string_view text) const;

    std::vector<DecodedInstruction> Decode(uint64 address, std::size_t maxBytes, std::size_t maxInstructions) const;
    std::vector<DecodedInstruction> DecodeFunctionUntil(uint64 address) const;

    uint64 GetImageBase() const noexcept;
    PeImage const& GetImage() const noexcept;

private:
    PeImage const& _image;
    std::unordered_map<uint64, std::vector<uint64>> _leaReferences;
    std::unordered_map<uint64, std::vector<uint64>> _callSites;
    std::unordered_map<uint64, std::vector<uint64>> _indirectBranchSites;
};

#endif
