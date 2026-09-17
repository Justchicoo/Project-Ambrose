/*
 * Project Ambrose by Imjustchico
 * Loads the guest's modules into the machine with relocations, TLS blocks and resolved imports, refusing a runtime DLL or export the folder lacks, dispatches stub calls to handlers by name while counting them and naming the API a handler failed in, and runs module startup code.
 */

#include "GuestProcess.h"
#include "ClientLocator.h"
#include "ConfigMgr.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <exception>
#include <string>
#include <string_view>
#include <system_error>

namespace
{
    constexpr uint64 TebSize = 0x10000;
    constexpr uint64 PebSize = 0x10000;
    constexpr uint64 DllGap = 0x100000;
    constexpr uint64 TlsSlack = 0x100;
    constexpr uint64 CallbackBudget = 50000000;
    constexpr uint64 EntryBudget = 200000000;
    constexpr uint32 MaxForwarderDepth = 16;
    constexpr uint32 MaxTlsCallbacks = 64;
    constexpr uint64 DllProcessAttach = 1;

    uint64 AlignUp(uint64 value, uint64 alignment)
    {
        return (value + alignment - 1) / alignment * alignment;
    }

    std::string Lower(std::string_view text)
    {
        std::string out(text);
        std::transform(out.begin(), out.end(), out.begin(), [](char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c); });
        return out;
    }

    std::string ModuleKey(std::string_view name)
    {
        std::string key = Lower(name);
        std::size_t const slash = key.find_last_of("/\\");
        if (slash != std::string::npos)
            key.erase(0, slash + 1);
        if (key.find('.') == std::string::npos)
            key += ".dll";
        return key;
    }

    bool IsKernelSet(std::string_view key)
    {
        return key.starts_with("api-ms-win-core-") || key == "kernel32.dll" || key == "kernelbase.dll" || key == "ntdll.dll";
    }

    std::string ImportLabel(std::string_view name, std::optional<uint16> ordinal)
    {
        return name.empty() && ordinal ? fmt::format("#{}", *ordinal) : std::string(name);
    }

    std::string NeededBy(std::string_view requester)
    {
        return requester.empty() ? std::string{} : fmt::format(", which {} needs", requester);
    }
}

struct GuestProcess::State
{
    explicit State(Options opts) : options(std::move(opts))
    {
    }

    Options options;
    Machine machine;
    std::unique_ptr<GuestHeap> heap;
    std::vector<std::unique_ptr<GuestModule>> modules;
    std::map<std::string, GuestModule*, std::less<>> byName;
    GuestModule* main = nullptr;
    uint64 nextDllBase = FirstDllBase;
    uint32 nextTlsIndex = 0;
    std::map<std::string, ApiHandler, std::less<>> handlers;
    std::vector<std::pair<std::string, std::string>> stubs;
    std::map<std::string, uint64, std::less<>> stubByKey;
    std::map<std::string, uint64> callCounts;
    std::map<std::string, uint64> unhandled;
    std::string currentApi;

    std::filesystem::path FindFile(std::string const& key) const
    {
        std::error_code ec;
        for (std::filesystem::directory_iterator it(options.Folder, ec), end; !ec && it != end; it.increment(ec))
        {
            std::string name;
            try
            {
                name = ConfigMgr::PathToUtf8(it->path().filename());
            }
            catch (std::exception const&)
            {
                continue;
            }
            if (Lower(name) == key)
                return it->path();
        }
        return {};
    }

    GuestModule& Register(std::string const& key, std::filesystem::path const& path, bool isMain)
    {
        std::string error;
        std::unique_ptr<PeImage> image = PeImage::Load(path, error);
        if (!image)
            throw EmulationError(fmt::format("{} could not be read: {}", ClientLocator::PathText(path), error));
        if (image->GetMachine() != PeImage::MachineAmd64)
            throw EmulationError(fmt::format("{} is not an x86-64 image", ClientLocator::PathText(path)));
        auto module = std::make_unique<GuestModule>();
        module->Name = key;
        module->Path = path;
        module->Size = AlignUp(image->GetSizeOfImage(), Machine::PageSize);
        if (isMain)
            module->Base = image->GetImageBase();
        else
        {
            module->Base = nextDllBase;
            nextDllBase += AlignUp(module->Size, DllGap) + DllGap;
        }
        module->Image = std::move(image);
        GuestModule* const raw = module.get();
        modules.push_back(std::move(module));
        byName[key] = raw;
        if (isMain)
            main = raw;
        return *raw;
    }

