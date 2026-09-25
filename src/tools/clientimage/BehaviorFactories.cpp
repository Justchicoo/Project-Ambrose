/*
 * Project Ambrose by Imjustchico
 * Takes every name ending in Behavior that the program's code reads from its data sections, as the NUL-terminated text at the exact address an instruction loads, and follows each read forward, through the whole of each function the exception table gives even where it splits one into regions, to the factory the program stores for it, in place after the name is turned into its id or inside the next function called, a vtable whose first two slots are code; the factory's create function is decoded for the vtable it writes at offset 0 of the object it allocates, tracking that pointer through register copies until a call may clobber it, and into the constructor it calls when it writes none itself, the last such vtable being the class made; that vtable's first slot, GetType, followed through jump thunks, registers the class under the first string of class name form it loads, handing the registration the type its parent's GetType returned, so each parent's GetType is read the same way up to a type registered with no parent, with registers followed by the 64-bit register each write is part of. A read that stores no factory registers nothing, nor does one whose factory's create function gives no object a vtable, since a factory is what makes an object; a class name that cannot be followed is left empty rather than guessed.
 */

#include "BehaviorFactories.h"
#include "CodeIndex.h"
#include "PeImage.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <utility>

namespace
{
    constexpr uint32 LongestString = 200;
    constexpr std::size_t RegistrationReach = 64;
    constexpr std::size_t CalleeReach = 256;
    constexpr std::size_t CreateReach = 128;
    constexpr std::size_t GetTypeReach = 96;
    constexpr std::size_t StoreGap = 3;
    constexpr std::size_t ThunkHops = 3;
    constexpr std::size_t DeepestBase = 32;
    constexpr uint8 JumpLength = 5;

    using Registers = std::map<std::string, uint64>;

    struct VtableStore
    {
        uint64 Vtable = 0;
        std::string Base;
    };

    struct TypeRegistration
    {
        std::string_view Name;
        std::optional<uint64> Parent;
    };

    bool IsVolatile(std::string const& reg)
    {
        return reg == "rax" || reg == "rcx" || reg == "rdx" || reg == "r8" || reg == "r9" || reg == "r10" || reg == "r11";
    }

    void ForgetVolatile(Registers& registers)
    {
        std::erase_if(registers, [](Registers::value_type const& entry) { return IsVolatile(entry.first); });
    }

    void Carry(DecodedInstruction const& instruction, Registers& registers)
    {
        if (!instruction.WritesFirstOperand || instruction.FirstRegisterFamily.empty())
            return;
        auto const source = registers.find(instruction.SecondRegister);
        if (instruction.Kind == InstructionKind::Mov && instruction.FirstRegister == instruction.FirstRegisterFamily && instruction.SecondRegister == instruction.SecondRegisterFamily
            && source != registers.end())
        {
            uint64 const value = source->second;
            registers[instruction.FirstRegister] = value;
            return;
        }
        registers.erase(instruction.FirstRegisterFamily);
    }

    std::optional<uint32> RvaOf(PeImage const& image, uint64 address)
    {
        if (address < image.GetImageBase() || address - image.GetImageBase() > std::numeric_limits<uint32>::max())
            return std::nullopt;
        return static_cast<uint32>(address - image.GetImageBase());
    }

    PeSection const* SectionOf(PeImage const& image, uint64 address)
    {
        std::optional<uint32> const rva = RvaOf(image, address);
        return rva ? image.SectionOfRva(*rva) : nullptr;
    }

    bool IsCode(PeImage const& image, uint64 address)
    {
        PeSection const* const section = SectionOf(image, address);
        return section != nullptr && section->IsExecutable();
    }

    bool IsData(PeImage const& image, uint64 address)
    {
        PeSection const* const section = SectionOf(image, address);
        return section != nullptr && !section->IsExecutable();
    }

    std::optional<uint64> PointerAt(PeImage const& image, uint64 address)
    {
        std::optional<uint32> const rva = RvaOf(image, address);
        if (!rva)
            return std::nullopt;
        std::span<uint8 const> const bytes = image.ReadRva(*rva, 8);
        if (bytes.size() < 8)
            return std::nullopt;
        uint64 value = 0;
        for (std::size_t at = 0; at < 8; ++at)
            value |= uint64{ bytes[at] } << (8 * at);
        return value;
    }

    bool IsVtable(PeImage const& image, uint64 vtable, std::size_t slots)
    {
        for (std::size_t slot = 0; slot < slots; ++slot)
        {
            std::optional<uint64> const entry = PointerAt(image, vtable + 8 * slot);
            if (!entry || !IsCode(image, *entry))
                return false;
        }
        return true;
    }

    bool IsClassName(std::string_view text)
    {
        if (text.size() < 3 || (std::isalpha(static_cast<unsigned char>(text.front())) == 0 && text.front() != '_'))
            return false;
        return std::all_of(text.begin(), text.end(), [](char c)
        {
            return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == ':' || c == '<' || c == '>' || c == ',' || c == ' ' || c == '*';
        });
    }

