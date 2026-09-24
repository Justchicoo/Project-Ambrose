/*
 * Project Ambrose by Imjustchico
 * Generates a patch manifest from the user's own Wizard101 installation and writes XML and binary output for one revision.
 */

#include "PatchListGenerator.h"

#include "Environment.h"
#include "LogConfig.h"

#include <fmt/format.h>

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
  --rules <file>       optional Src|Tar|Package|FileType rules file
  --reference <file>   compare generated fields with a supplied LatestFileList.xml
  --help               print this text
)";

    struct Arguments
    {
        std::optional<std::filesystem::path> Client;
        std::filesystem::path Output = "patch-output";
        std::optional<std::filesystem::path> Rules;
        std::optional<std::filesystem::path> Reference;
        bool Help = false;
    };

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
    if (!arguments->Client)
        if (std::optional<std::string> client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR"); client && !client->empty())
            arguments->Client = LogConfig::Utf8Path(*client);
    if (!arguments->Client)
    {
        std::cerr << "patchlist_generator: --client or AMBROSE_CLIENT_DIR is required\n";
        return BadUsage;
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
