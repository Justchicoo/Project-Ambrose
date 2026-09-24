/*
 * Project Ambrose by Imjustchico
 * Renders log records as prefixed text lines under a layout, with escaped control characters and repaired UTF-8, recording the byte range each part covers when a caller colors them.
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
    std::size_t AddSpan(std::vector<LogSpan>* spans, std::size_t start, std::size_t end, LogPart part)
    {
        if (spans && end > start)
            spans->push_back({ start, end - start, part });
        return end;
    }

    void ClampSpans(std::vector<LogSpan>& spans, std::size_t first, std::size_t limit)
    {
        while (spans.size() > first && spans.back().Offset >= limit)
            spans.pop_back();
        if (spans.size() > first && spans.back().Offset + spans.back().Length > limit)
            spans.back().Length = limit - spans.back().Offset;
    }

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

void LogMessage::AppendPrefix(std::string& out, AppenderFlags flags, bool utc, std::vector<LogSpan>* spans) const
{
    AppendPrefix(out, flags, utc, spans, LogLayout{});
}

void LogMessage::AppendCategoryColumn(std::string& out, std::string_view category, uint16 width, std::vector<LogSpan>* spans)
{
    std::size_t start = out.size();
    out.push_back('[');
    start = AddSpan(spans, start, out.size(), LogPart::Punctuation);

    std::string name;
    AppendSanitized(name, category);
    if (width > 0 && name.size() > width)
    {
        std::size_t const keep = static_cast<std::size_t>(width) - 2;
        std::size_t const left = (keep + 1) / 2;
        std::size_t const right = keep - left;
        name = name.substr(0, left) + ".." + name.substr(name.size() - right);
    }
    out.append(name);
    start = AddSpan(spans, start, out.size(), LogPart::Category);

    if (width > 0 && name.size() < width)
    {
        out.append(static_cast<std::size_t>(width) - name.size(), ' ');
        start = AddSpan(spans, start, out.size(), LogPart::Padding);
    }

    out.push_back(']');
    start = AddSpan(spans, start, out.size(), LogPart::Punctuation);
    out.push_back(' ');
    AddSpan(spans, start, out.size(), LogPart::Padding);
}

void LogMessage::AppendPrefix(std::string& out, AppenderFlags flags, bool utc, std::vector<LogSpan>* spans, LogLayout const& layout) const
{
    std::size_t start = out.size();
    if (HasAppenderFlag(flags, AppenderFlags::PrefixTimestamp) && layout.Timestamp != LogTimestampStyle::Off)
    {
        std::string_view const full = LogTimestamp::FormatPrefix(Time, utc);
        if (layout.Timestamp == LogTimestampStyle::Short)
        {
            std::size_t const split = full.find_first_of("_ ");
            out.append(split == std::string_view::npos ? full : full.substr(split + 1));
        }
        else
        {
            out.append(full);
        }
        out.push_back(' ');
        start = AddSpan(spans, start, out.size(), LogPart::Timestamp);
    }
    if (HasAppenderFlag(flags, AppenderFlags::PrefixLevel))
    {
        out.append(Ambrose::Logging::GetLogLevelPaddedName(Level));
        out.push_back(' ');
        start = AddSpan(spans, start, out.size(), LogPart::Level);
    }
    if (HasAppenderFlag(flags, AppenderFlags::PrefixThread))
    {
        fmt::format_to(std::back_inserter(out), "T{} ", ThreadId);
        start = AddSpan(spans, start, out.size(), LogPart::Thread);
    }
    if (HasAppenderFlag(flags, AppenderFlags::PrefixCategory))
    {
        if (layout.SuppressCategory && layout.CategoryWidth > 0)
        {
            out.append(static_cast<std::size_t>(layout.CategoryWidth) + 3, ' ');
            AddSpan(spans, start, out.size(), LogPart::Padding);
        }
        else
        {
            AppendCategoryColumn(out, Category, layout.CategoryWidth, spans);
        }
    }
}

void LogMessage::AppendLines(std::string& out, AppenderFlags flags, bool utc, std::vector<LogSpan>* spans) const
{
    AppendLines(out, flags, utc, spans, LogLayout{});
}

void LogMessage::AppendLines(std::string& out, AppenderFlags flags, bool utc, std::vector<LogSpan>* spans, LogLayout const& layout) const
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
        std::size_t const spanStart = spans ? spans->size() : 0;
        AppendPrefix(out, flags, utc, spans, layout);
        while (line.empty() && out.size() > lineStart && out.back() == ' ')
            out.pop_back();
        if (spans)
            ClampSpans(*spans, spanStart, out.size());
        std::size_t const textStart = out.size();
        AppendSanitized(out, line);
        out.push_back('\n');
        AddSpan(spans, textStart, out.size(), LogPart::Body);

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
