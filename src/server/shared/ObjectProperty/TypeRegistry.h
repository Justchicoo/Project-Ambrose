/*
 * Project Ambrose by Imjustchico
 * The loaded type dump as an immutable catalog of classes found by hash or name, aliases included, and the registry that builds a catalog off to the side, validates it completely, and swaps it in, keeping the active one when a load fails.
 */

#ifndef AMBROSE_TYPEREGISTRY_H
#define AMBROSE_TYPEREGISTRY_H

#include "TypeInfo.h"

#include <array>
#include <atomic>
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
};

using TypeCatalogPtr = std::shared_ptr<TypeCatalog const>;

class TypeRegistry
{
public:
    static constexpr char const* LogFilter = "server.loading";
    static constexpr std::size_t MaxReportedErrors = 100;

    TypeRegistry();
    TypeRegistry(TypeRegistry const&) = delete;
    TypeRegistry& operator=(TypeRegistry const&) = delete;

    static TypeRegistry& Instance();

    bool LoadFromFile(std::filesystem::path const& path);
    bool LoadFromText(std::string_view text, std::string sourceName);
    void Clear();

    TypeCatalogPtr GetCatalog() const;
    bool IsLoaded() const;
    uint64 GetGeneration() const;
    std::vector<std::string> GetErrors() const;

private:
    bool Build(std::string_view text, std::string sourceName);

    std::atomic<TypeCatalogPtr> _catalog;
    mutable std::mutex _writeMutex;
    std::vector<std::string> _errors;
    uint64 _nextGeneration = 1;
};

#define sTypeRegistry TypeRegistry::Instance()

#endif
