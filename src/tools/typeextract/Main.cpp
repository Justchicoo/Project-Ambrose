/*
 * Project Ambrose by Imjustchico
 * typeextract entry point: with --exit-when-input-ends first watches its input and exits 1 as soon as that input ends, which happens once the program that started it and holds the input open is gone, then reads and parses the dump named by --compare before anything else, builds the type dump of the user's own install by emulating its client program, using the install named by --client or AMBROSE_CLIENT_DIR or else the newest revision found on the machine, writes it to --out or the Ambrose data folder, asking for --out when that folder or the revision cannot name the file, compares it with the dump read earlier, and exits 0 on success, 1 when reading the comparison dump, extraction, validation, naming or writing the output fails, and 2 on bad usage.
 */

#include "ChildProcess.h"
#include "ClientLocator.h"
#include "ClientSystem.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "TypeDumpLoader.h"
#include "TypeDumpWriter.h"
#include "TypeExtraction.h"

#include <fmt/format.h>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr int Success = 0;
    constexpr int Failure = 1;
    constexpr int BadUsage = 2;

    constexpr std::string_view Usage = R"(Usage: typeextract [options]

Builds the type dump of your own Wizard101 install by emulating its client program.
The game is never launched, and nothing is written into the install.

Options:
  --client <dir>     the install (default: AMBROSE_CLIENT_DIR, else the newest install found on this machine)
  --out <file>       where to write the dump (default: types/<revision>.json in the Ambrose data folder)
  --compare <dump>   compare the result with another type dump and print every difference
  --quiet            print only errors
  --exit-when-input-ends
                     exit as soon as standard input ends, for a program that runs typeextract
                     and holds its input open, so typeextract never outlives it
  --help             print this text

