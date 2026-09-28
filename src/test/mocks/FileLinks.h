/*
 * Project Ambrose by Imjustchico
 * Test helpers that make what a file jail has to refuse: a folder link, which is a symbolic link on Linux and a junction written with FSCTL_SET_REPARSE_POINT on Windows so it needs no privilege, a symbolic link to a file where this user may make one, a named pipe and a unix socket file left bound in place, each saying why when this system cannot make it.
 */

#ifndef AMBROSE_FILELINKS_H
#define AMBROSE_FILELINKS_H

#include <filesystem>
#include <string>

namespace FileLinks
{
    bool MakeFolderLink(std::filesystem::path const& link, std::filesystem::path const& target, std::string& why);
    bool MakeFileLink(std::filesystem::path const& link, std::filesystem::path const& target, std::string& why);
    bool MakeFifo(std::filesystem::path const& path, std::string& why);
    bool MakeSocket(std::filesystem::path const& path, std::string& why);
}

#endif
