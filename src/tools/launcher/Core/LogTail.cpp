/*
 * Project Ambrose by Imjustchico
 * Implements following the client's log: the file is opened when a poll first finds it, each poll reads to the end and clears the end-of-file state so the next poll sees what was appended since, and lines are split on the newline the client writes.
 */

#include "LogTail.h"

#include "StringUtil.h"

#include <array>
#include <utility>

LogTail::LogTail(std::filesystem::path file, std::function<void(std::string_view line)> onLine) : _file(std::move(file)), _onLine(std::move(onLine))
{
}

void LogTail::Poll()
{
    if (!_stream.is_open())
    {
        _stream.open(_file, std::ios::binary);
        if (!_stream.is_open())
            return;
    }
    std::array<char, ReadBytes> buffer{};
    while (true)
    {
        _stream.clear();
        _stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        std::streamsize const read = _stream.gcount();
        if (read <= 0)
            break;
        _pending.append(buffer.data(), static_cast<std::size_t>(read));
        std::size_t begin = 0;
        for (std::size_t newline = _pending.find('\n'); newline != std::string::npos; newline = _pending.find('\n', begin))
        {
            Emit(std::string_view(_pending).substr(begin, newline - begin));
            begin = newline + 1;
        }
        _pending.erase(0, begin);
        while (_pending.size() > MaxLineBytes)
        {
            std::string_view piece = Ambrose::TruncateUtf8(_pending, MaxLineBytes);
            if (piece.empty())
                piece = std::string_view(_pending).substr(0, MaxLineBytes);
            Emit(piece);
            _pending.erase(0, piece.size());
        }
    }
    _stream.clear();
}

void LogTail::Finish()
{
    Poll();
    if (_pending.empty())
        return;
    std::string const rest = std::move(_pending);
    _pending.clear();
    Emit(rest);
}

void LogTail::Emit(std::string_view line)
{
    if (!line.empty() && line.back() == '\r')
        line.remove_suffix(1);
    if (!_onLine)
        return;
    do
    {
        std::string_view piece = Ambrose::TruncateUtf8(line, MaxLineBytes);
        if (piece.empty() && !line.empty())
            piece = line.substr(0, MaxLineBytes);
        _onLine(piece);
        line.remove_prefix(piece.size());
    } while (!line.empty());
}
