/*
 * Project Ambrose by Imjustchico
 * Temporary folder for logging tests with file writing and line reading helpers.
 */

#ifndef AMBROSE_LOGTESTDIRECTORY_H
#define AMBROSE_LOGTESTDIRECTORY_H

#include <filesystem>
#include <string>
#include <vector>

class LogTestDirectory
{
public:
    LogTestDirectory();
    ~LogTestDirectory();

    LogTestDirectory(LogTestDirectory const&) = delete;
    LogTestDirectory& operator=(LogTestDirectory const&) = delete;

    std::filesystem::path const& Path() const;
    std::filesystem::path Write(std::string const& name, std::string const& content) const;
    std::vector<std::string> ReadLines(std::filesystem::path const& file) const;
    std::string ReadBytes(std::filesystem::path const& file) const;
    std::vector<std::filesystem::path> ListFiles(std::filesystem::path const& folder) const;

private:
    std::filesystem::path _path;
};

#endif
