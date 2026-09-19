/*
 * Project Ambrose by Imjustchico
 * Compares archive, zone, and locale files between two client revisions.
 */

#include <cstdint>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

enum class FileCategory
{
    Archives,
    Zones,
    Locales
};

struct FileRecord
{
    std::uintmax_t size = 0;
    std::uint64_t hash = 0;
};

using FileMap = std::map<std::string, FileRecord>;

static std::string Lowercase(std::string value)
{
    for (char& character : value)
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    return value;
}

static std::string CategoryName(FileCategory category)
{
    switch (category)
    {
        case FileCategory::Archives:
            return "archives";
        case FileCategory::Zones:
            return "zones";
        case FileCategory::Locales:
            return "locales";
    }
    return "unknown";
}

static bool HasExtension(std::string const& path, std::string_view extension)
{
    return path.size() >= extension.size() && path.compare(path.size() - extension.size(), extension.size(), extension) == 0;
}

static std::optional<FileCategory> Classify(fs::path const& relativePath)
{
    std::string const normalized = Lowercase(relativePath.generic_string());
    std::string const extension = Lowercase(relativePath.extension().string());

    if (HasExtension(extension, ".wad") || HasExtension(extension, ".kiwad"))
        return FileCategory::Archives;
    if (normalized.find("/locale/") != std::string::npos || normalized.starts_with("locale/") || extension == ".lang")
        return FileCategory::Locales;
    if (normalized.find("/zone/") != std::string::npos || normalized.find("/zones/") != std::string::npos
        || normalized.starts_with("zone/") || normalized.starts_with("zones/") || extension == ".zone"
        || extension == ".nif" || extension == ".dds")
        return FileCategory::Zones;
    return std::nullopt;
}

static std::uint64_t HashFile(fs::path const& path)
{
    constexpr std::uint64_t offset = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t hash = offset;
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Could not open file: " + path.string());

    char buffer[8192];
    while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0)
    {
        for (std::streamsize index = 0; index < input.gcount(); ++index)
        {
            hash ^= static_cast<unsigned char>(buffer[index]);
            hash *= prime;
        }
    }
    return hash;
}

static std::map<FileCategory, FileMap> Scan(fs::path const& root)
{
    if (!fs::exists(root) || !fs::is_directory(root))
        throw std::runtime_error("Not a directory: " + root.string());

    std::map<FileCategory, FileMap> files;
    for (fs::recursive_directory_iterator iterator(root), end; iterator != end; ++iterator)
    {
        if (!iterator->is_regular_file())
            continue;

        fs::path const relativePath = fs::relative(iterator->path(), root);
        FileRecord record{iterator->file_size(), HashFile(iterator->path())};
        std::optional<FileCategory> const category = Classify(relativePath);
        if (category)
            files[*category][relativePath.generic_string()] = record;
    }
    return files;
}

static void PrintDifference(FileCategory category, FileMap const& oldFiles, FileMap const& newFiles)
{
    std::cout << '[' << CategoryName(category) << "]\n";
    for (auto const& [path, record] : newFiles)
    {
        auto const old = oldFiles.find(path);
        if (old == oldFiles.end())
        {
            std::cout << "added " << path << " " << record.size << '\n';
        }
        else if (old->second.size != record.size || old->second.hash != record.hash)
        {
            std::cout << "changed " << path << ' ' << old->second.size << " -> " << record.size << '\n';
        }
    }
    for (auto const& [path, record] : oldFiles)
    {
        if (!newFiles.contains(path))
            std::cout << "removed " << path << " " << record.size << '\n';
    }
}

static FileMap const& GetFiles(std::map<FileCategory, FileMap> const& files, FileCategory category)
{
    static FileMap const empty;
    auto const iterator = files.find(category);
    return iterator == files.end() ? empty : iterator->second;
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: ambrose-install-diff <old-install> <new-install>\n";
        return 2;
    }

    try
    {
        std::map<FileCategory, FileMap> const oldFiles = Scan(argv[1]);
        std::map<FileCategory, FileMap> const newFiles = Scan(argv[2]);
        for (FileCategory category : {FileCategory::Archives, FileCategory::Zones, FileCategory::Locales})
            PrintDifference(category, GetFiles(oldFiles, category), GetFiles(newFiles, category));
    }
    catch (fs::filesystem_error const& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    catch (std::runtime_error const& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
