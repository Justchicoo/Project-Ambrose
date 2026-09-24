/*
 * Project Ambrose by Imjustchico
 * Reads and writes the versioned binary envelope around a type dump, validating its revision, payload size and SHA-256 before a registry builds from it.
 */

#ifndef AMBROSE_TYPEREGISTRYBINARY_H
#define AMBROSE_TYPEREGISTRYBINARY_H

#include "TypeDumpLoader.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace TypeRegistryBinary
{
    bool Write(std::filesystem::path const& output, TypeDumpLoader::RawDump const& dump, std::string_view revision, std::string& error);
    bool Read(std::filesystem::path const& input, std::string_view expectedRevision, TypeDumpLoader::RawDump& dump, std::string& revision, std::string& payloadHash, std::string& error);
}

#endif
