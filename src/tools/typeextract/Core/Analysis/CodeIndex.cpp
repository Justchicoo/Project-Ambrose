/*
 * Project Ambrose by Imjustchico
 * Scans the raw bytes of a PE image's executable sections once for RIP-relative lea, direct call and RIP-relative indirect call and jump patterns, keeps sorted sites per target, and decodes instructions with Zydis into their kind, branch and RIP-relative targets and leading operands.
 */

#include "CodeIndex.h"
#include "PeImage.h"

#include <Zydis/Zydis.h>

#include <algorithm>
#include <limits>

namespace
{
    constexpr std::size_t LeaLength = 7;
    constexpr std::size_t DirectCallLength = 5;
    constexpr std::size_t IndirectBranchLength = 6;
    constexpr std::size_t MaxInstructionLength = 15;

    struct AddressRange
    {
        uint64 Begin = 0;
        uint64 End = 0;
    };

    uint64 Displaced(uint64 next, uint8 const* displacement)
    {
        uint32 const raw = uint32{ displacement[0] } | (uint32{ displacement[1] } << 8) | (uint32{ displacement[2] } << 16) | (uint32{ displacement[3] } << 24);
        return next + static_cast<uint64>(static_cast<int64>(static_cast<int32>(raw)));
    }

    std::span<uint64 const> SitesOf(std::unordered_map<uint64, std::vector<uint64>> const& index, uint64 key)
    {
        auto const found = index.find(key);
        if (found == index.end())
            return {};
        return found->second;
    }

    void SortSites(std::unordered_map<uint64, std::vector<uint64>>& index)
    {
        for (auto& entry : index)
            std::sort(entry.second.begin(), entry.second.end());
    }

    InstructionKind KindOf(ZydisMnemonic mnemonic)
    {
        switch (mnemonic)
        {
            case ZYDIS_MNEMONIC_CALL:
                return InstructionKind::Call;
            case ZYDIS_MNEMONIC_JMP:
            case ZYDIS_MNEMONIC_JB:
            case ZYDIS_MNEMONIC_JBE:
            case ZYDIS_MNEMONIC_JCXZ:
            case ZYDIS_MNEMONIC_JECXZ:
            case ZYDIS_MNEMONIC_JKNZD:
            case ZYDIS_MNEMONIC_JKZD:
            case ZYDIS_MNEMONIC_JL:
            case ZYDIS_MNEMONIC_JLE:
            case ZYDIS_MNEMONIC_JNB:
            case ZYDIS_MNEMONIC_JNBE:
            case ZYDIS_MNEMONIC_JNL:
            case ZYDIS_MNEMONIC_JNLE:
            case ZYDIS_MNEMONIC_JNO:
            case ZYDIS_MNEMONIC_JNP:
            case ZYDIS_MNEMONIC_JNS:
            case ZYDIS_MNEMONIC_JNZ:
            case ZYDIS_MNEMONIC_JO:
            case ZYDIS_MNEMONIC_JP:
            case ZYDIS_MNEMONIC_JRCXZ:
            case ZYDIS_MNEMONIC_JS:
            case ZYDIS_MNEMONIC_JZ:
                return InstructionKind::Jump;
            case ZYDIS_MNEMONIC_LEA:
                return InstructionKind::Lea;
            case ZYDIS_MNEMONIC_MOV:
                return InstructionKind::Mov;
            case ZYDIS_MNEMONIC_RET:
                return InstructionKind::Return;
            default:
                return InstructionKind::Other;
        }
    }

    std::string RegisterName(ZydisDecodedOperand const& operand)
    {
        if (operand.type != ZYDIS_OPERAND_TYPE_REGISTER)
            return {};
        char const* const name = ZydisRegisterGetString(operand.reg.value);
        if (name == nullptr)
            return {};
        std::string lowered(name);
        for (char& c : lowered)
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c - 'A' + 'a');
        return lowered;
    }

    DecodedInstruction Describe(ZydisDecodedInstruction const& instruction, ZydisDecodedOperand const* operands, uint64 address)
    {
        DecodedInstruction decoded;
        decoded.Address = address;
        decoded.Length = instruction.length;
        decoded.Kind = KindOf(instruction.mnemonic);
        for (std::size_t index = 0; index < instruction.operand_count; ++index)
        {
            ZydisDecodedOperand const& operand = operands[index];
            uint64 absolute = 0;
            if (!decoded.BranchTarget && operand.type == ZYDIS_OPERAND_TYPE_IMMEDIATE && operand.imm.is_relative && ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(&instruction, &operand, address, &absolute)))
                decoded.BranchTarget = absolute;
            if (!decoded.RipRelativeTarget && operand.type == ZYDIS_OPERAND_TYPE_MEMORY && operand.mem.base == ZYDIS_REGISTER_RIP && ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(&instruction, &operand, address, &absolute)))
                decoded.RipRelativeTarget = absolute;
        }
        if (instruction.operand_count_visible > 0)
        {
            decoded.FirstRegister = RegisterName(operands[0]);
            decoded.FirstOperandIsMemory = operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY;
        }
        if (instruction.operand_count_visible > 1)
            decoded.SecondRegister = RegisterName(operands[1]);
        return decoded;
    }
}

