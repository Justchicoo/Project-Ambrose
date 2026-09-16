/*
 * Project Ambrose by Imjustchico
 * The libFuzzer target for the compact ObjectProperty decoder: the first input byte picks text enums, trailing bytes and whether the rest is an enveloped message field, the rest is decoded against the fuzz corpus catalog under tight limits, and anything that decodes must re-encode and decode back equal; the corpus seeds, each with its mode byte, are written into the first corpus folder named on the command line.
 */

#include "ObjectFuzzCorpus.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

namespace
{
    TypeCatalogPtr& Catalog()
    {
        static TypeCatalogPtr catalog;
        return catalog;
    }

    void WriteSeeds(std::filesystem::path const& folder)
    {
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        std::size_t index = 0;
        for (ObjectFuzzCorpus::Seed const& seed : ObjectFuzzCorpus::MakeSeeds(Catalog()))
        {
            std::ofstream stream(folder / ("seed-" + std::to_string(index++) + ".bin"), std::ios::binary | std::ios::trunc);
            char const mode = static_cast<char>(seed.Mode);
            stream.write(&mode, 1);
            stream.write(reinterpret_cast<char const*>(seed.Bytes.data()), static_cast<std::streamsize>(seed.Bytes.size()));
        }
    }
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv)
{
    std::string error;
    Catalog() = ObjectFuzzCorpus::LoadCatalog(error);
    if (!Catalog())
    {
        std::cerr << "the fuzz catalog did not load: " << error << '\n';
        std::abort();
    }
    for (int index = 1; index < *argc; ++index)
    {
        std::string_view const argument = (*argv)[index];
        if (!argument.empty() && argument.front() != '-')
        {
            WriteSeeds(std::filesystem::path(argument));
            break;
        }
    }
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(uint8_t const* data, std::size_t size)
{
    if (size == 0)
        return 0;
    uint8 const mode = data[0];
    std::span<uint8 const> const blob(data + 1, size - 1);
    DecodeResult const decoded = ObjectFuzzCorpus::Decode(Catalog(), mode, blob);
    std::string const problem = ObjectFuzzCorpus::CheckDecoded(Catalog(), mode, blob, decoded);
    if (!problem.empty())
    {
        std::cerr << problem << '\n';
        std::abort();
    }
    return 0;
}
