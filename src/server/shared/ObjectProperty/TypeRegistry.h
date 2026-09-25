/*
 * Project Ambrose by Imjustchico
 * The loaded type dump as an immutable catalog of classes found by hash or name, aliases included, each generation holding the typed view bindings made when it loaded, and the registry that builds a catalog off to the side, validates it and binds its views completely, and swaps it in, keeping the active one when a load fails. A supplement of classes the client uses but its dump does not describe, handed in by whoever keeps them in the dump's own shape, joins the dump's classes whenever a catalog is built: every class and property it names is checked against the hash formula the client uses, a new supplement rebuilds the dump already loaded, which the registry keeps for that, into a new generation off to the side, a supplement that fails either step keeps the one already in place and the catalog serving, and a class the dump turns out to describe after all is taken from the dump, because the client's own description wins over ours.
 */

#ifndef AMBROSE_TYPEREGISTRY_H
#define AMBROSE_TYPEREGISTRY_H

#include "TypeInfo.h"
#include "ViewDefinition.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct TypeNameHash
{
    using is_transparent = void;

    std::size_t operator()(std::string_view name) const noexcept { return std::hash<std::string_view>{}(name); }
};

class TypeCatalog
{
public:
    TypeCatalog(TypeCatalog const&) = delete;
    TypeCatalog& operator=(TypeCatalog const&) = delete;

    ClassInfo const* FindClass(uint32 hash) const noexcept;
    ClassInfo const* FindClass(std::string_view name) const noexcept;
    std::span<ClassInfo const* const> GetClasses() const noexcept { return _classList; }
    std::size_t GetClassCount(ClassKind kind) const noexcept;
    std::size_t GetPropertyCount() const noexcept { return _propertyCount; }
    std::size_t GetAliasCount() const noexcept { return _aliasCount; }
    std::size_t GetApproximateBytes() const noexcept { return _approximateBytes; }
    std::string const& GetSourceName() const noexcept { return _sourceName; }
    std::string const& GetSha256() const noexcept { return _sha256; }
    uint64 GetGeneration() const noexcept { return _generation; }
    ViewBinding const* FindView(ViewDefinition const& definition) const noexcept;
    std::span<ViewBinding const> GetViews() const noexcept { return _views; }

private:
    friend class TypeCatalogBuilder;

    TypeCatalog() = default;

    std::vector<std::unique_ptr<ClassInfo>> _classes;
    std::vector<ClassInfo const*> _classList;
    std::unordered_map<uint32, ClassInfo const*> _byHash;
    std::unordered_map<std::string, ClassInfo const*, TypeNameHash, std::equal_to<>> _byName;
    std::array<std::size_t, 6> _kindCounts{};
    std::size_t _propertyCount = 0;
    std::size_t _aliasCount = 0;
    std::size_t _approximateBytes = 0;
    std::string _sourceName;
    std::string _sha256;
    uint64 _generation = 0;
    std::vector<ViewBinding> _views;
};

using TypeCatalogPtr = std::shared_ptr<TypeCatalog const>;

class TypedViewRegistry;
namespace TypeDumpLoader
{
    struct RawDump;
}

class TypeRegistry
{
public:
    static constexpr char const* LogFilter = "server.loading";
    static constexpr std::size_t MaxReportedErrors = 100;

    explicit TypeRegistry(TypedViewRegistry* views = nullptr);
    TypeRegistry(TypeRegistry const&) = delete;
    TypeRegistry& operator=(TypeRegistry const&) = delete;

    static TypeRegistry& Instance();

    void SetViews(TypedViewRegistry* views);
    bool LoadFromFile(std::filesystem::path const& path);
    bool LoadBinary(std::filesystem::path const& path, std::string_view expectedRevision = {});
    bool LoadBinary(std::filesystem::path const& path, std::filesystem::path const& fallbackJson, std::string_view expectedRevision);
    bool LoadFromText(std::string_view text, std::string sourceName);
    void Clear();

    bool SetSupplement(TypeDumpLoader::RawDump supplement, std::string sourceName, std::vector<std::string>& errors);
    bool ClearSupplement(std::vector<std::string>& errors);
    std::size_t GetSupplementClassCount() const;
    bool IsFromSupplement(uint32 hash) const;

    TypeCatalogPtr GetCatalog() const;
    bool IsLoaded() const;
    uint64 GetGeneration() const;
    std::vector<std::string> GetErrors() const;

private:
    using RawDumpPtr = std::shared_ptr<TypeDumpLoader::RawDump const>;

    bool Build(std::string_view text, std::string sourceName);
    bool BuildRaw(TypeDumpLoader::RawDump dump, std::string sourceName, std::string sha256);
    bool BuildFrom(RawDumpPtr source, std::string sourceName, std::string sha256, RawDumpPtr supplement, std::string supplementSource,
        std::chrono::steady_clock::time_point start, std::vector<std::string> errors);
    bool Publish(TypeCatalogPtr catalog, std::vector<std::string> errors, std::string sourceName,
        std::chrono::steady_clock::time_point start);
    bool ReplaceSupplement(RawDumpPtr supplement, std::string sourceName, std::vector<std::string>& errors);

    TypedViewRegistry* _views;
    RawDumpPtr _source;
    std::string _sourceName;
    std::string _sourceSha256;
    RawDumpPtr _supplement;
    std::string _supplementSource;
    std::vector<uint32> _supplementHashes;
    std::atomic<TypeCatalogPtr> _catalog;
    mutable std::mutex _writeMutex;
    std::vector<std::string> _errors;
    uint64 _nextGeneration = 1;
};

#define sTypeRegistry TypeRegistry::Instance()

#endif
