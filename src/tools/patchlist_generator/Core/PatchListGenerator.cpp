/*
 * Project Ambrose by Imjustchico
 * Walks installation files, derives patch packages and header metrics, and writes XML and binary manifests.
 */

#include "PatchListGenerator.h"

#include "Crc32.h"
#include "KiwadHeader.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "LatestFileListXml.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string_view>
#include <vector>

namespace
{
    using json = nlohmann::json;

    struct Rule
    {
        std::string Src;
        std::string Tar;
        std::string Package;
        uint32 Type = 0;
    };

    struct CachedFile
    {
        uintmax_t Size = 0;
        intmax_t Time = 0;
        uint32 CRC = 0;
        uint32 HeaderSize = 0;
        uint32 HeaderCRC = 0;
    };

    std::string RelativeName(std::filesystem::path const& path, std::filesystem::path const& root)
    {
        std::string result = std::filesystem::relative(path, root).generic_string();
        while (result.starts_with("./"))
            result.erase(0, 2);
        return result;
    }

    std::string Trim(std::string value)
    {
        std::size_t first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return {};
        std::size_t last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    std::map<std::string, Rule> ReadRules(std::optional<std::filesystem::path> const& path, std::string& error)
    {
        std::map<std::string, Rule> rules;
        if (!path)
            return rules;
        std::ifstream input(*path);
        if (!input)
        {
            error = fmt::format("cannot open rules file {}", ConfigMgr::PathToUtf8(*path));
            return {};
        }
        std::string line;
        std::size_t lineNumber = 0;
        while (std::getline(input, line))
        {
            ++lineNumber;
            line = Trim(std::move(line));
            if (line.empty() || line.starts_with('#'))
                continue;
            std::vector<std::string> fields;
            std::size_t start = 0;
            while (start <= line.size())
            {
                std::size_t end = line.find('|', start);
                fields.push_back(Trim(line.substr(start, end == std::string::npos ? std::string::npos : end - start)));
                if (end == std::string::npos)
                    break;
                start = end + 1;
            }
            if (fields.size() != 4)
            {
                error = fmt::format("rules file {} line {} needs src|tar|package|type", ConfigMgr::PathToUtf8(*path), lineNumber);
                return {};
            }
            try
            {
                Rule rule{ fields[0], fields[1], fields[2], static_cast<uint32>(std::stoul(fields[3])) };
                rules[rule.Src] = std::move(rule);
            }
            catch (std::exception const&)
            {
                error = fmt::format("rules file {} line {} has an invalid type", ConfigMgr::PathToUtf8(*path), lineNumber);
                return {};
            }
        }
        return rules;
    }

    std::map<std::string, CachedFile> ReadCache(std::filesystem::path const& path)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return {};
        json document;
        try
        {
            input >> document;
        }
        catch (json::exception const&)
        {
            return {};
        }
        std::map<std::string, CachedFile> result;
        for (auto const& [name, value] : document.items())
        {
            if (!value.is_object())
                continue;
            result[name] = {
                value.value("size", uintmax_t{ 0 }),
                value.value("time", intmax_t{ 0 }),
                value.value("crc", uint32{ 0 }),
                value.value("header_size", uint32{ 0 }),
                value.value("header_crc", uint32{ 0 })
            };
        }
        return result;
    }