    std::optional<std::string_view> StringAt(PeImage const& image, uint64 address)
    {
        std::optional<uint32> const rva = RvaOf(image, address);
        PeSection const* const section = rva ? image.SectionOfRva(*rva) : nullptr;
        if (section == nullptr || section->IsExecutable())
            return std::nullopt;
        uint32 const backedEnd = section->VirtualAddress + std::min(section->VirtualSize, section->RawSize);
        if (*rva >= backedEnd)
            return std::nullopt;
        std::span<uint8 const> const bytes = image.ReadRva(*rva, std::min<uint32>(LongestString + 1, backedEnd - *rva));
        auto const end = std::find(bytes.begin(), bytes.end(), uint8{ 0 });
        if (end == bytes.begin() || end == bytes.end() || !std::all_of(bytes.begin(), end, [](uint8 byte) { return byte >= 0x20 && byte <= 0x7E; }))
            return std::nullopt;
        return std::string_view(reinterpret_cast<char const*>(bytes.data()), static_cast<std::size_t>(end - bytes.begin()));
    }

    std::optional<VtableStore> VtableStoreAt(std::vector<DecodedInstruction> const& code, std::size_t at, PeImage const& image)
    {
        DecodedInstruction const& load = code[at];
        if (load.Kind != InstructionKind::Lea || !load.RipRelativeTarget || load.FirstRegister.empty() || !IsData(image, *load.RipRelativeTarget))
            return std::nullopt;
        for (std::size_t next = at + 1; next < code.size() && next <= at + StoreGap; ++next)
        {
            DecodedInstruction const& store = code[next];
            if (store.Kind == InstructionKind::Mov && store.FirstOperandIsMemory && store.MemoryDisplacement == 0 && !store.MemoryBase.empty()
                && store.SecondRegister == load.FirstRegister)
                return VtableStore{ *load.RipRelativeTarget, store.MemoryBase };
            if (store.WritesFirstOperand && store.FirstRegisterFamily == load.FirstRegisterFamily)
                break;
        }
        return std::nullopt;
    }

    std::optional<uint64> FactoryIn(std::vector<DecodedInstruction> const& code, PeImage const& image)
    {
        for (std::size_t at = 0; at < code.size(); ++at)
            if (std::optional<VtableStore> const store = VtableStoreAt(code, at, image); store && IsVtable(image, store->Vtable, 2))
                return store->Vtable;
        return std::nullopt;
    }

    std::optional<uint64> FactoryAfter(PeImage const& image, CodeIndex const& index, uint64 site, std::set<uint64> const& names)
    {
        std::vector<DecodedInstruction> const after = index.DecodeFunctionFrom(site, RegistrationReach);
        std::size_t calls = 0;
        for (std::size_t at = 1; at < after.size(); ++at)
        {
            DecodedInstruction const& instruction = after[at];
            if (instruction.Kind == InstructionKind::Return || (instruction.RipRelativeTarget && names.contains(*instruction.RipRelativeTarget)))
                return std::nullopt;
            if (std::optional<VtableStore> const store = VtableStoreAt(after, at, image); store && IsVtable(image, store->Vtable, 2))
                return store->Vtable;
            if (instruction.Kind != InstructionKind::Call || !instruction.BranchTarget || !IsCode(image, *instruction.BranchTarget) || ++calls < 2)
                continue;
            if (std::optional<uint64> const found = FactoryIn(index.DecodeFunctionFrom(*instruction.BranchTarget, CalleeReach), image))
                return found;
        }
        return std::nullopt;
    }

    std::optional<uint64> ObjectVtableIn(std::vector<DecodedInstruction> const& code, PeImage const& image, Registers object, bool allocates, std::vector<uint64>* constructors)
    {
        std::optional<uint64> found;
        bool allocated = !allocates;
        for (std::size_t at = 0; at < code.size(); ++at)
        {
            DecodedInstruction const& instruction = code[at];
            if (instruction.Kind == InstructionKind::Return)
                break;
            if (instruction.Kind == InstructionKind::Call)
            {
                if (!allocated)
                {
                    allocated = true;
                    object = { { "rax", 0 } };
                    continue;
                }
                if (constructors != nullptr && object.contains("rcx") && instruction.BranchTarget && IsCode(image, *instruction.BranchTarget))
                    constructors->push_back(*instruction.BranchTarget);
                ForgetVolatile(object);
                continue;
            }
            if (!allocated)
                continue;
            if (std::optional<VtableStore> const store = VtableStoreAt(code, at, image); store && object.contains(store->Base) && IsVtable(image, store->Vtable, 1))
                found = store->Vtable;
            Carry(instruction, object);
        }
        return found;
    }

