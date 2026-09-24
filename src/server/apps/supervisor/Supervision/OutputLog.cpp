/*
 * Project Ambrose by Imjustchico
 * Keeps current.out, current.err, previous.out and previous.err in the app's output folder: a new run moves the current files and ring to previous, a poll reads at most a mebibyte per stream from where it stopped, restarts from the top of a file something else shortened, splits lines at 64 KiB without cutting a character, drops terminal colour and control sequences, replaces invalid UTF-8 and stamps each line with the time it was read, and a read-back tail has no time because the file does not say when each line was written.
 */

#include "OutputLog.h"
#include "ConfigMgr.h"
#include "StringUtil.h"
#include "Utf.h"

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <system_error>

namespace
{
    int64 NowEpochMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::string ReadRange(std::filesystem::path const& file, uint64 offset, uint64 bytes)
    {
        std::ifstream stream(file, std::ios::binary);
        if (!stream)
            return std::string();
        stream.seekg(static_cast<std::streamoff>(offset));
        std::string buffer(static_cast<std::size_t>(bytes), '\0');
        stream.read(buffer.data(), static_cast<std::streamsize>(bytes));
        buffer.resize(static_cast<std::size_t>(std::max<std::streamsize>(stream.gcount(), 0)));
        return buffer;
    }
}

OutputLog::OutputLog(std::filesystem::path folder, uint64 maxFileBytes) : _folder(std::move(folder)), _maxFileBytes(std::max(maxFileBytes, MinMaxFileBytes))
{
    _streams[0].Name = "stdout";
    _streams[0].Current = _folder / "current.out";
    _streams[0].Previous = _folder / "previous.out";
    _streams[1].Name = "stderr";
    _streams[1].Current = _folder / "current.err";
    _streams[1].Previous = _folder / "previous.err";
}

std::filesystem::path OutputLog::OutputFile() const
{
    return _streams[0].Current;
}

std::filesystem::path OutputLog::ErrorFile() const
{
    return _streams[1].Current;
}

std::string OutputLog::Clean(std::string_view text)
{
    std::string cleaned;
    cleaned.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        unsigned char const c = static_cast<unsigned char>(text[index]);
        if (c == 0x1B)
        {
            if (index + 1 < text.size() && text[index + 1] == '[')
            {
                index += 2;
                while (index < text.size() && (static_cast<unsigned char>(text[index]) < 0x40 || static_cast<unsigned char>(text[index]) > 0x7E))
                    ++index;
                continue;
            }
            if (index + 1 < text.size() && text[index + 1] == ']')
            {
                index += 2;
                while (index < text.size() && text[index] != '\a' && !(text[index] == 0x1B && index + 1 < text.size() && text[index + 1] == '\\'))
                    ++index;
                if (index < text.size() && text[index] == 0x1B)
                    ++index;
                continue;
            }
            ++index;
            continue;
        }
        if (c == '\t')
        {
            cleaned.push_back(' ');
            continue;
        }
        if (c < 0x20 || c == 0x7F)
            continue;
        cleaned.push_back(static_cast<char>(c));
    }
    if (Utf::IsValidUtf8(cleaned))
        return cleaned;
    std::u16string const wide = Utf::Utf8ToUtf16(cleaned, Utf::InvalidPolicy::ReplaceWithU_FFFD).value_or(std::u16string());
    return Utf::Utf16ToUtf8(wide, Utf::InvalidPolicy::ReplaceWithU_FFFD).value_or(std::string());
}

void OutputLog::Push(std::deque<OutputLine>& ring, std::string const& stream, std::string_view text, int64 epochMs, std::vector<OutputLine>* collected)
{
    std::string const cleaned = Clean(text);
    std::string_view rest = cleaned;
    do
    {
        std::string_view piece = Ambrose::TruncateUtf8(rest, MaxLineBytes);
        if (piece.empty() && !rest.empty())
            piece = rest.substr(0, MaxLineBytes);
        OutputLine line{ ++_sequence, stream, std::string(piece), epochMs };
        if (collected)
            collected->push_back(line);
        ring.push_back(std::move(line));
        while (ring.size() > RingLines)
            ring.pop_front();
        rest.remove_prefix(piece.size());
    } while (!rest.empty());
}

