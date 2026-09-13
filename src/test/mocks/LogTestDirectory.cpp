/*
 * Project Ambrose by Imjustchico
 * Creates and removes a unique temporary folder and reads files written by logging tests.
 */

#include "LogTestDirectory.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <random>
#include <system_error>

LogTestDirectory::LogTestDirectory()
{
    std::random_device device;
    _path = std::filesystem::path(testing::TempDir()) / ("ambrose-log-" + std::to_string(device()) + "-" + std::to_string(device()));
    std::filesystem::create_directories(_path);
}

LogTestDirectory::~LogTestDirectory()
{
    std::error_code error;
    std::filesystem::remove_all(_path, error);
}

std::filesystem::path const& LogTestDirectory::Path() const
{
    return _path;
}

std::filesystem::path LogTestDirectory::Write(std::string const& name, std::string const& content) const
{
    std::filesystem::path const file = _path / name;
    std::filesystem::create_directories(file.parent_path());
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream << content;
    return file;
}

std::vector<std::string> LogTestDirectory::ReadLines(std::filesystem::path const& file) const
{
    std::string const bytes = ReadBytes(file);
    std::vector<std::string> lines;
    std::size_t position = 0;
    while (position < bytes.size())
    {
        std::size_t const end = bytes.find('\n', position);
        if (end == std::string::npos)
        {
            lines.push_back(bytes.substr(position));
            break;
        }
        lines.push_back(bytes.substr(position, end - position));
        position = end + 1;
    }
    return lines;
}

std::string LogTestDirectory::ReadBytes(std::filesystem::path const& file) const
{
    std::ifstream stream(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::vector<std::filesystem::path> LogTestDirectory::ListFiles(std::filesystem::path const& folder) const
{
    std::vector<std::filesystem::path> files;
    std::error_code error;
    for (std::filesystem::directory_iterator it(folder, error), end; !error && it != end; it.increment(error))
        if (it->is_regular_file())
            files.push_back(it->path());
    std::sort(files.begin(), files.end());
    return files;
}
