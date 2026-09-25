/*
 * Project Ambrose by Imjustchico
 * Reads every NUL-terminated Class::MSG_Name and MSG_Name string in a program's data sections and follows each RIP-relative reference to a Class::MSG_Name, a lea or the first read of a copy made with mov or movups. A reference is a registration only when the code just before it, back to the previous reference to any such name, also reads the plain MSG_Name, either a string of its own or the tail of the debug name the linker merged it into, because the client's log calls name their own function with the same string and read no plain name; the handler is the last lea of an address in executable code between the name and the call that registers it, as when the handler is that call's last argument, or else the last one in the stretch before the name, as when it is stored first; a registration with neither is kept with no address, and repeats of one handler under one name are kept once. A reference inside the very function its name registers as a handler is not a registration either, since a handler that names itself in its log lines and builds its own plain name to post itself for later reads both names just as a registration does.
 */

#include "MessageHandlers.h"
#include "CodeIndex.h"
#include "PeImage.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <utility>

namespace
{
    constexpr std::size_t LongestName = 256;
    constexpr std::size_t RegistrationReach = 96;
    constexpr std::size_t CallReach = 32;
    constexpr std::size_t CallReachBytes = 256;

    struct DebugName
    {
        uint64 Address = 0;
        std::string_view Text;
        std::size_t Split = 0;
    };

    bool IsPlainName(std::string_view text)
    {
        return text.size() > 4 && text.starts_with("MSG_")
            && std::all_of(text.begin(), text.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_'; });
    }

    void ForEachString(PeImage const& image, std::function<void(uint64, std::string_view)> const& visit)
    {
        for (PeSection const& section : image.GetSections())
        {
            if (section.IsExecutable())
                continue;
            std::span<uint8 const> const bytes = image.ReadRva(section.VirtualAddress, std::min(section.VirtualSize, section.RawSize));
            std::size_t start = 0;
            for (std::size_t at = 0; at < bytes.size(); ++at)
            {
                if (bytes[at] != 0)
                    continue;
                if (at > start && at - start <= LongestName)
                    visit(image.GetImageBase() + section.VirtualAddress + start, std::string_view(reinterpret_cast<char const*>(bytes.data() + start), at - start));
                start = at + 1;
            }
        }
    }

    bool IsCode(PeImage const& image, uint64 address)
    {
        if (address < image.GetImageBase() || address - image.GetImageBase() > std::numeric_limits<uint32>::max())
            return false;
        PeSection const* const section = image.SectionOfRva(static_cast<uint32>(address - image.GetImageBase()));
        return section != nullptr && section->IsExecutable();
    }
}

std::optional<std::size_t> MessageHandlers::SplitName(std::string_view text)
{
    std::size_t const split = text.rfind("::MSG_");
    if (split == std::string_view::npos || split == 0 || split + 6 >= text.size())
        return std::nullopt;
    auto const inOwner = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == ':' || c == '<' || c == '>'; };
    auto const inName = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_'; };
    if (std::isalpha(static_cast<unsigned char>(text.front())) == 0 || !std::all_of(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(split), inOwner)
        || !std::all_of(text.begin() + static_cast<std::ptrdiff_t>(split) + 2, text.end(), inName))
        return std::nullopt;
    return split;
}

std::vector<MessageHandlerRegistration> MessageHandlers::Find(PeImage const& image, CodeIndex const& index)
{
    std::vector<DebugName> debugNames;
    std::map<std::string_view, std::vector<std::pair<uint64, uint64>>> plainNames;
    ForEachString(image, [&debugNames, &plainNames](uint64 address, std::string_view text)
    {
        if (std::optional<std::size_t> const split = SplitName(text))
        {
            debugNames.push_back({ address, text, *split });
            std::string_view const tail = text.substr(*split + 2);
            plainNames[tail].emplace_back(address + *split + 2, address + text.size());
        }
        else if (IsPlainName(text))
            plainNames[text].emplace_back(address, address + text.size());
    });
    std::set<uint64> debugAddresses;
    for (DebugName const& name : debugNames)
        debugAddresses.insert(name.Address);

    std::vector<MessageHandlerRegistration> registrations;
    for (DebugName const& name : debugNames)
    {
        std::string_view const handler = name.Text.substr(name.Split + 2);
        std::vector<std::pair<uint64, uint64>> const& plain = plainNames[handler];
        auto const readsPlainName = [&plain](uint64 target)
        {
            return std::any_of(plain.begin(), plain.end(), [target](std::pair<uint64, uint64> const& range) { return target >= range.first && target < range.second; });
        };
        for (uint64 const site : index.RipReferences(name.Address))
        {
            std::vector<DecodedInstruction> const code = index.DecodeFunctionUntil(site);
            bool plainRead = false;
            uint64 handlerAddress = 0;
            std::size_t walked = 0;
            for (auto instruction = code.rbegin(); instruction != code.rend() && walked < RegistrationReach; ++instruction)
            {
                if (instruction->Address >= site)
                    continue;
                ++walked;
                if (!instruction->RipRelativeTarget)
                    continue;
                uint64 const target = *instruction->RipRelativeTarget;
                if (debugAddresses.contains(target))
                    break;
                if (readsPlainName(target))
                    plainRead = true;
                else if (handlerAddress == 0 && instruction->Kind == InstructionKind::Lea && IsCode(image, target))
                    handlerAddress = target;
            }
            if (!plainRead)
                continue;
            std::vector<DecodedInstruction> const after = index.Decode(site, CallReachBytes, CallReach);
            for (std::size_t at = 1; at < after.size(); ++at)
            {
                DecodedInstruction const& instruction = after[at];
                if (instruction.Kind == InstructionKind::Call || instruction.Kind == InstructionKind::Return
                    || (instruction.RipRelativeTarget && debugAddresses.contains(*instruction.RipRelativeTarget)))
                    break;
                if (instruction.Kind == InstructionKind::Lea && instruction.RipRelativeTarget && IsCode(image, *instruction.RipRelativeTarget))
                    handlerAddress = *instruction.RipRelativeTarget;
            }
            registrations.push_back({ std::string(name.Text.substr(0, name.Split)), std::string(handler), site, handlerAddress });
        }
    }
    std::set<std::pair<std::string, uint64>> handlers;
    for (MessageHandlerRegistration const& registration : registrations)
        if (registration.Address != 0)
            handlers.emplace(registration.Handler, registration.Address);
    std::erase_if(registrations, [&index, &handlers](MessageHandlerRegistration const& registration)
    {
        std::optional<uint64> const start = index.FunctionStart(registration.Site);
        return start && handlers.contains({ registration.Handler, *start });
    });
    std::sort(registrations.begin(), registrations.end(), [](MessageHandlerRegistration const& left, MessageHandlerRegistration const& right)
    {
        return std::tie(left.Handler, left.Owner, left.Address, left.Site) < std::tie(right.Handler, right.Owner, right.Address, right.Site);
    });
    registrations.erase(std::unique(registrations.begin(), registrations.end(), [](MessageHandlerRegistration const& left, MessageHandlerRegistration const& right)
    {
        return left.Handler == right.Handler && left.Owner == right.Owner && left.Address == right.Address;
    }), registrations.end());
    return registrations;
}
