/*
 * Project Ambrose by Imjustchico
 * Converts a user's own JSON type dump into the revision-stamped binary registry cache used for fast startup.
 */

#include "ConfigMgr.h"
#include "TypeDumpLoader.h"
#include "TypeRegistryBinary.h"

#include <fmt/format.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    if (argc != 5 || std::string_view(argv[1]) != "--input" || std::string_view(argv[3]) != "--output")
    {
        std::cerr << "usage: typeregbuild --input <dump.json> --output <registry.bin>\n";
        return 2;
    }

    std::filesystem::path const input = ConfigMgr::PathFromUtf8(argv[2]);
    std::filesystem::path const output = ConfigMgr::PathFromUtf8(argv[4]);
    std::ifstream stream(input, std::ios::binary);
    if (!stream)
    {
        std::cerr << fmt::format("typeregbuild: cannot open {}\n", ConfigMgr::PathToUtf8(input));
        return 1;
    }
    std::string const json((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad())
    {
        std::cerr << fmt::format("typeregbuild: cannot read {}\n", ConfigMgr::PathToUtf8(input));
        return 1;
    }
    TypeDumpLoader::RawDump dump;
    std::vector<std::string> errors;
    if (!TypeDumpLoader::Parse(json, dump, errors))
    {
        std::cerr << fmt::format("typeregbuild: {} is not a valid type dump: {}\n", ConfigMgr::PathToUtf8(input), errors.empty() ? std::string("unknown error") : errors.front());
        return 1;
    }
    std::string error;
    if (!TypeRegistryBinary::Write(output, dump, input.stem().string(), error))
    {
        std::cerr << fmt::format("typeregbuild: {}\n", error);
        return 1;
    }
    std::cout << fmt::format("wrote {} bytes for revision {} to {}\n", json.size(), input.stem().string(), ConfigMgr::PathToUtf8(output));
    return 0;
}