Exit status: 0 on success, 1 when the dump to compare with cannot be read, extraction,
validation or writing fails, or input ends with --exit-when-input-ends, 2 on bad usage.
)";

    struct Arguments
    {
        std::optional<std::string> Client;
        std::optional<std::string> Out;
        std::optional<std::string> Compare;
        bool Quiet = false;
        bool ExitWhenInputEnds = false;
        bool Help = false;
    };

    std::optional<Arguments> Parse(std::vector<std::string> const& args, std::string& error)
    {
        Arguments parsed;
        for (std::size_t index = 1; index < args.size(); ++index)
        {
            std::string const& arg = args[index];
            if (arg == "--help" || arg == "-h")
                parsed.Help = true;
            else if (arg == "--quiet")
                parsed.Quiet = true;
            else if (arg == "--exit-when-input-ends")
                parsed.ExitWhenInputEnds = true;
            else if (arg == "--client" || arg == "--out" || arg == "--compare")
            {
                if (index + 1 >= args.size() || args[index + 1].starts_with("--"))
                {
                    error = fmt::format("{} needs a value", arg);
                    return std::nullopt;
                }
                std::string const& value = args[++index];
                if (arg == "--client")
                    parsed.Client = value;
                else if (arg == "--out")
                    parsed.Out = value;
                else
                    parsed.Compare = value;
            }
            else
            {
                error = fmt::format("unknown argument {}", arg);
                return std::nullopt;
            }
        }
        return parsed;
    }

    std::optional<TypeDumpLoader::RawDump> ReadDump(std::string const& name, std::string& error)
    {
        std::ifstream stream(ConfigMgr::PathFromUtf8(name), std::ios::binary);
        if (!stream)
        {
            error = fmt::format("cannot open {}", name);
            return std::nullopt;
        }
        std::string const text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        if (stream.bad())
        {
            error = fmt::format("cannot read {}", name);
            return std::nullopt;
        }
        TypeDumpLoader::RawDump dump;
        std::vector<std::string> errors;
        if (!TypeDumpLoader::Parse(text, dump, errors))
        {
            error = fmt::format("{} is not a type dump: {}", name, errors.empty() ? std::string("unknown error") : errors.front());
            return std::nullopt;
        }
        return dump;
    }

    uint64 RevisionNumber(std::string_view revision)
    {
        std::size_t index = revision.starts_with('r') ? 1 : 0;
        uint64 number = 0;
        for (; index < revision.size() && revision[index] >= '0' && revision[index] <= '9'; ++index)
            number = number * 10 + static_cast<uint64>(revision[index] - '0');
        return number;
    }

    int Run(std::vector<std::string> const& args)
    {
        std::string error;
        std::optional<Arguments> arguments = Parse(args, error);
        if (!arguments)
        {
            std::cerr << "typeextract: " << error << "\n\n" << Usage;
            return BadUsage;
        }
        if (arguments->Help)
        {
            std::cout << Usage;
            return Success;
        }
        if (arguments->ExitWhenInputEnds)
            ChildProcess::ExitWhenInputEnds(Failure);
        auto say = [&](std::string const& text)
        {
            if (!arguments->Quiet)
                std::cout << "typeextract: " << text << std::endl;
        };

        std::optional<TypeDumpLoader::RawDump> reference;
        if (arguments->Compare)
        {
            reference = ReadDump(*arguments->Compare, error);
            if (!reference)
            {
                std::cerr << "typeextract: " << error << "\n";
                return Failure;
            }
            say(fmt::format("read {} classes from {} to compare with", reference->Classes.size(), *arguments->Compare));
        }

        LocalClientSystem const system;
        std::filesystem::path const dataFolder = ClientLocator::GetDataFolder(system);
        if (!arguments->Out && (dataFolder.empty() || !dataFolder.is_absolute()))
        {
            std::cerr << fmt::format("typeextract: the Ambrose data folder {} on this machine, so name the output file with --out\n",
                dataFolder.empty() ? std::string("cannot be found") : fmt::format("{} is not an absolute path", ClientLocator::PathText(dataFolder)));
            return Failure;
        }
        if (!arguments->Client)
        {
            if (std::optional<std::string> found = Ambrose::GetEnv("AMBROSE_CLIENT_DIR"); found && !found->empty())
                arguments->Client = std::move(found);
        }
        if (!arguments->Client)
        {
            std::vector<ClientCandidate> installs = ClientLocator::FindInstalls(system);
            if (installs.empty())
            {
                std::cerr << "typeextract: no Wizard101 install was found on this machine; name one with --client\n";
                return Failure;
            }
            std::stable_sort(installs.begin(), installs.end(), [](ClientCandidate const& a, ClientCandidate const& b) { return RevisionNumber(a.Install.Revision) > RevisionNumber(b.Install.Revision); });
            arguments->Client = ClientLocator::PathText(installs.front().Install.Root);
            say(fmt::format("using {}, found through {}", installs.front().Install.Describe(), installs.front().Source));
        }

        TypeExtractionOptions options;
        options.ClientDir = ConfigMgr::PathFromUtf8(*arguments->Client);
        options.Progress = [&](std::string_view text) { say(std::string(text)); };
        TypeExtractionResult const result = TypeExtraction::Extract(options);
        for (std::string const& line : result.Discovered)
            say(line);
        for (auto const& [kind, samples] : result.ProblemSamples)
            for (std::string const& sample : samples)
                std::cerr << fmt::format("typeextract: {}: {}\n", kind, sample);
        for (auto const& [call, count] : result.UnhandledApiCalls)
            say(fmt::format("stubbed {} ({} calls)", call, count));
        if (!result.Succeeded())
        {
            std::cerr << "typeextract: " << result.Error << "\n";
            return Failure;
        }
        say(fmt::format("{} classes and {} properties from {} in {} ms (load {} ms, initializers {} ms, discovery {} ms, getters, races and walk {} ms; {} of {} lazy getters ran, {} faulted, {} races, {} MiB of guest heap)",
            result.Stats.Classes, result.Stats.Properties, result.Metadata.Revision, result.Stats.TotalMilliseconds, result.Stats.LoadMilliseconds, result.Stats.InitializeMilliseconds,
            result.Stats.DiscoverMilliseconds, result.Stats.WalkMilliseconds, result.Stats.LazyGettersRun, result.Stats.LazyGetters, result.Stats.LazyGettersFaulted, result.Stats.Races, result.Stats.HeapBytes >> 20));

        std::optional<std::filesystem::path> const out = arguments->Out ? std::optional<std::filesystem::path>(ConfigMgr::PathFromUtf8(*arguments->Out)) : TypeExtraction::DefaultOutputPath(dataFolder, result.Metadata.Revision);
        if (!out)
        {
            std::cerr << fmt::format("typeextract: the revision {} and the Ambrose data folder {} cannot name the output file, so name it with --out\n", result.Metadata.Revision, ClientLocator::PathText(dataFolder));
            return Failure;
        }
        if (!TypeDumpWriter::Save(*out, TypeDumpWriter::ToJson(result.Dump, result.Metadata), error))
        {
            std::cerr << "typeextract: " << error << "\n";
            return Failure;
        }
        say(fmt::format("wrote {}", ClientLocator::PathText(*out)));

        if (reference)
        {
            std::vector<TypeDumpDifference> const differences = TypeDumpWriter::Compare(result.Dump, *reference);
            for (TypeDumpDifference const& difference : differences)
                std::cout << TypeDumpWriter::Describe(difference) << "\n";
            say(fmt::format("{} differences from {}", differences.size(), *arguments->Compare));
        }
        return Success;
    }
}

int main(int argc, char** argv)
{
    try
    {
        Ambrose::UseUtf8Console();
        return Run(Ambrose::GetArguments(argc, argv));
    }
    catch (std::exception const& error)
    {
        std::cerr << "typeextract: " << error.what() << "\n";
        return Failure;
    }
}
