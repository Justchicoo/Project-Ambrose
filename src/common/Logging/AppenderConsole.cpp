/*
 * Project Ambrose by Imjustchico
 * Parses console color lists in their six-code and named forms and writes each record with every part in the color its role asks for.
 */

#include "AppenderConsole.h"
#include "ConsoleWriter.h"
#include "LogConfig.h"
#include "StringUtil.h"

#include <chrono>
#include <string>

namespace
{
    ConsoleColor* RoleSlot(ConsoleRoles& roles, std::string_view name)
    {
        if (name == "body")
            return &roles.Body;
        if (name == "value")
            return &roles.Value;
        if (name == "category")
            return &roles.Category;
        if (name == "timestamp")
            return &roles.Timestamp;
        if (name == "mark")
            return &roles.Mark;
        if (name == "punctuation")
            return &roles.Punctuation;
        return nullptr;
    }

    std::optional<LogLevel> LevelNamed(std::string_view name)
    {
        if (name == "fatal")
            return LogLevel::Fatal;
        if (name == "error")
            return LogLevel::Error;
        if (name == "warn")
            return LogLevel::Warn;
        if (name == "info")
            return LogLevel::Info;
        if (name == "debug")
            return LogLevel::Debug;
        if (name == "trace")
            return LogLevel::Trace;
        return std::nullopt;
    }
}

AppenderConsole::AppenderConsole(AppenderDefinition const& definition, ColorSettings colors, ConsoleWriter& console, bool utc)
    : AppenderConsole(definition, colors, console, utc, LogLayout{}, true)
{
}

AppenderConsole::AppenderConsole(AppenderDefinition const& definition, ColorSettings colors, ConsoleWriter& console, bool utc, LogLayout layout, bool repeatCategory)
    : Appender(definition.Name, definition.Type, definition.Level, definition.Flags), _colors(colors), _console(console), _utc(utc), _layout(layout), _repeatCategory(repeatCategory)
{
}

AppenderTypeInfo AppenderConsole::GetTypeInfo()
{
    AppenderTypeInfo info;
    info.Type = AppenderType::Console;
    info.Name = "Console";
    info.Create = [](AppenderDefinition const& definition, AppenderCreateContext const& context, LogConfigResult& result) -> std::shared_ptr<Appender>
    {
        if (!ValidateFields(definition, result))
            return nullptr;
        ColorSettings colors{ DefaultColors, ConsoleRoles{} };
        if (!definition.Fields.empty() && !Ambrose::Trim(definition.Fields[0]).empty())
            colors = *ParseColorField(definition.Fields[0]);
        return std::make_shared<AppenderConsole>(definition, colors, context.Console, context.Utc, context.ConsoleLayout, context.RepeatCategory);
    };
    return info;
}

std::optional<AppenderConsole::ColorTable> AppenderConsole::ParseColors(std::string_view text)
{
    std::optional<ColorSettings> const settings = ParseColorField(text);
    if (!settings)
        return std::nullopt;
    return settings->Levels;
}

std::optional<AppenderConsole::ColorSettings> AppenderConsole::ParseColorField(std::string_view text)
{
    std::vector<std::string_view> tokens;
    for (std::string_view const token : Ambrose::Tokenize(text, ' ', false))
        if (!Ambrose::Trim(token).empty())
            tokens.push_back(Ambrose::Trim(token));

    ColorSettings settings{ DefaultColors, ConsoleRoles{} };
    if (tokens.empty())
        return std::nullopt;

    bool const named = tokens[0].find('=') != std::string_view::npos;
    if (!named)
    {
        if (tokens.size() != 6)
            return std::nullopt;
        for (std::size_t i = 0; i < tokens.size(); ++i)
        {
            std::optional<ConsoleColor> const color = Ambrose::Logging::ParseConsoleColor(tokens[i]);
            if (!color)
                return std::nullopt;
            settings.Levels[static_cast<std::size_t>(LogLevel::Fatal) - i] = *color;
        }
        return settings;
    }

    for (std::string_view const token : tokens)
    {
        std::size_t const split = token.find('=');
        if (split == std::string_view::npos || split == 0 || split + 1 >= token.size())
            return std::nullopt;
        std::string const name = Ambrose::ToLower(std::string(token.substr(0, split)));
        std::optional<ConsoleColor> const color = Ambrose::Logging::ParseConsoleColor(token.substr(split + 1));
        if (!color)
            return std::nullopt;
        if (std::optional<LogLevel> const level = LevelNamed(name))
            settings.Levels[static_cast<std::size_t>(*level)] = *color;
        else if (ConsoleColor* const slot = RoleSlot(settings.Roles, name))
            *slot = *color;
        else
            return std::nullopt;
    }
    return settings;
}