    std::optional<uint64> ObjectVtable(PeImage const& image, CodeIndex const& index, uint64 create)
    {
        std::vector<uint64> constructors;
        std::vector<DecodedInstruction> const code = index.DecodeFunctionFrom(create, CreateReach);
        if (std::optional<uint64> const found = ObjectVtableIn(code, image, {}, true, &constructors))
            return found;
        for (auto constructor = constructors.rbegin(); constructor != constructors.rend(); ++constructor)
            if (std::optional<uint64> const found = ObjectVtableIn(index.DecodeFunctionFrom(*constructor, CreateReach), image, { { "rcx", 0 } }, false, nullptr))
                return found;
        return std::nullopt;
    }

    uint64 FollowThunks(PeImage const& image, CodeIndex const& index, uint64 target)
    {
        for (std::size_t hop = 0; hop < ThunkHops; ++hop)
        {
            std::vector<DecodedInstruction> const first = index.Decode(target, 16, 1);
            if (first.size() != 1 || first[0].Kind != InstructionKind::Jump || first[0].Length != JumpLength || !first[0].BranchTarget || !IsCode(image, *first[0].BranchTarget))
                break;
            target = *first[0].BranchTarget;
        }
        return target;
    }

    std::optional<TypeRegistration> RegistrationIn(PeImage const& image, CodeIndex const& index, uint64 getType)
    {
        std::optional<std::string_view> name;
        Registers results;
        for (DecodedInstruction const& instruction : index.DecodeFunctionFrom(getType, GetTypeReach))
        {
            if (instruction.Kind == InstructionKind::Call)
            {
                if (name)
                {
                    auto const parent = results.find("rdx");
                    return TypeRegistration{ *name, parent == results.end() ? std::nullopt : std::optional<uint64>(parent->second) };
                }
                ForgetVolatile(results);
                if (instruction.BranchTarget && IsCode(image, *instruction.BranchTarget))
                    results["rax"] = FollowThunks(image, index, *instruction.BranchTarget);
                continue;
            }
            if (!name && instruction.Kind == InstructionKind::Lea && instruction.RipRelativeTarget)
                if (std::optional<std::string_view> const text = StringAt(image, *instruction.RipRelativeTarget); text && IsClassName(*text))
                    name = text;
            Carry(instruction, results);
        }
        if (!name)
            return std::nullopt;
        return TypeRegistration{ *name, std::nullopt };
    }

    void Describe(PeImage const& image, CodeIndex const& index, BehaviorFactory& registration)
    {
        std::optional<uint64> const entry = PointerAt(image, registration.ObjectVtable);
        if (!entry || !IsCode(image, *entry))
            return;
        registration.GetType = FollowThunks(image, index, *entry);
        std::optional<TypeRegistration> type = RegistrationIn(image, index, registration.GetType);
        if (!type)
            return;
        registration.ClassName = std::string(type->Name);
        std::set<uint64> seen = { registration.GetType };
        while (type->Parent && registration.Bases.size() < DeepestBase && seen.insert(*type->Parent).second)
        {
            type = RegistrationIn(image, index, *type->Parent);
            if (!type)
                return;
            registration.Bases.emplace_back(type->Name);
        }
    }
}

bool BehaviorFactories::IsBehaviorName(std::string_view text)
{
    return text.size() > 8 && text.size() <= 64 && text.ends_with("Behavior") && std::isupper(static_cast<unsigned char>(text.front())) != 0
        && std::all_of(text.begin(), text.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0; });
}

std::vector<BehaviorFactory> BehaviorFactories::Find(PeImage const& image, CodeIndex const& index)
{
    std::vector<std::pair<std::string_view, uint64>> behaviors;
    std::set<uint64> names;
    for (uint64 const target : index.RipTargets())
    {
        std::optional<std::string_view> const text = StringAt(image, target);
        if (!text || !IsBehaviorName(*text))
            continue;
        behaviors.emplace_back(*text, target);
        names.insert(target);
    }
    std::sort(behaviors.begin(), behaviors.end());

    std::vector<BehaviorFactory> found;
    for (auto const& [text, address] : behaviors)
    {
        for (uint64 const site : index.RipReferences(address))
        {
            std::optional<uint64> const factory = FactoryAfter(image, index, site, names);
            if (!factory)
                continue;
            std::optional<uint64> const create = PointerAt(image, *factory + 8);
            std::optional<uint64> const object = create ? ObjectVtable(image, index, *create) : std::nullopt;
            if (!object)
                continue;
            BehaviorFactory registration;
            registration.Behavior = std::string(text);
            registration.Site = site;
            registration.FactoryVtable = *factory;
            registration.Create = *create;
            registration.ObjectVtable = *object;
            Describe(image, index, registration);
            found.push_back(std::move(registration));
        }
    }
    found.erase(std::unique(found.begin(), found.end(), [](BehaviorFactory const& left, BehaviorFactory const& right)
    {
        return left.Behavior == right.Behavior && left.FactoryVtable == right.FactoryVtable;
    }), found.end());
    return found;
}
