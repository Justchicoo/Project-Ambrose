/*
 * Project Ambrose by Imjustchico
 * Reads a client type dump (format v2) into raw classes with a streaming JSON parser that checks the type of every field it knows, and builds a validated TypeCatalog from them: every hash recomputed, ids and base chains checked, aliases collapsed into their classes, and every property type classified.
 */

#ifndef AMBROSE_TYPEDUMPLOADER_H
#define AMBROSE_TYPEDUMPLOADER_H

#include "TypeRegistry.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace TypeDumpLoader
{
    inline constexpr int64 SupportedVersion = 2;

    struct RawProperty
    {
        std::string Name;
        std::optional<std::string> Type;
        std::optional<std::string> Container;
        std::optional<uint64> Id;
        std::optional<uint64> Offset;
        std::optional<uint64> Flags;
        std::optional<uint64> Hash;
        std::optional<bool> Dynamic;
        std::optional<bool> Singleton;
        std::optional<bool> Pointer;
        std::vector<std::pair<std::string, std::variant<int64, std::string>>> Options;
    };

    struct RawClass
    {
        std::string Key;
        std::optional<std::string> Name;
        std::optional<uint64> Hash;
        std::vector<std::string> Bases;
        std::vector<RawProperty> Properties;
    };

    struct RawDump
    {
        std::optional<int64> Version;
        bool HasClasses = false;
        std::vector<RawClass> Classes;
    };

    bool Parse(std::string_view text, RawDump& dump, std::vector<std::string>& errors);
    std::string Canonicalize(std::string_view typeName);
    std::string Normalize(std::string_view typeName);
}

class TypeCatalogBuilder
{
public:
    static TypeCatalogPtr Build(TypeDumpLoader::RawDump dump, std::string sourceName, std::string sha256, uint64 generation, std::span<ViewDefinition const* const> views, std::vector<std::string>& errors);
};

#endif