    void Place(GuestProcess& process, GuestModule& target)
    {
        PeImage const& image = *target.Image;
        machine.Map(target.Base, target.Size);
        std::span<uint8 const> const bytes = image.GetBytes();
        std::size_t const headers = std::min<std::size_t>({ image.GetSizeOfHeaders(), bytes.size(), static_cast<std::size_t>(target.Size) });
        machine.Write(target.Base, bytes.subspan(0, headers));
        for (PeSection const& section : image.GetSections())
        {
            if (!section.RawSize || section.RawOffset >= bytes.size() || section.VirtualAddress >= target.Size)
                continue;
            uint64 length = section.RawSize;
            if (section.VirtualSize)
                length = std::min<uint64>(length, section.VirtualSize);
            length = std::min<uint64>({ length, bytes.size() - section.RawOffset, target.Size - section.VirtualAddress });
            machine.Write(target.Base + section.VirtualAddress, bytes.subspan(section.RawOffset, static_cast<std::size_t>(length)));
        }
        uint64 const delta = target.Base - image.GetImageBase();
        if (delta)
        {
            for (PeRelocation const& relocation : image.GetRelocations())
            {
                uint64 const at = target.Base + relocation.Rva;
                if (relocation.Type == PeImage::RelocationDir64)
                    machine.WriteU64(at, machine.ReadU64(at) + delta);
                else if (relocation.Type == PeImage::RelocationHighLow)
                    machine.WriteU32(at, static_cast<uint32>(machine.ReadU32(at) + static_cast<uint32>(delta)));
                else
                    throw EmulationError(fmt::format("{} has a relocation of unsupported type {} at {:#x}", target.Name, relocation.Type, relocation.Rva));
            }
        }
        if (image.GetTls())
        {
            PeTls const& tls = *image.GetTls();
            if (nextTlsIndex >= MaxTlsModules)
                throw EmulationError("too many modules with thread-local storage");
            uint64 const start = tls.StartAddressOfRawData + delta;
            uint64 const end = tls.EndAddressOfRawData + delta;
            uint64 const templateSize = end >= start ? end - start : 0;
            uint64 const block = heap->Allocate(templateSize + tls.SizeOfZeroFill + TlsSlack, true);
            if (templateSize)
                machine.Write(block, machine.ReadBytes(start, static_cast<std::size_t>(templateSize)));
            uint32 const index = nextTlsIndex++;
            target.TlsIndex = index;
            if (tls.AddressOfIndex)
                machine.WriteU32(tls.AddressOfIndex + delta, index);
            machine.WriteU64(TlsArrayAddress + 8 * uint64{ index }, block);
            if (tls.AddressOfCallBacks)
            {
                uint64 array = tls.AddressOfCallBacks + delta;
                for (uint32 i = 0; i < MaxTlsCallbacks; ++i, array += 8)
                {
                    uint64 const callback = machine.ReadU64(array);
                    if (!callback)
                        break;
                    target.TlsCallbacks.push_back(callback);
                }
            }
        }
        for (PeImport const& import : image.GetImports())
            machine.WriteU64(target.Base + import.SlotRva, Resolve(process, import.Dll, import.Name, import.Ordinal, 0, target.Name));
    }