    bool WriteCache(std::filesystem::path const& path, std::map<std::string, CachedFile> const& cache, std::string& error)
    {
        json document = json::object();
        for (auto const& [name, value] : cache)
            document[name] = {
                { "size", value.Size },
                { "time", value.Time },
                { "crc", value.CRC },
                { "header_size", value.HeaderSize },
                { "header_crc", value.HeaderCRC }
            };
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = fmt::format("cannot write cache {}", ConfigMgr::PathToUtf8(path));
            return false;
        }
        output << document.dump(2) << '\n';
        if (!output)
        {
            error = fmt::format("cannot finish cache {}", ConfigMgr::PathToUtf8(path));
            return false;
        }
        return true;
    }

    std::optional<CachedFile> ScanFile(std::filesystem::path const& path, std::string const& name, std::map<std::string, CachedFile>& cache,
        std::size_t& cacheHits, std::string& error)
    {
        std::error_code statusError;
        uintmax_t const size = std::filesystem::file_size(path, statusError);
        auto const time = std::filesystem::last_write_time(path, statusError).time_since_epoch().count();
        if (statusError)
        {
            error = fmt::format("cannot stat {}", ConfigMgr::PathToUtf8(path));
            return std::nullopt;
        }
        auto const cached = cache.find(name);
        if (cached != cache.end() && cached->second.Size == size && cached->second.Time == time)
        {
            ++cacheHits;
            return cached->second;
        }

        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            error = fmt::format("cannot open {}", ConfigMgr::PathToUtf8(path));
            return std::nullopt;
        }
        std::vector<uint8> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (input.bad())
        {
            error = fmt::format("cannot read {}", ConfigMgr::PathToUtf8(path));
            return std::nullopt;
        }
        CachedFile result{ size, time, Crc32::ComputeClient(bytes), static_cast<uint32>(size), Crc32::ComputeClient(bytes) };
        if (path.extension() == ".wad" || path.extension() == ".WAD")
        {
            std::optional<uint64> toc = KiwadHeader::MeasureTocLength(bytes);
            if (!toc || *toc > bytes.size() || *toc > UINT32_MAX)
            {
                error = fmt::format("{} is not a valid KIWAD archive", ConfigMgr::PathToUtf8(path));
                return std::nullopt;
            }
            result.HeaderSize = static_cast<uint32>(*toc);
            result.HeaderCRC = Crc32::ComputeClient(std::span<uint8 const>(bytes.data(), *toc));
        }
        cache[name] = result;
        return result;
    }

    bool IsWad(std::string_view name)
    {
        return name.size() >= 4 && (name.ends_with(".wad") || name.ends_with(".WAD"));
    }

    std::string PackageName(std::string const& name)
    {
        if (name == "Data/GameData/Root.wad")
            return "Base";
        if (name.starts_with("Data/GameData/") && IsWad(name))
            return std::filesystem::path(name).stem().string();
        if (name.starts_with("PatchClient/"))
            return "PatchClient";
        return "Base";
    }

    bool IsWithin(std::filesystem::path const& path, std::filesystem::path const& directory)
    {
        std::filesystem::path const normalizedPath = std::filesystem::absolute(path).lexically_normal();
        std::filesystem::path const normalizedDirectory = std::filesystem::absolute(directory).lexically_normal();
        auto pathIt = normalizedPath.begin();
        auto directoryIt = normalizedDirectory.begin();
        for (; directoryIt != normalizedDirectory.end(); ++directoryIt, ++pathIt)
            if (pathIt == normalizedPath.end() || *pathIt != *directoryIt)
                return false;
        return true;
    }

    LatestFileList::FileRecord MakeRecord(std::string const& name, CachedFile const& file, Rule const* rule)
    {
        LatestFileList::FileRecord record;
        record.SrcFileName = name;
        record.TarFileName = rule ? rule->Tar : name;
        record.FileType = rule && rule->Type != 0 ? rule->Type : (IsWad(name) ? 3u : 1u);
        record.Size = static_cast<uint32>(file.Size);
        record.HeaderSize = file.HeaderSize;
        record.HeaderCRC = file.HeaderCRC;
        record.CRC = file.CRC;
        return record;
    }
}

