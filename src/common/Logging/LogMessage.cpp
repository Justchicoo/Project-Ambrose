/*
 * Project Ambrose by Imjustchico
 * Renders log records as prefixed text lines with escaped control characters and repaired UTF-8.
 */

#include "LogMessage.h"
#include "LogTimestamp.h"
#include "Utf.h"

#include <fmt/format.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace
{
    bool IsEscapedByte(unsigned char c) noexcept
    {
        return (c < 0x20 && c != '\t') || c == 0x7F;
    }

    bool NeedsSanitizing(std::string_view line) noexcept
    {
        for (char const c : line)
        {
            unsigned char const byte = static_cast<unsigned char>(c);
            if (IsEscapedByte(byte) || byte >= 0x80)
                return true;
        }
        return false;
    }

    void AppendEscaped(std::string& out, std::string_view line)
    {
        static constexpr char const Digits[] = "0123456789ABCDEF";
        for (std::size_t i = 0; i < line.size(); ++i)
        {
            unsigned char const byte = static_cast<unsigned char>(line[i]);
            if (byte == 0xC2 && i + 1 < line.size())
            {
                unsigned char const next = static_cast<unsigned char>(line[i + 1]);
                if (next >= 0x80 && next <= 0x9F)
                {
                    out.append("\\u00");
                    out.push_back(Digits[next >> 4]);
                    out.push_back(Digits[next & 0x0F]);
                    ++i;
                    continue;
                }
            }
            if (!IsEscapedByte(byte))
            {
                out.push_back(line[i]);
                continue;
            }
            out.append("\\x");
            out.push_back(Digits[byte >> 4]);
            out.push_back(Digits[byte & 0x0F]);
        }
    }
}

void LogMessage::AppendPrefix(std::string& out, AppenderFlags flags, bool utc) const
{
    if (HasAppenderFlag(flags, AppenderFlags::PrefixTimestamp))
    {
        out.append(LogTimestamp::FormatPrefix(Time, utc));
        out.push_back(' ');
    }
    if (HasAppenderFlag(flags, AppenderFlags::PrefixLevel))
    {
        out.append(Ambrose::Logging::GetLogLevelPaddedName(Level));
        out.push_back(' ');
    }
    if (HasAppenderFlag(flags, AppenderFlags::PrefixThread))
        fmt::format_to(std::back_inserter(out), "T{} ", ThreadId);
    if (HasAppenderFlag(flags, AppenderFlags::PrefixCategory))
    {
        out.push_back('[');
        AppendSanitized(out, Category);
        out.append("] ");
    }
}

void LogMessage::AppendLines(std::string& out, AppenderFlags flags, bool utc) const
{
    std::string_view text = Text;
    if (!text.empty() && text.back() == '\n')
        text.remove_suffix(1);
    if (!text.empty() && text.back() == '\r')
        text.remove_suffix(1);

    std::size_t position = 0;
    while (true)
    {
        std::size_t const end = text.find('\n', position);
        std::string_view line = text.substr(position, end == std::string_view::npos ? std::string_view::npos : end - position);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);

        std::size_t const lineStart = out.size();
        AppendPrefix(out, flags, utc);
        while (line.empty() && out.size() > lineStart && out.back() == ' ')
            out.pop_back();
        AppendSanitized(out, line);
        out.push_back('\n');

        if (end == std::string_view::npos)
            break;
        position = end + 1;
    }
}

void LogMessage::AppendSanitized(std::string& out, std::string_view line)
{
    if (!NeedsSanitizing(line))
    {
        out.append(line);
        return;
    }
    if (Utf::IsValidUtf8(line))
    {
        AppendEscaped(out, line);
        return;
    }
    std::optional<std::u16string> const utf16 = Utf::Utf8ToUtf16(line, Utf::InvalidPolicy::ReplaceWithU_FFFD);
    std::optional<std::string> const repaired = utf16 ? Utf::Utf16ToUtf8(*utf16, Utf::InvalidPolicy::ReplaceWithU_FFFD) : std::nullopt;
    AppendEscaped(out, repaired ? std::string_view(*repaired) : std::string_view());
}

uint64 LogMessage::CurrentOsThreadId() noexcept
{
    thread_local uint64 const id =
#ifdef _WIN32
        static_cast<uint64>(::GetCurrentThreadId());
#else
        static_cast<uint64>(::syscall(SYS_gettid));
#endif
    return id;
}