bool AppenderConsole::ValidateFields(AppenderDefinition const& definition, LogConfigResult& result)
{
    if (definition.Fields.size() > MaxFields)
    {
        result.Errors.push_back(definition.MakeIssue("too many fields; Console takes Type,Level,Flags[,Colors]"));
        return false;
    }
    if (!definition.Fields.empty() && !Ambrose::Trim(definition.Fields[0]).empty() && !ParseColorField(definition.Fields[0]))
    {
        result.Errors.push_back(definition.MakeIssue("colors must be six codes 0-15 in the order fatal, error, warn, info, debug, trace, or named pairs such as 'warn=14 body=15 category=8', where a name is a level or one of body, value, category, timestamp, mark and punctuation"));
        return false;
    }
    return true;
}

AppenderConsole::ColorTable const& AppenderConsole::GetColors() const noexcept
{
    return _colors.Levels;
}

ConsoleRoles const& AppenderConsole::GetRoles() const noexcept
{
    return _colors.Roles;
}

LogLayout const& AppenderConsole::GetLayout() const noexcept
{
    return _layout;
}

void AppenderConsole::Flush()
{
    _console.Flush();
}

ConsoleColor AppenderConsole::ColorFor(LogPart part, LogLevel level, bool newSecond) const noexcept
{
    std::size_t const index = static_cast<std::size_t>(level);
    ConsoleColor const levelColor = index < _colors.Levels.size() ? _colors.Levels[index] : ConsoleColor::Default;
    bool const dim = level == LogLevel::Debug || level == LogLevel::Trace;
    switch (part)
    {
        case LogPart::Timestamp:
            return newSecond ? _colors.Roles.Mark : _colors.Roles.Timestamp;
        case LogPart::Level:
            return levelColor;
        case LogPart::Thread:
        case LogPart::Punctuation:
            return _colors.Roles.Punctuation;
        case LogPart::Category:
            return _colors.Roles.Category;
        case LogPart::Value:
            return dim ? levelColor : _colors.Roles.Value;
        case LogPart::Padding:
            return ConsoleColor::Default;
        case LogPart::Body:
        default:
            return dim ? levelColor : _colors.Roles.Body;
    }
}

void AppenderConsole::WriteMessage(LogMessage const& message)
{
    bool const terminal = _console.IsTerminal();
    LogLayout layout;
    bool newSecond = false;
    if (terminal)
    {
        layout = _layout;
        layout.SuppressCategory = !_repeatCategory && _seenLine && message.Category == _lastCategory;
        int64 const second = std::chrono::duration_cast<std::chrono::seconds>(message.Time.time_since_epoch()).count();
        newSecond = !_seenLine || second != _lastSecond;
        _lastSecond = second;
        _lastCategory = message.Category;
        _seenLine = true;
    }

    std::string lines;
    lines.reserve(message.Text.size() + 64);
    std::vector<LogSpan> spans;
    message.AppendLines(lines, GetFlags(), _utc, &spans, layout);

    std::vector<ConsoleSegment> segments;
    segments.reserve(spans.size());
    for (LogSpan const& span : spans)
    {
        ConsoleColor const color = terminal ? ColorFor(span.Part, message.Level, newSecond)
            : (span.Part == LogPart::Timestamp || span.Part == LogPart::Thread || span.Part == LogPart::Category ? PrefixColor
                                                                                                                 : ColorFor(LogPart::Level, message.Level, false));
        std::string_view const text(lines.data() + span.Offset, span.Length);
        if (!segments.empty() && segments.back().Color == color && segments.back().Text.data() + segments.back().Text.size() == text.data())
            segments.back().Text = std::string_view(segments.back().Text.data(), segments.back().Text.size() + text.size());
        else
            segments.push_back({ text, color });
    }
    _console.WriteLines(segments);
}