std::vector<OutputLine> OutputLog::Read(Stream& stream, bool flush)
{
    std::vector<OutputLine> lines;
    std::error_code code;
    uint64 const size = std::filesystem::file_size(stream.Current, code);
    if (code)
        return lines;
    if (size < stream.Offset)
    {
        stream.Offset = 0;
        stream.Pending.clear();
    }
    uint64 const end = std::min(size, stream.Offset + MaxReadBytes);
    if (end > stream.Offset)
    {
        std::string const bytes = ReadRange(stream.Current, stream.Offset, end - stream.Offset);
        stream.Offset += bytes.size();
        stream.Pending += bytes;
    }
    int64 const now = NowEpochMs();
    std::lock_guard<std::mutex> const lock(_mutex);
    std::size_t begin = 0;
    while (true)
    {
        std::size_t const newline = stream.Pending.find('\n', begin);
        if (newline == std::string::npos)
            break;
        std::string_view line = std::string_view(stream.Pending).substr(begin, newline - begin);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        Push(_current, stream.Name, line, now, &lines);
        begin = newline + 1;
    }
    stream.Pending.erase(0, begin);
    if (stream.Pending.size() > MaxLineBytes || (flush && !stream.Pending.empty()))
    {
        Push(_current, stream.Name, stream.Pending, now, &lines);
        stream.Pending.clear();
    }
    if (size > _maxFileBytes && stream.Offset >= size)
    {
        std::filesystem::resize_file(stream.Current, 0, code);
        if (!code)
        {
            stream.Offset = 0;
            Push(_current, "supervisor", fmt::format("The supervisor emptied {} after it passed {} bytes; the lines above are kept here, and a line written while it was emptied may be missing", ConfigMgr::PathToUtf8(stream.Current.filename()), _maxFileBytes), now, &lines);
        }
    }
    return lines;
}

std::vector<OutputLine> OutputLog::Poll()
{
    std::vector<OutputLine> lines;
    for (Stream& stream : _streams)
    {
        std::vector<OutputLine> read = Read(stream, false);
        lines.insert(lines.end(), std::make_move_iterator(read.begin()), std::make_move_iterator(read.end()));
    }
    return lines;
}

bool OutputLog::BeginRun(std::string& error)
{
    for (Stream& stream : _streams)
        Read(stream, true);
    {
        std::lock_guard<std::mutex> const lock(_mutex);
        _previous = std::move(_current);
        _current.clear();
    }
    std::error_code code;
    std::filesystem::create_directories(_folder, code);
    if (code)
    {
        error = fmt::format("the output folder {} could not be made: {}", ConfigMgr::PathToUtf8(_folder), code.message());
        return false;
    }
    for (Stream& stream : _streams)
    {
        std::filesystem::remove(stream.Previous, code);
        if (std::filesystem::exists(stream.Current, code))
        {
            std::filesystem::rename(stream.Current, stream.Previous, code);
            if (code)
            {
                error = fmt::format("{} could not be kept as {}: {}", ConfigMgr::PathToUtf8(stream.Current), ConfigMgr::PathToUtf8(stream.Previous), code.message());
                return false;
            }
        }
        std::ofstream created(stream.Current, std::ios::binary | std::ios::trunc);
        if (!created)
        {
            error = fmt::format("{} could not be created", ConfigMgr::PathToUtf8(stream.Current));
            return false;
        }
        stream.Offset = 0;
        stream.Pending.clear();
    }
    return true;
}

std::vector<std::string> OutputLog::Tail(std::filesystem::path const& file, std::size_t count)
{
    std::vector<std::string> lines;
    std::error_code code;
    uint64 const size = std::filesystem::file_size(file, code);
    if (code || size == 0)
        return lines;
    uint64 const start = size > MaxTailBytes ? size - MaxTailBytes : 0;
    std::string const text = ReadRange(file, start, size - start);
    std::string_view view = text;
    if (start > 0)
    {
        std::size_t const first = view.find('\n');
        view = first == std::string_view::npos ? std::string_view() : view.substr(first + 1);
    }
    for (std::string_view const line : Ambrose::Tokenize(view, '\n', true))
        lines.emplace_back(line);
    if (!lines.empty() && lines.back().empty())
        lines.pop_back();
    for (std::string& line : lines)
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
    if (lines.size() > count)
        lines.erase(lines.begin(), lines.end() - static_cast<std::ptrdiff_t>(count));
    return lines;
}

void OutputLog::Attach()
{
    std::lock_guard<std::mutex> const lock(_mutex);
    _current.clear();
    _previous.clear();
    for (Stream& stream : _streams)
    {
        for (std::string const& line : Tail(stream.Previous, RingLines))
            Push(_previous, stream.Name, line, 0, nullptr);
        for (std::string const& line : Tail(stream.Current, RingLines))
            Push(_current, stream.Name, line, 0, nullptr);
        std::error_code code;
        uint64 const size = std::filesystem::file_size(stream.Current, code);
        stream.Offset = code ? 0 : size;
        stream.Pending.clear();
    }
}

void OutputLog::Note(std::string const& text)
{
    std::lock_guard<std::mutex> const lock(_mutex);
    Push(_current, "supervisor", text, NowEpochMs(), nullptr);
}

std::vector<OutputLine> OutputLog::Lines(OutputRun run, uint64 after) const
{
    std::lock_guard<std::mutex> const lock(_mutex);
    std::deque<OutputLine> const& ring = run == OutputRun::Current ? _current : _previous;
    std::vector<OutputLine> lines;
    for (OutputLine const& line : ring)
        if (line.Sequence > after)
            lines.push_back(line);
    return lines;
}