CodeIndex::CodeIndex(PeImage const& image) : _image(image)
{
    uint64 const base = image.GetImageBase();
    uint64 const imageEnd = base + image.GetSizeOfImage();
    std::vector<AddressRange> executable;
    for (PeSection const& section : image.GetSections())
        if (section.IsExecutable())
            executable.push_back({ base + section.VirtualAddress, base + section.VirtualAddress + std::max(section.VirtualSize, section.RawSize) });
    auto const isExecutable = [&executable](uint64 address)
    {
        return std::any_of(executable.begin(), executable.end(), [address](AddressRange const& range) { return address >= range.Begin && address < range.End; });
    };

    std::span<uint8 const> const bytes = image.GetBytes();
    for (PeSection const& section : image.GetSections())
    {
        if (!section.IsExecutable() || section.RawSize == 0)
            continue;
        uint8 const* const data = bytes.data() + section.RawOffset;
        std::size_t const size = section.RawSize;
        uint64 const start = base + section.VirtualAddress;
        for (std::size_t position = 0; position < size; ++position)
        {
            uint8 const byte = data[position];
            if (byte == 0x48 || byte == 0x4C)
            {
                if (position + LeaLength <= size && data[position + 1] == 0x8D && (data[position + 2] & 0xC7) == 0x05)
                    _leaReferences[Displaced(start + position + LeaLength, data + position + 3)].push_back(start + position);
            }
            else if (byte == 0xE8)
            {
                if (position + DirectCallLength > size)
                    continue;
                uint64 const target = Displaced(start + position + DirectCallLength, data + position + 1);
                if (isExecutable(target))
                    _callSites[target].push_back(start + position);
            }
            else if (byte == 0xFF)
            {
                if (position + IndirectBranchLength > size || (data[position + 1] != 0x15 && data[position + 1] != 0x25))
                    continue;
                uint64 const slot = Displaced(start + position + IndirectBranchLength, data + position + 2);
                if (slot >= base && slot < imageEnd)
                    _indirectBranchSites[slot].push_back(start + position);
            }
        }
    }
    SortSites(_leaReferences);
    SortSites(_callSites);
    SortSites(_indirectBranchSites);
}

std::span<uint64 const> CodeIndex::LeaReferences(uint64 target) const
{
    return SitesOf(_leaReferences, target);
}

std::span<uint64 const> CodeIndex::CallSites(uint64 target) const
{
    return SitesOf(_callSites, target);
}

std::span<uint64 const> CodeIndex::IndirectBranchSites(uint64 slot) const
{
    return SitesOf(_indirectBranchSites, slot);
}

std::optional<uint64> CodeIndex::FunctionStart(uint64 address) const
{
    uint64 const base = _image.GetImageBase();
    if (address < base || address - base > std::numeric_limits<uint32>::max())
        return std::nullopt;
    std::optional<uint32> const start = _image.PrimaryFunctionStart(static_cast<uint32>(address - base));
    if (!start)
        return std::nullopt;
    return base + *start;
}

std::vector<uint64> CodeIndex::FindStrings(std::string_view text) const
{
    std::vector<uint32> const rvas = _image.FindTerminatedString(text);
    std::vector<uint64> addresses;
    addresses.reserve(rvas.size());
    for (uint32 const rva : rvas)
        addresses.push_back(_image.GetImageBase() + rva);
    return addresses;
}

std::vector<DecodedInstruction> CodeIndex::Decode(uint64 address, std::size_t maxBytes, std::size_t maxInstructions) const
{
    std::vector<DecodedInstruction> instructions;
    uint64 const base = _image.GetImageBase();
    ZydisDecoder decoder;
    if (address < base || !ZYAN_SUCCESS(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64)))
        return instructions;
    std::size_t consumed = 0;
    while (consumed < maxBytes && instructions.size() < maxInstructions)
    {
        uint64 const current = address + consumed;
        if (current < address || current - base > std::numeric_limits<uint32>::max())
            break;
        uint32 const rva = static_cast<uint32>(current - base);
        std::span<uint8 const> bytes;
        for (std::size_t length = std::min(MaxInstructionLength, maxBytes - consumed); length > 0 && bytes.empty(); --length)
            bytes = _image.ReadRva(rva, static_cast<uint32>(length));
        if (bytes.empty())
            break;
        ZydisDecodedInstruction instruction;
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
        if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, bytes.data(), bytes.size(), &instruction, operands)))
            break;
        instructions.push_back(Describe(instruction, operands, current));
        consumed += instruction.length;
    }
    return instructions;
}

std::vector<DecodedInstruction> CodeIndex::DecodeFunctionUntil(uint64 address) const
{
    std::optional<uint64> const start = FunctionStart(address);
    if (!start || *start >= address)
        return {};
    uint64 const window = address - *start + MaxInstructionLength;
    std::size_t const maxBytes = static_cast<std::size_t>(std::min<uint64>(window, std::numeric_limits<std::size_t>::max()));
    std::vector<DecodedInstruction> instructions = Decode(*start, maxBytes, maxBytes);
    auto const after = std::find_if(instructions.begin(), instructions.end(), [address](DecodedInstruction const& instruction) { return instruction.Address >= address; });
    instructions.erase(after, instructions.end());
    return instructions;
}

uint64 CodeIndex::GetImageBase() const noexcept
{
    return _image.GetImageBase();
}

PeImage const& CodeIndex::GetImage() const noexcept
{
    return _image;
}