std::optional<PatchListGenerator::Result> PatchListGenerator::Generate(Options const& options, std::string& error)
{
    if (options.Client.empty() || !std::filesystem::is_directory(options.Client))
    {
        error = "client must name an existing installation directory";
        return std::nullopt;
    }
    std::ifstream revisionFile(options.Client / "Bin" / "revision.dat");
    std::string revision;
    if (!revisionFile || !std::getline(revisionFile, revision))
    {
        error = fmt::format("cannot read {}", ConfigMgr::PathToUtf8(options.Client / "Bin" / "revision.dat"));
        return std::nullopt;
    }
    revision = Trim(std::move(revision));
    if (revision.empty())
    {
        error = "Bin/revision.dat is empty";
        return std::nullopt;
    }

    std::map<std::string, Rule> const rules = ReadRules(options.Rules, error);
    if (!error.empty())
        return std::nullopt;
    std::filesystem::path const revisionDirectory = options.Output / ("V_" + revision);
    std::error_code directoryError;
    std::filesystem::create_directories(revisionDirectory, directoryError);
    if (directoryError)
    {
        error = fmt::format("cannot create {}", ConfigMgr::PathToUtf8(revisionDirectory));
        return std::nullopt;
    }

    std::map<std::string, CachedFile> cache = ReadCache(options.Output / ".patchlist-cache.json");
    Result result;
    result.Revision = revision;
    result.OutputDirectory = revisionDirectory;
    std::map<std::string, std::vector<LatestFileList::FileRecord>> packages;
    std::error_code iteratorError;
    for (std::filesystem::recursive_directory_iterator it(options.Client, iteratorError), end; it != end && !iteratorError; it.increment(iteratorError))
    {
        if (IsWithin(it->path(), options.Output))
        {
            if (it->is_directory())
                it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file())
            continue;
        std::string const name = RelativeName(it->path(), options.Client);
        std::string package = PackageName(name);
        auto const rule = rules.find(name);
        if (rule != rules.end() && !rule->second.Package.empty())
            package = rule->second.Package;
        std::optional<CachedFile> file = ScanFile(it->path(), name, cache, result.CacheHits, error);
        if (!file)
            return std::nullopt;
        packages[package].push_back(MakeRecord(name, *file, rule == rules.end() ? nullptr : &rule->second));
        ++result.FilesScanned;
    }
    if (iteratorError)
    {
        error = fmt::format("cannot walk {}: {}", ConfigMgr::PathToUtf8(options.Client), iteratorError.message());
        return std::nullopt;
    }

    result.Manifest.About.Version = 1;
    for (auto& [name, records] : packages)
    {
        std::sort(records.begin(), records.end(), [](auto const& left, auto const& right) { return left.SrcFileName < right.SrcFileName; });
        result.Manifest.Packages.push_back({ name, std::move(records) });
        result.Manifest.TableOrder.push_back(name);
    }
    std::sort(result.Manifest.Packages.begin(), result.Manifest.Packages.end(), [](auto const& left, auto const& right) { return left.Name < right.Name; });
    result.Manifest.TableOrder.clear();
    for (auto const& package : result.Manifest.Packages)
        result.Manifest.TableOrder.push_back(package.Name);
    result.Manifest.TableOrder.push_back("About");

    std::ofstream xml(revisionDirectory / "LatestFileList.xml", std::ios::binary | std::ios::trunc);
    std::ofstream binary(revisionDirectory / "LatestFileList.bin", std::ios::binary | std::ios::trunc);
    std::vector<uint8> bytes = result.Manifest.WriteBinary();
    if (!xml || !binary)
    {
        error = fmt::format("cannot write manifest in {}", ConfigMgr::PathToUtf8(revisionDirectory));
        return std::nullopt;
    }
    xml << LatestFileListXml::Write(result.Manifest);
    binary.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!xml || !binary || !WriteCache(options.Output / ".patchlist-cache.json", cache, error))
        return std::nullopt;

    if (options.Reference)
    {
        std::ifstream reference(*options.Reference);
        if (!reference)
        {
            error = fmt::format("cannot open reference {}", ConfigMgr::PathToUtf8(*options.Reference));
            return std::nullopt;
        }
        std::string const text((std::istreambuf_iterator<char>(reference)), std::istreambuf_iterator<char>());
        LatestFileList const expected = LatestFileListXml::Read(text);
        std::cout << Diff(result.Manifest, expected);
    }
    return result;
}

std::string PatchListGenerator::Diff(LatestFileList const& generated, LatestFileList const& reference)
{
    std::ostringstream output;
    std::map<std::string, LatestFileList::FileRecord const*> actual;
    std::map<std::string, LatestFileList::FileRecord const*> expected;
    for (auto const& package : generated.Packages)
        for (auto const& record : package.Records)
            actual[package.Name + "\n" + record.SrcFileName] = &record;
    for (auto const& package : reference.Packages)
        for (auto const& record : package.Records)
            expected[package.Name + "\n" + record.SrcFileName] = &record;
    std::size_t differences = 0;
    for (auto const& [key, record] : expected)
    {
        auto const found = actual.find(key);
        if (found == actual.end())
        {
            output << "missing " << key << '\n';
            ++differences;
            continue;
        }
        if (found->second->FileType != record->FileType || found->second->Size != record->Size || found->second->CRC != record->CRC ||
            found->second->HeaderSize != record->HeaderSize || found->second->HeaderCRC != record->HeaderCRC)
        {
            output << "different " << key << '\n';
            ++differences;
        }
    }
    for (auto const& [key, record] : actual)
    {
        if (!expected.contains(key))
        {
            output << "unexpected " << key << '\n';
            ++differences;
        }
    }
    output << differences << " differences\n";
    return output.str();
}
