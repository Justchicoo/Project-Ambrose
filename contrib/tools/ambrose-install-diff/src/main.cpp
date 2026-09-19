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
#include <vector>

namespace fs = std::filesystem;

enum class FileCategory
{
    Archives,
    Zones,
    Locales
};

using FileMap = std::map<std::string, std::uintmax_t>;

struct Installation
{
    fs::path root;
    std::map<FileCategory, FileMap> files;
    std::vector<std::string> unreadable;
};

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

static std::optional<std::uint64_t> HashFile(fs::path const& path)
{
    constexpr std::uint64_t offset = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t hash = offset;
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return std::nullopt;

    char buffer[8192];
    while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0)
    {
        for (std::streamsize index = 0; index < input.gcount(); ++index)
        {
            hash ^= static_cast<unsigned char>(buffer[index]);
            hash *= prime;
        }
    }
    if (input.bad())
        return std::nullopt;
    return hash;
}

static Installation Scan(fs::path const& root)
{
    if (!fs::exists(root) || !fs::is_directory(root))
        throw std::runtime_error("Not a directory: " + root.string());

    Installation installation;
    installation.root = root;

    std::error_code code;
    fs::recursive_directory_iterator iterator(root, fs::directory_options::skip_permission_denied, code);
    if (code)
        throw std::runtime_error("Could not read: " + root.string());

    fs::recursive_directory_iterator const end;
    while (iterator != end)
    {
        fs::path const path = iterator->path();
        bool const regular = iterator->is_regular_file(code);
        if (!code && regular)
        {
            fs::path const relativePath = fs::relative(path, root, code);
            std::optional<FileCategory> const category = code ? std::nullopt : Classify(relativePath);
            if (category)
            {
                std::uintmax_t const size = fs::file_size(path, code);
                if (code)
                    installation.unreadable.push_back(relativePath.generic_string());
                else
                    installation.files[*category][relativePath.generic_string()] = size;
            }
        }
        code.clear();

        iterator.increment(code);
        if (code)
        {
            installation.unreadable.push_back(path.generic_string());
            code.clear();
            break;
        }
    }
    return installation;
}

static FileMap const& GetFiles(std::map<FileCategory, FileMap> const& files, FileCategory category)
{
    static FileMap const empty;
    auto const iterator = files.find(category);
    return iterator == files.end() ? empty : iterator->second;
}

static void PrintDifference(FileCategory category, Installation const& oldInstall, Installation const& newInstall,
    std::vector<std::string>& unreadable)
{
    FileMap const& oldFiles = GetFiles(oldInstall.files, category);
    FileMap const& newFiles = GetFiles(newInstall.files, category);

    std::cout << '[' << CategoryName(category) << "]\n";
    for (auto const& [path, size] : newFiles)
    {
        auto const old = oldFiles.find(path);
        if (old == oldFiles.end())
        {
            std::cout << "added " << path << ' ' << size << '\n';
            continue;
        }
        if (old->second != size)
        {
            std::cout << "changed " << path << ' ' << old->second << " -> " << size << '\n';
            continue;
        }

        std::optional<std::uint64_t> const oldHash = HashFile(oldInstall.root / path);
        std::optional<std::uint64_t> const newHash = HashFile(newInstall.root / path);
        if (!oldHash || !newHash)
        {
            unreadable.push_back(path);
            continue;
        }
        if (*oldHash != *newHash)
            std::cout << "changed " << path << ' ' << size << " -> " << size << " same size, contents differ\n";
    }
    for (auto const& [path, size] : oldFiles)
    {
        if (!newFiles.contains(path))
            std::cout << "removed " << path << ' ' << size << '\n';
    }
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
        Installation const oldInstall = Scan(argv[1]);
        Installation const newInstall = Scan(argv[2]);

        std::vector<std::string> unreadable = oldInstall.unreadable;
        unreadable.insert(unreadable.end(), newInstall.unreadable.begin(), newInstall.unreadable.end());

        for (FileCategory category : {FileCategory::Archives, FileCategory::Zones, FileCategory::Locales})
            PrintDifference(category, oldInstall, newInstall, unreadable);

        if (!unreadable.empty())
        {
            std::cout << "[unread]\n";
            for (std::string const& path : unreadable)
                std::cout << "could not be read " << path << '\n';
            std::cerr << unreadable.size() << " file(s) could not be read; the report above is incomplete\n";
            return 3;
        }
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
