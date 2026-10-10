/*
 * Project Ambrose by Imjustchico
 * Walks an installation through Src and Tar rules, type patterns, launcher banks and skip patterns, derives packages, file types and header metrics, notes the streamed WADs whose data is not all downloaded yet, writes XML and binary manifests, and diffs them against a reference list, taking from it only the CompressedHeaderSize it cannot derive.
 */

#include "PatchListGenerator.h"

#include "Compression.h"
#include "Crc32.h"
#include "KiwadHeader.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "LatestFileListXml.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
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

    constexpr uint32 CompressedFrame = 12;

    struct Rule
    {
        std::string Src;
        std::string Tar;
        std::string Package;
        uint32 Type = 0;
    };

    struct Rules
    {
        std::map<std::string, Rule> ByDisk;
        std::vector<std::string> Skip;
        std::vector<std::pair<std::string, uint32>> Types;
    };

    struct CachedFile
    {
        uintmax_t Size = 0;
        intmax_t Time = 0;
        uint32 CRC = 0;
        uint32 WadHeaderSize = 0;
        uint32 WadHeaderCRC = 0;
        uint32 Compressed = 0;
    };

    struct Found
    {
        std::filesystem::path Path;
        std::string Disk;
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

    std::string Lower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    bool GlobMatch(std::string_view pattern, std::string_view name)
    {
        if (pattern.empty())
            return name.empty();
        if (pattern.starts_with("**"))
        {
            std::string_view const rest = pattern.substr(pattern.size() > 2 && pattern[2] == '/' ? 3 : 2);
            if (rest.empty())
                return true;
            for (std::size_t index = 0; index <= name.size(); ++index)
                if ((index == 0 || name[index - 1] == '/') && GlobMatch(rest, name.substr(index)))
                    return true;
            return false;
        }
        if (pattern[0] == '*')
        {
            for (std::size_t index = 0; index <= name.size(); ++index)
            {
                if (GlobMatch(pattern.substr(1), name.substr(index)))
                    return true;
                if (index < name.size() && name[index] == '/')
                    return false;
            }
            return false;
        }
        if (name.empty() || (pattern[0] != '?' && std::tolower(static_cast<unsigned char>(pattern[0])) != std::tolower(static_cast<unsigned char>(name[0]))))
            return false;
        if (pattern[0] == '?' && name[0] == '/')
            return false;
        return GlobMatch(pattern.substr(1), name.substr(1));
    }

    std::string DiskName(std::string const& src, std::string const& tar, std::string const& package)
    {
        if (package == "PatchClient")
            return "PatchClient/" + (tar.empty() ? std::filesystem::path(src).filename().generic_string() : tar);
        return tar.empty() ? src : tar;
    }

    std::vector<std::string> SplitFields(std::string const& line)
    {
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
        return fields;
    }

    Rules ReadRules(std::optional<std::filesystem::path> const& path, std::string& error)
    {
        Rules rules;
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
            std::vector<std::string> fields = SplitFields(line);
            if (fields.size() == 2 && fields[0] == "skip" && !fields[1].empty())
            {
                rules.Skip.push_back(fields[1]);
                continue;
            }
            if (fields.size() == 3 && fields[0] == "type" && !fields[1].empty())
            {
                try
                {
                    rules.Types.emplace_back(fields[1], static_cast<uint32>(std::stoul(fields[2])));
                }
                catch (std::exception const&)
                {
                    error = fmt::format("rules file {} line {} has an invalid type", ConfigMgr::PathToUtf8(*path), lineNumber);
                    return {};
                }
                continue;
            }
            if (fields.size() != 4)
            {
                error = fmt::format("rules file {} line {} needs src|tar|package|type, skip|pattern or type|pattern|type", ConfigMgr::PathToUtf8(*path), lineNumber);
                return {};
            }
            try
            {
                Rule rule{ fields[0], fields[1], fields[2], static_cast<uint32>(std::stoul(fields[3])) };
                rules.ByDisk[DiskName(rule.Src, rule.Tar, rule.Package)] = std::move(rule);
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
        if (!document.is_object() || document.value("version", 0) != 2 || !document.contains("files") || !document["files"].is_object())
            return {};
        std::map<std::string, CachedFile> result;
        for (auto const& [name, value] : document["files"].items())
        {
            if (!value.is_object())
                continue;
            result[name] = {
                value.value("size", uintmax_t{ 0 }),
                value.value("time", intmax_t{ 0 }),
                value.value("crc", uint32{ 0 }),
                value.value("wad_header_size", uint32{ 0 }),
                value.value("wad_header_crc", uint32{ 0 }),
                value.value("compressed", uint32{ 0 })
            };
        }
        return result;
    }

    bool WriteCache(std::filesystem::path const& path, std::map<std::string, CachedFile> const& cache, std::string& error)
    {
        json files = json::object();
        for (auto const& [name, value] : cache)
            files[name] = {
                { "size", value.Size },
                { "time", value.Time },
                { "crc", value.CRC },
                { "wad_header_size", value.WadHeaderSize },
                { "wad_header_crc", value.WadHeaderCRC },
                { "compressed", value.Compressed }
            };
        json const document = { { "version", 2 }, { "files", std::move(files) } };
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

    bool IsWad(std::string_view name)
    {
        return name.size() >= 4 && (name.ends_with(".wad") || name.ends_with(".WAD"));
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
        CachedFile result{ size, time, Crc32::ComputeClient(bytes), 0, 0, 0 };
        if (IsWad(name))
        {
            std::optional<uint64> toc = KiwadHeader::MeasureTocLength(bytes);
            if (!toc || *toc > bytes.size() || *toc > UINT32_MAX)
            {
                error = fmt::format("{} is not a valid KIWAD archive", ConfigMgr::PathToUtf8(path));
                return std::nullopt;
            }
            result.WadHeaderSize = static_cast<uint32>(*toc);
            result.WadHeaderCRC = Crc32::ComputeClient(std::span<uint8 const>(bytes.data(), *toc));
        }
        else
            result.Compressed = static_cast<uint32>(Ambrose::Compression::Deflate(bytes).size() + CompressedFrame);
        cache[name] = result;
        return result;
    }

    std::string PackageName(std::string const& name)
    {
        if (name == "Data/GameData/Root.wad")
            return "Base";
        if (name.starts_with("Data/GameData/") && IsWad(name) && name.find('/', 14) == std::string::npos)
            return std::filesystem::path(name).stem().string();
        if (name.starts_with("PatchClient/"))
            return "PatchClient";
        return "Base";
    }

    bool IsOwnManifest(std::string const& name)
    {
        std::string const file = std::filesystem::path(name).filename().string();
        return file == "LatestFileList.bin" || file == "LatestFileList.xml";
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

    std::optional<std::string> LauncherBank(std::filesystem::path const& client)
    {
        for (char const* bank : { "BankB", "BankA" })
            if (std::filesystem::is_directory(client / "PatchClient" / bank))
                return std::string(bank);
        return std::nullopt;
    }

    std::optional<std::string> DiskNameOf(std::string const& relative, std::optional<std::string> const& bank)
    {
        if (!bank || !relative.starts_with("PatchClient/"))
            return relative;
        std::string const prefix = "PatchClient/" + *bank + "/";
        if (!relative.starts_with(prefix))
            return std::nullopt;
        return "PatchClient/" + relative.substr(prefix.size());
    }

    std::filesystem::path PathOf(std::filesystem::path const& client, std::string const& disk, std::optional<std::string> const& bank)
    {
        if (bank && disk.starts_with("PatchClient/"))
            return client / "PatchClient" / *bank / ConfigMgr::PathFromUtf8(disk.substr(12));
        return client / ConfigMgr::PathFromUtf8(disk);
    }

    void FillMetrics(LatestFileList::FileRecord& record, CachedFile const& file)
    {
        record.Size = static_cast<uint32>(file.Size);
        record.CRC = file.CRC;
        record.HeaderSize = 0;
        record.HeaderCRC = 0;
        if (record.FileType == 3 || record.FileType == 5)
        {
            record.HeaderSize = file.WadHeaderSize;
            record.HeaderCRC = file.WadHeaderCRC;
        }
        else if (record.FileType == 4)
            record.HeaderSize = file.Compressed;
    }

    LatestFileList::FileRecord MakeRecord(std::string const& disk, std::string const& package, CachedFile const& file, Rule const* rule, uint32 typed, bool sidecar)
    {
        LatestFileList::FileRecord record;
        if (rule)
        {
            record.SrcFileName = rule->Src;
            record.TarFileName = rule->Tar;
        }
        else
        {
            record.SrcFileName = disk;
            record.TarFileName = package == "PatchClient" ? disk.substr(12) : std::string();
        }
        if (rule && rule->Type != 0)
            record.FileType = rule->Type;
        else if (typed != 0)
            record.FileType = typed;
        else if (IsWad(disk))
            record.FileType = sidecar ? 5u : 3u;
        else
            record.FileType = 1u;
        FillMetrics(record, file);
        return record;
    }

    std::optional<LatestFileList> ReadReference(std::filesystem::path const& path, std::string& error)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            error = fmt::format("cannot open reference {}", ConfigMgr::PathToUtf8(path));
            return std::nullopt;
        }
        std::vector<uint8> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        try
        {
            if (Lower(path.extension().string()) == ".bin")
                return LatestFileList::ReadBinary(bytes);
            return LatestFileListXml::Read(std::string_view(reinterpret_cast<char const*>(bytes.data()), bytes.size()));
        }
        catch (std::exception const& exception)
        {
            error = fmt::format("cannot read reference {}: {}", ConfigMgr::PathToUtf8(path), exception.what());
            return std::nullopt;
        }
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

    Rules const rules = ReadRules(options.Rules, error);
    if (!error.empty())
        return std::nullopt;
    std::optional<LatestFileList> reference;
    if (options.Reference)
    {
        reference = ReadReference(*options.Reference, error);
        if (!reference)
            return std::nullopt;
    }
    std::filesystem::path const revisionDirectory = options.Output / ("V_" + revision);
    std::error_code directoryError;
    std::filesystem::create_directories(revisionDirectory, directoryError);
    if (directoryError)
    {
        error = fmt::format("cannot create {}", ConfigMgr::PathToUtf8(revisionDirectory));
        return std::nullopt;
    }

    std::map<std::string, CachedFile> cache = ReadCache(options.Output / ".patchlist-cache.json");
    std::optional<std::string> const bank = LauncherBank(options.Client);
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
        std::string const relative = RelativeName(it->path(), options.Client);
        std::optional<std::string> const disk = DiskNameOf(relative, bank);
        if (!disk || IsOwnManifest(*disk) || Lower(*disk).ends_with(".wad.utd"))
            continue;
        if (std::any_of(rules.Skip.begin(), rules.Skip.end(), [&](std::string const& pattern) { return GlobMatch(pattern, *disk); }))
            continue;
        auto const rule = rules.ByDisk.find(*disk);
        Rule const* const matched = rule == rules.ByDisk.end() ? nullptr : &rule->second;
        std::string const package = matched && !matched->Package.empty() ? matched->Package : PackageName(*disk);
        std::optional<CachedFile> file = ScanFile(it->path(), *disk, cache, result.CacheHits, error);
        if (!file)
            return std::nullopt;
        std::filesystem::path sidecar = it->path();
        sidecar += ".utd";
        auto const typeRule = std::find_if(rules.Types.begin(), rules.Types.end(), [&](auto const& entry) { return GlobMatch(entry.first, *disk); });
        uint32 const typed = typeRule == rules.Types.end() ? 0 : typeRule->second;
        std::error_code sidecarError;
        bool const hasSidecar = std::filesystem::is_regular_file(sidecar, sidecarError);
        if (hasSidecar && std::filesystem::file_size(sidecar, sidecarError) > 0 && !sidecarError)
            result.StillStreaming.push_back(*disk);
        packages[package].push_back(MakeRecord(*disk, package, *file, matched, typed, hasSidecar));
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
    }
    std::sort(result.Manifest.Packages.begin(), result.Manifest.Packages.end(), [](auto const& left, auto const& right) { return left.Name < right.Name; });
    for (auto const& package : result.Manifest.Packages)
        result.Manifest.TableOrder.push_back(package.Name);
    result.Manifest.TableOrder.push_back("About");

    if (!result.StillStreaming.empty())
        std::cout << fmt::format("{} type 5 WADs still have segments pending in their .utd sidecar, so their CRC is of the older bytes on disk, not of the finished file a list names\n",
            result.StillStreaming.size());
    if (reference)
    {
        std::filesystem::path const client = options.Client;
        std::cout << Diff(result.Manifest, *reference, [&](std::string const& package, LatestFileList::FileRecord const& record) {
            return std::filesystem::is_regular_file(PathOf(client, DiskName(record.SrcFileName, record.TarFileName, package), bank));
        });
        std::map<std::string, LatestFileList::FileRecord const*> known;
        for (auto const& package : reference->Packages)
            for (auto const& record : package.Records)
                known[package.Name + "\n" + record.SrcFileName] = &record;
        for (auto& package : result.Manifest.Packages)
            for (auto& record : package.Records)
            {
                auto const found = known.find(package.Name + "\n" + record.SrcFileName);
                if (found == known.end())
                    continue;
                record.CompressedHeaderSize = found->second->CompressedHeaderSize;
            }
    }

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
    return result;
}

std::string PatchListGenerator::Diff(LatestFileList const& generated, LatestFileList const& reference, OnDisk const& onDisk)
{
    std::ostringstream output;
    std::map<std::string, std::pair<std::string, LatestFileList::FileRecord const*>> actual;
    std::map<std::string, std::pair<std::string, LatestFileList::FileRecord const*>> expected;
    for (auto const& package : generated.Packages)
        for (auto const& record : package.Records)
            actual[package.Name + "\n" + record.SrcFileName] = { package.Name, &record };
    for (auto const& package : reference.Packages)
        for (auto const& record : package.Records)
            expected[package.Name + "\n" + record.SrcFileName] = { package.Name, &record };

    std::size_t differences = 0;
    std::size_t absent = 0;
    std::size_t measured = 0;
    std::size_t measuredMatch = 0;
    std::map<std::string, bool> tables;
    for (auto const& package : reference.Packages)
        tables[package.Name] = true;
    for (auto const& package : generated.Packages)
        tables.try_emplace(package.Name, true);
    for (auto const& [key, entry] : expected)
    {
        auto const& [package, record] = entry;
        auto const found = actual.find(key);
        if (found == actual.end())
        {
            if (onDisk && !onDisk(package, *record))
            {
                ++absent;
                continue;
            }
            output << "missing " << package << ' ' << record->SrcFileName << '\n';
            tables[package] = false;
            ++differences;
            continue;
        }
        LatestFileList::FileRecord const& made = *found->second.second;
        std::vector<std::string> fields;
        if (made.TarFileName != record->TarFileName)
            fields.push_back(fmt::format("Tar '{}' vs '{}'", made.TarFileName, record->TarFileName));
        if (made.FileType != record->FileType)
            fields.push_back(fmt::format("FileType {} vs {}", made.FileType, record->FileType));
        if (made.Size != record->Size)
            fields.push_back(fmt::format("Size {} vs {}", made.Size, record->Size));
        if (made.CRC != record->CRC)
            fields.push_back(fmt::format("CRC {} vs {}", made.CRC, record->CRC));
        if (made.HeaderSize != record->HeaderSize)
            fields.push_back(fmt::format("HeaderSize {} vs {}", made.HeaderSize, record->HeaderSize));
        if (made.HeaderCRC != record->HeaderCRC)
            fields.push_back(fmt::format("HeaderCRC {} vs {}", made.HeaderCRC, record->HeaderCRC));
        if (record->FileType == 3 || record->FileType == 5)
        {
            ++measured;
            if (made.Size == record->Size && made.CRC == record->CRC && made.HeaderSize == record->HeaderSize && made.HeaderCRC == record->HeaderCRC)
                ++measuredMatch;
        }
        if (!fields.empty())
        {
            output << "different " << package << ' ' << record->SrcFileName << " (ours vs reference): ";
            for (std::size_t index = 0; index < fields.size(); ++index)
                output << (index ? ", " : "") << fields[index];
            output << '\n';
            ++differences;
        }
    }
    for (auto const& [key, entry] : actual)
    {
        if (!expected.contains(key))
        {
            output << "unexpected " << entry.first << ' ' << entry.second->SrcFileName << '\n';
            tables[entry.first] = false;
            ++differences;
        }
    }
    std::size_t const matchedTables = static_cast<std::size_t>(std::count_if(tables.begin(), tables.end(), [](auto const& table) { return table.second; }));
    output << fmt::format("{} reference records are not in the install and were not compared\n", absent);
    output << fmt::format("type 3 and 5 records in both: {} of {} match Size, CRC, HeaderSize and HeaderCRC\n", measuredMatch, measured);
    output << fmt::format("package membership: {} of {} tables match\n", matchedTables, tables.size());
    output << differences << " differences\n";
    return output.str();
}