    uint64 Stub(std::string const& dll, std::string_view name, std::optional<uint16> ordinal)
    {
        std::string const label = ImportLabel(name, ordinal);
        std::string const key = dll + "!" + label;
        auto const found = stubByKey.find(key);
        if (found != stubByKey.end())
            return found->second;
        if (stubs.size() >= StubCapacity)
            throw EmulationError("the guest process ran out of import stubs");
        uint64 const address = StubBase + stubs.size() * StubSize;
        std::array<uint8, 1> const ret{ 0xC3 };
        machine.Write(address, ret);
        stubs.emplace_back(dll, label);
        stubByKey.emplace(key, address);
        return address;
    }

    GuestModule* LoadReal(GuestProcess& process, std::string const& key)
    {
        auto const existing = byName.find(key);
        if (existing != byName.end())
            return existing->second;
        std::filesystem::path const path = FindFile(key);
        if (path.empty())
            return nullptr;
        GuestModule& module = Register(key, path, false);
        Place(process, module);
        return &module;
    }

    uint64 Resolve(GuestProcess& process, std::string_view dll, std::string_view name, std::optional<uint16> ordinal, uint32 depth, std::string_view requester)
    {
        if (depth > MaxForwarderDepth)
            throw EmulationError(fmt::format("the export forwarding chain for {}!{} is too long", dll, ImportLabel(name, ordinal)));
        std::string const key = ModuleKey(dll);
        if (IsKernelSet(key))
            return Stub("kernel32.dll", name, ordinal);
        bool const crtSet = key.starts_with("api-ms-win-crt-");
        bool const real = crtSet || std::any_of(options.RealModules.begin(), options.RealModules.end(), [&](std::string const& m) { return ModuleKey(m) == key; });
        if (!real)
            return Stub(key, name, ordinal);
        bool const universalFallback = crtSet && !name.empty();
        GuestModule* const module = LoadReal(process, key);
        if (!module)
        {
            if (universalFallback)
                return Resolve(process, "ucrtbase.dll", name, ordinal, depth + 1, requester);
            throw EmulationError(fmt::format("{} is missing from {}{}", key, ClientLocator::PathText(options.Folder), NeededBy(requester)));
        }
        if (std::optional<uint64> const address = InModule(process, *module, name, ordinal, depth))
            return *address;
        if (universalFallback)
            return Resolve(process, "ucrtbase.dll", name, ordinal, depth + 1, requester);
        throw EmulationError(fmt::format("{} does not export {}{}", key, ImportLabel(name, ordinal), NeededBy(requester)));
    }

    std::optional<uint64> InModule(GuestProcess& process, GuestModule const& module, std::string_view name, std::optional<uint16> ordinal, uint32 depth)
    {
        std::string const& key = module.Name;
        PeExport const* const entry = name.empty() && ordinal ? module.Image->FindExportByOrdinal(*ordinal) : module.Image->FindExport(name);
        if (!entry)
            return std::nullopt;
        if (entry->Forwarder.empty())
            return module.Base + entry->Rva;
        std::size_t const dot = entry->Forwarder.find('.');
        if (dot == std::string::npos)
            throw EmulationError(fmt::format("{} forwards {} to the malformed target {}", key, name, entry->Forwarder));
        std::string const targetDll = entry->Forwarder.substr(0, dot) + ".dll";
        std::string_view const targetName = std::string_view(entry->Forwarder).substr(dot + 1);
        std::string const requester = fmt::format("{}!{}", key, ImportLabel(name, ordinal));
        if (targetName.starts_with('#'))
        {
            std::optional<uint16> const number = Ambrose::StringTo<uint16>(targetName.substr(1));
            if (!number)
                throw EmulationError(fmt::format("{} forwards {} to the malformed ordinal {}", key, name, entry->Forwarder));
            return Resolve(process, targetDll, {}, number, depth + 1, requester);
        }
        return Resolve(process, targetDll, targetName, std::nullopt, depth + 1, requester);
    }
};

