/*
 * Project Ambrose by Imjustchico
 * Writes a format v2 dump model as JSON with classes in name order, properties, options and duplicates in model order and the extraction's revision, executable SHA-256 and tool name at the root, saves it through a uniquely named temporary file in the target's folder, and compares two dump models paired by class, property and option name, describing every difference in class, property and option presence, class hash, bases, property and option order, property fields and option values.
 */

#ifndef AMBROSE_TYPEDUMPWRITER_H
#define AMBROSE_TYPEDUMPWRITER_H

#include "TypeDumpLoader.h"

#include <filesystem>
#include <string>
#include <vector>

struct TypeDumpMetadata
{
    std::string Revision;
    std::string ExecutableSha256;
    std::string Extractor;
};

struct TypeDumpDifference
{
    std::string Class;
    std::string Property;
    std::string Field;
    std::string Ours;
    std::string Theirs;
};

namespace TypeDumpWriter
{
    std::string ToJson(TypeDumpLoader::RawDump const& dump, TypeDumpMetadata const& metadata);
    bool Save(std::filesystem::path const& path, std::string const& text, std::string& error);
    std::vector<TypeDumpDifference> Compare(TypeDumpLoader::RawDump const& ours, TypeDumpLoader::RawDump const& theirs);
    std::string Describe(TypeDumpDifference const& difference);
}

#endif
