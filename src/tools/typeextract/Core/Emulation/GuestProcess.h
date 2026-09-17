/*
 * Project Ambrose by Imjustchico
 * A guest Windows process: maps the main executable at its preferred base and the C and C++ runtime DLLs from the same folder with relocations, resolves imports through forwarders and api-ms-win-crt sets with a fallback to ucrtbase, refuses a runtime DLL or export the folder lacks by naming it and what needs it, turns every other import into a stub whose handler is looked up by name when called and whose failure is reported under the API's name, sets up the TEB, PEB and each module's TLS block, runs TLS callbacks and DLL entry points, and counts every stub call, naming the ones no handler covers.
 */

#ifndef AMBROSE_GUESTPROCESS_H
#define AMBROSE_GUESTPROCESS_H

#include "GuestHeap.h"
#include "Machine.h"
#include "PeImage.h"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct GuestModule
{
    std::string Name;
    std::filesystem::path Path;
    std::unique_ptr<PeImage> Image;
    uint64 Base = 0;
    uint64 Size = 0;
    std::optional<uint32> TlsIndex;
    std::vector<uint64> TlsCallbacks;
    bool Attached = false;

    uint64 ToAddress(uint32 rva) const noexcept { return Base + rva; }
};

class GuestProcess
{
public:
    static constexpr uint64 HeapBase = 0x10000000000;
    static constexpr uint64 DefaultHeapSize = 0x80000000;
    static constexpr uint64 TebAddress = 0x7FFD00000000;
    static constexpr uint64 PebAddress = 0x7FFD00010000;
    static constexpr uint64 TlsArrayAddress = 0x7FFD00002000;
    static constexpr uint64 StubBase = 0x7FF000000000;
    static constexpr uint64 StubSize = 16;
    static constexpr uint64 StubCapacity = 0x10000;
    static constexpr uint64 FirstDllBase = 0x180000000;
    static constexpr uint64 KernelModuleHandle = 0x7FF100000000;
    static constexpr uint32 MaxTlsModules = 64;

    using ApiHandler = std::function<uint64(GuestProcess& process)>;

    struct Options
    {
        std::filesystem::path Folder;
        std::vector<std::string> RealModules = { "ucrtbase.dll", "vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll", "msvcp140_atomic_wait.dll", "msvcp140_codecvt_ids.dll", "concrt140.dll" };
        uint64 HeapSize = DefaultHeapSize;
    };

    explicit GuestProcess(Options options);
    ~GuestProcess();

    GuestProcess(GuestProcess const&) = delete;
    GuestProcess& operator=(GuestProcess const&) = delete;

    GuestModule& LoadMain(std::string_view fileName);
    GuestModule& GetMain();
    GuestModule* FindModule(std::string_view name);
    GuestModule const* ModuleAt(uint64 address) const;
    std::vector<GuestModule const*> GetModules() const;

    void AttachRuntime();
    void Attach(GuestModule& module);

    void RegisterApi(std::string_view name, ApiHandler handler);
    bool HasApi(std::string_view name) const;
    uint64 ResolveImport(std::string_view dll, std::string_view name, std::optional<uint16> ordinal);
    std::optional<uint64> ResolveExport(GuestModule const& module, std::string_view name);
    std::optional<uint64> ResolveExportByOrdinal(GuestModule const& module, uint16 ordinal);

    uint64 Call(uint64 function, std::initializer_list<uint64> arguments, uint64 instructionBudget);
    uint64 Call(uint64 function, std::span<uint64 const> arguments, uint64 instructionBudget);

    uint64 StoreBytes(std::span<uint8 const> bytes);
    uint64 StoreCString(std::string_view text);
    uint64 StoreWideString(std::u16string_view text);

    Machine& GetMachine() noexcept;
    Machine const& GetMachine() const noexcept;
    GuestHeap& GetHeap() noexcept;
    GuestHeap const& GetHeap() const noexcept;
    std::filesystem::path const& GetFolder() const noexcept;

    std::string_view GetCurrentApiName() const noexcept;
    std::map<std::string, uint64> const& GetApiCallCounts() const noexcept;
    std::map<std::string, uint64> const& GetUnhandledApiCalls() const noexcept;
    std::string DescribeAddress(uint64 address) const;

private:
    struct State;
    std::unique_ptr<State> _state;
};

#endif