GuestProcess::GuestProcess(Options options) : _state(std::make_unique<State>(std::move(options)))
{
    Machine& machine = _state->machine;
    _state->heap = std::make_unique<GuestHeap>(machine, HeapBase, _state->options.HeapSize);
    machine.Map(TebAddress, TebSize);
    machine.Map(PebAddress, PebSize);
    machine.WriteU64(TebAddress + 0x30, TebAddress);
    machine.WriteU64(TebAddress + 0x40, 0x1234);
    machine.WriteU64(TebAddress + 0x48, 0x1238);
    machine.WriteU64(TebAddress + 0x58, TlsArrayAddress);
    machine.WriteU64(TebAddress + 0x60, PebAddress);
    machine.SetRegister(GuestRegister::GsBase, TebAddress);
    machine.Map(StubBase, AlignUp(StubCapacity * StubSize, Machine::PageSize));
    machine.SetAddressDescriber([this](uint64 address) { return DescribeAddress(address); });
    machine.HookCode(StubBase, StubBase + StubCapacity * StubSize, [this](uint64 address)
    {
        uint64 const offset = address - StubBase;
        if (offset % StubSize != 0)
            return;
        uint64 const index = offset / StubSize;
        if (index >= _state->stubs.size())
            return;
        auto const& [dll, name] = _state->stubs[index];
        ++_state->callCounts[name];
        _state->currentApi = name;
        Machine& m = _state->machine;
        auto const handler = _state->handlers.find(name);
        if (handler == _state->handlers.end())
        {
            ++_state->unhandled[dll + "!" + name];
            m.SetRegister(GuestRegister::Rax, 0);
            return;
        }
        uint64 result = 0;
        try
        {
            result = handler->second(*this);
        }
        catch (EmulationError const&)
        {
            throw;
        }
        catch (std::exception const& failure)
        {
            throw EmulationError(fmt::format("the handler for {}!{} failed: {}", _state->stubs[index].first, _state->stubs[index].second, failure.what()));
        }
        catch (...)
        {
            throw EmulationError(fmt::format("the handler for {}!{} failed with an exception that carries no message", _state->stubs[index].first, _state->stubs[index].second));
        }
        m.SetRegister(GuestRegister::Rax, result);
    });
}

GuestProcess::~GuestProcess() = default;

GuestModule& GuestProcess::LoadMain(std::string_view fileName)
{
    if (_state->main)
        throw EmulationError("the main module is already loaded");
    std::string const key = ModuleKey(fileName);
    std::filesystem::path const path = _state->FindFile(key);
    if (path.empty())
        throw EmulationError(fmt::format("{} was not found in {}", fileName, ClientLocator::PathText(_state->options.Folder)));
    GuestModule& module = _state->Register(key, path, true);
    _state->Place(*this, module);
    return module;
}

GuestModule& GuestProcess::GetMain()
{
    if (!_state->main)
        throw EmulationError("no main module is loaded");
    return *_state->main;
}

GuestModule* GuestProcess::FindModule(std::string_view name)
{
    auto const found = _state->byName.find(ModuleKey(name));
    return found == _state->byName.end() ? nullptr : found->second;
}

GuestModule const* GuestProcess::ModuleAt(uint64 address) const
{
    for (auto const& module : _state->modules)
        if (address >= module->Base && address - module->Base < module->Size)
            return module.get();
    return nullptr;
}

std::vector<GuestModule const*> GuestProcess::GetModules() const
{
    std::vector<GuestModule const*> out;
    out.reserve(_state->modules.size());
    for (auto const& module : _state->modules)
        out.push_back(module.get());
    return out;
}

void GuestProcess::AttachRuntime()
{
    for (std::string const& name : _state->options.RealModules)
        if (GuestModule* const module = FindModule(name))
            Attach(*module);
    if (_state->main)
        Attach(*_state->main);
}

void GuestProcess::Attach(GuestModule& module)
{
    if (module.Attached)
        return;
    module.Attached = true;
    std::array<uint64, 3> const arguments{ module.Base, DllProcessAttach, 0 };
    for (uint64 const callback : module.TlsCallbacks)
        Call(callback, arguments, CallbackBudget);
    if (&module == _state->main || !module.Image->IsDll() || !module.Image->GetEntryPointRva())
        return;
    uint64 const result = Call(module.Base + module.Image->GetEntryPointRva(), arguments, EntryBudget);
    if ((result & 0xFFFFFFFFu) == 0)
        throw EmulationError(fmt::format("{} refused to start: its entry point returned FALSE", module.Name));
}

