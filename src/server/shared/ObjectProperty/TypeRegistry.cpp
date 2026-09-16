/*
 * Project Ambrose by Imjustchico
 * Reads the type dump file, hashes it with SHA-256 for revision pinning, builds and validates a new catalog generation off to the side, and publishes it atomically, logging the load time, size and every problem, and keeping the active catalog when a load fails.
 */

#include "TypeRegistry.h"
#include "ConfigMgr.h"
#include "Hex.h"
#include "Log.h"
#include "SHA256.h"
#include "TypeDumpLoader.h"

#include <chrono>
#include <fstream>
#include <span>

namespace
{
    constexpr std::uintmax_t MaxDumpBytes = std::uintmax_t{ 512 } << 20;
}

ClassInfo const* TypeCatalog::FindClass(uint32 hash) const noexcept
{
    auto const found = _byHash.find(hash);
    return found == _byHash.end() ? nullptr : found->second;
}

ClassInfo const* TypeCatalog::FindClass(std::string_view name) const noexcept
{
    auto const found = _byName.find(name);
    return found == _byName.end() ? nullptr : found->second;
}

std::size_t TypeCatalog::GetClassCount(ClassKind kind) const noexcept
{
    return _kindCounts[static_cast<std::size_t>(kind)];
}

TypeRegistry::TypeRegistry() = default;

TypeRegistry& TypeRegistry::Instance()
{
    static TypeRegistry instance;
    return instance;
}

bool TypeRegistry::LoadFromFile(std::filesystem::path const& path)
{
    std::string const sourceName = ConfigMgr::PathToUtf8(path);
    std::error_code error;
    std::uintmax_t const size = std::filesystem::file_size(path, error);
    std::string text;
    if (error)
        text.clear();
    else if (size > MaxDumpBytes)
        error = std::make_error_code(std::errc::file_too_large);
    else
    {
        std::ifstream stream(path, std::ios::binary);
        text.resize(static_cast<std::size_t>(size));
        if (!stream || !stream.read(text.data(), static_cast<std::streamsize>(text.size())))
            error = std::make_error_code(std::errc::io_error);
    }
    if (error)
    {
        std::lock_guard const lock(_writeMutex);
        _errors = { fmt::format("cannot read the type dump {}: {}", sourceName, error.message()) };
        LOG_ERROR(LogFilter, "{}; keeping the active type dump", _errors.front());
        return false;
    }
    return Build(text, sourceName);
}

bool TypeRegistry::LoadFromText(std::string_view text, std::string sourceName)
{
    return Build(text, std::move(sourceName));
}

bool TypeRegistry::Build(std::string_view text, std::string sourceName)
{
    std::lock_guard const lock(_writeMutex);
    auto const start = std::chrono::steady_clock::now();
    std::string const sha256 = Hex::Encode(SHA256::GetDigestOf(std::span<uint8 const>(reinterpret_cast<uint8 const*>(text.data()), text.size())));
    std::vector<std::string> errors;
    TypeDumpLoader::RawDump dump;
    TypeCatalogPtr catalog;
    if (TypeDumpLoader::Parse(text, dump, errors))
        catalog = TypeCatalogBuilder::Build(std::move(dump), sourceName, sha256, _nextGeneration, errors);
    if (!catalog)
    {
        if (errors.empty())
            errors.push_back("the type dump could not be read");
        std::size_t const total = errors.size();
        if (errors.size() > MaxReportedErrors)
            errors.resize(MaxReportedErrors);
        for (std::string const& problem : errors)
            LOG_ERROR(LogFilter, "Type dump {}: {}", sourceName, problem);
        LOG_ERROR(LogFilter, "Type dump {} has {} problem(s); keeping the active type dump", sourceName, total);
        _errors = std::move(errors);
        return false;
    }

    ++_nextGeneration;
    _errors.clear();
    auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    LOG_INFO(LogFilter, "Loaded type dump generation {} from {} in {} ms: {} property classes, {} enums, {} value types, {} primitives, {} containers, {} opaque classes, {} properties and {} aliases, about {} MiB, SHA-256 {}",
        catalog->GetGeneration(), sourceName, elapsed.count(), catalog->GetClassCount(ClassKind::PropertyClass), catalog->GetClassCount(ClassKind::Enum), catalog->GetClassCount(ClassKind::ValueType),
        catalog->GetClassCount(ClassKind::Primitive), catalog->GetClassCount(ClassKind::Container), catalog->GetClassCount(ClassKind::Opaque), catalog->GetPropertyCount(), catalog->GetAliasCount(),
        (catalog->GetApproximateBytes() + (std::size_t{ 1 } << 19)) >> 20, catalog->GetSha256());
    _catalog.store(std::move(catalog));
    return true;
}

void TypeRegistry::Clear()
{
    std::lock_guard const lock(_writeMutex);
    _catalog.store(nullptr);
    _errors.clear();
}

TypeCatalogPtr TypeRegistry::GetCatalog() const
{
    return _catalog.load();
}

bool TypeRegistry::IsLoaded() const
{
    return _catalog.load() != nullptr;
}

uint64 TypeRegistry::GetGeneration() const
{
    TypeCatalogPtr const catalog = _catalog.load();
    return catalog ? catalog->GetGeneration() : 0;
}

std::vector<std::string> TypeRegistry::GetErrors() const
{
    std::lock_guard const lock(_writeMutex);
    return _errors;
}
