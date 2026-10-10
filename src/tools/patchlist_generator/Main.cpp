/*
 * Project Ambrose by Imjustchico
 * Generates a patch manifest from the user's own Wizard101 installation and writes XML and binary output for one revision, or prints the client's CRC of one file or byte range.
 */

#include "PatchListGenerator.h"

#include "Environment.h"
#include "LogConfig.h"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace
{
    constexpr int Success = 0;
    constexpr int Failure = 1;
    constexpr int BadUsage = 2;
    constexpr std::string_view Usage = R"(Usage: patchlist_generator [options]

Options:
  --client <dir>       the Wizard101 installation (default: AMBROSE_CLIENT_DIR)
  --out <dir>          output directory (default: patch-output)
  --rules <file>       Src|Tar|Package|FileType and skip|pattern rules (default: patchlist_generator.conf,
                       else patchlist_generator.conf.dist, beside this program)
  --reference <file>   compare with a LatestFileList.bin or .xml, such as the install's own
                       PatchInfo/LatestFileList.bin, then copy its FileType and CompressedHeaderSize
  --crc <file> [offset length]
                       print the client's CRC of the file, or of length bytes from offset, and exit
  --help               print this text
)";

    struct Arguments
    {
        std::optional<std::filesystem::path> Client;
        std::filesystem::path Output = "patch-output";
        std::optional<std::filesystem::path> Rules;
        std::optional<std::filesystem::path> Reference;
        std::optional<std::filesystem::path> CrcFile;
        uint64 CrcOffset = 0;
        std::optional<uint64> CrcLength;
        bool Help = false;
    };

    std::optional<uint64> Number(std::string const& text)
    {
        if (text.empty() || !std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c); }))
            return std::nullopt;
        try
        {
            return std::stoull(text);
        }
        catch (std::exception const&)
        {
            return std::nullopt;
        }
    }

    std::optional<Arguments> Parse(std::vector<std::string> const& args, std::string& error)
    {
        Arguments result;
        for (std::size_t index = 1; index < args.size(); ++index)
        {
            std::string const& arg = args[index];
            if (arg == "--help" || arg == "-h")
                result.Help = true;
            else if (arg == "--client" || arg == "--out" || arg == "--rules" || arg == "--reference")
            {
                if (index + 1 >= args.size() || args[index + 1].starts_with("--"))
                {
                    error = fmt::format("{} needs a value", arg);
                    return std::nullopt;
                }
                std::filesystem::path const value = LogConfig::Utf8Path(args[++index]);
                if (arg == "--client")
                    result.Client = value;
                else if (arg == "--out")
                    result.Output = value;
                else if (arg == "--rules")
                    result.Rules = value;
                else
                    result.Reference = value;
            }
            else if (arg == "--crc")
            {
                if (index + 1 >= args.size() || args[index + 1].starts_with("--"))
                {
                    error = "--crc needs a file";
                    return std::nullopt;
                }
                result.CrcFile = LogConfig::Utf8Path(args[++index]);
                if (index + 1 < args.size() && !args[index + 1].starts_with("--"))
                {
                    std::optional<uint64> const offset = Number(args[index + 1]);
                    std::optional<uint64> const length = index + 2 < args.size() ? Number(args[index + 2]) : std::nullopt;
                    if (!offset || !length)
                    {
                        error = "--crc takes a file, or a file, an offset and a length";
                        return std::nullopt;
                    }
                    result.CrcOffset = *offset;
                    result.CrcLength = *length;
                    index += 2;
                }
            }
            else
            {
                error = fmt::format("unknown option {}", arg);
                return std::nullopt;
            }
        }
        return result;
    }
}

int main(int argc, char** argv)
{
    std::vector<std::string> args;
    for (int index = 0; index < argc; ++index)
        args.emplace_back(argv[index]);
    std::string error;
    std::optional<Arguments> arguments = Parse(args, error);
    if (!arguments)
    {
        std::cerr << "patchlist_generator: " << error << "\n\n" << Usage;
        return BadUsage;
    }
    if (arguments->Help)
    {
        std::cout << Usage;
        return Success;
    }
    if (arguments->CrcFile)
    {
        std::optional<uint32> const crc = PatchListGenerator::FileCrc(*arguments->CrcFile, arguments->CrcOffset, arguments->CrcLength, error);
        if (!crc)
        {
            std::cerr << "patchlist_generator: " << error << '\n';
            return Failure;
        }
        std::cout << *crc << '\n';
        return Success;
    }
    if (!arguments->Client)
        if (std::optional<std::string> client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR"); client && !client->empty())
            arguments->Client = LogConfig::Utf8Path(*client);
    if (!arguments->Client)
    {
        std::cerr << "patchlist_generator: --client or AMBROSE_CLIENT_DIR is required\n";
        return BadUsage;
    }

    if (!arguments->Rules)
        for (char const* name : { "patchlist_generator.conf", "patchlist_generator.conf.dist" })
            if (std::filesystem::path const candidate = Ambrose::GetExecutableDirectory() / name; std::filesystem::is_regular_file(candidate))
            {
                arguments->Rules = candidate;
                break;
            }

    PatchListGenerator::Options options{ *arguments->Client, arguments->Output, arguments->Rules, arguments->Reference };
    std::optional<PatchListGenerator::Result> result = PatchListGenerator::Generate(options, error);
    if (!result)
    {
        std::cerr << "patchlist_generator: " << error << '\n';
        return Failure;
    }
    std::cout << fmt::format("patchlist_generator: wrote {} files to {} ({} cache hits)\n", result->FilesScanned,
        result->OutputDirectory.generic_string(), result->CacheHits);
    return Success;
}
