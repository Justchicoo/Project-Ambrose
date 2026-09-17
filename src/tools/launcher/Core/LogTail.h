/*
 * Project Ambrose by Imjustchico
 * Follows the client's own log while it runs: each poll opens the file once it exists, reads whatever has been appended and hands every finished line to a callback without its carriage return, cutting a line no program should write past MaxLineBytes, and Finish hands over the last line even when the client wrote no newline after it.
 */

#ifndef AMBROSE_LOGTAIL_H
#define AMBROSE_LOGTAIL_H

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <string_view>

class LogTail
{
public:
    static constexpr std::size_t MaxLineBytes = 64 * 1024;
    static constexpr std::size_t ReadBytes = 64 * 1024;

    LogTail(std::filesystem::path file, std::function<void(std::string_view line)> onLine);

    LogTail(LogTail const&) = delete;
    LogTail& operator=(LogTail const&) = delete;

    void Poll();
    void Finish();

private:
    void Emit(std::string_view line);

    std::filesystem::path _file;
    std::function<void(std::string_view line)> _onLine;
    std::ifstream _stream;
    std::string _pending;
};

#endif