void GuestProcess::RegisterApi(std::string_view name, ApiHandler handler)
{
    _state->handlers[std::string(name)] = std::move(handler);
}

bool GuestProcess::HasApi(std::string_view name) const
{
    return _state->handlers.find(name) != _state->handlers.end();
}

uint64 GuestProcess::ResolveImport(std::string_view dll, std::string_view name, std::optional<uint16> ordinal)
{
    return _state->Resolve(*this, dll, name, ordinal, 0, {});
}

std::optional<uint64> GuestProcess::ResolveExport(GuestModule const& module, std::string_view name)
{
    return _state->InModule(*this, module, name, std::nullopt, 0);
}

std::optional<uint64> GuestProcess::ResolveExportByOrdinal(GuestModule const& module, uint16 ordinal)
{
    return _state->InModule(*this, module, {}, ordinal, 0);
}

uint64 GuestProcess::Call(uint64 function, std::initializer_list<uint64> arguments, uint64 instructionBudget)
{
    return _state->machine.Call(function, std::span<uint64 const>(arguments.begin(), arguments.size()), instructionBudget);
}

uint64 GuestProcess::Call(uint64 function, std::span<uint64 const> arguments, uint64 instructionBudget)
{
    return _state->machine.Call(function, arguments, instructionBudget);
}

uint64 GuestProcess::StoreBytes(std::span<uint8 const> bytes)
{
    uint64 const address = _state->heap->Allocate(bytes.size(), true);
    if (!bytes.empty())
        _state->machine.Write(address, bytes);
    return address;
}

uint64 GuestProcess::StoreCString(std::string_view text)
{
    std::vector<uint8> bytes(text.begin(), text.end());
    bytes.push_back(0);
    return StoreBytes(bytes);
}

uint64 GuestProcess::StoreWideString(std::u16string_view text)
{
    std::vector<uint8> bytes;
    bytes.reserve((text.size() + 1) * 2);
    for (char16_t const c : text)
    {
        bytes.push_back(static_cast<uint8>(c));
        bytes.push_back(static_cast<uint8>(c >> 8));
    }
    bytes.push_back(0);
    bytes.push_back(0);
    return StoreBytes(bytes);
}

Machine& GuestProcess::GetMachine() noexcept
{
    return _state->machine;
}

Machine const& GuestProcess::GetMachine() const noexcept
{
    return _state->machine;
}

GuestHeap& GuestProcess::GetHeap() noexcept
{
    return *_state->heap;
}

GuestHeap const& GuestProcess::GetHeap() const noexcept
{
    return *_state->heap;
}

std::filesystem::path const& GuestProcess::GetFolder() const noexcept
{
    return _state->options.Folder;
}

std::string_view GuestProcess::GetCurrentApiName() const noexcept
{
    return _state->currentApi;
}

std::map<std::string, uint64> const& GuestProcess::GetApiCallCounts() const noexcept
{
    return _state->callCounts;
}

std::map<std::string, uint64> const& GuestProcess::GetUnhandledApiCalls() const noexcept
{
    return _state->unhandled;
}

std::string GuestProcess::DescribeAddress(uint64 address) const
{
    if (GuestModule const* const module = ModuleAt(address))
        return fmt::format("{}+{:#x}", module->Name, address - module->Base);
    if (address >= StubBase && address < StubBase + StubCapacity * StubSize)
    {
        uint64 const index = (address - StubBase) / StubSize;
        if (index < _state->stubs.size())
            return fmt::format("{}!{}", _state->stubs[index].first, _state->stubs[index].second);
    }
    if (_state->heap && address >= _state->heap->GetBase() && address < _state->heap->GetLimit())
        return fmt::format("heap {:#x}", address);
    return fmt::format("{:#x}", address);
}
