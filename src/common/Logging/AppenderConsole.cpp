/*
 * Project Ambrose by Imjustchico
 * Parses console color lists and writes each record as colored prefixed lines.
 */

#include "AppenderConsole.h"
#include "ConsoleWriter.h"
#include "LogConfig.h"
#include "StringUtil.h"

#include <string>

AppenderConsole::AppenderConsole(AppenderDefinition const& definition, ColorTable colors, ConsoleWriter& console, bool utc)
    : Appender(definition.Name, definition.Type, definition.Level, definition.Flags), _colors(colors), _console(console), _utc(utc)
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
        ColorTable colors = DefaultColors;
        if (!definition.Fields.empty() && !Ambrose::Trim(definition.Fields[0]).empty())
            colors = *ParseColors(definition.Fields[0]);
        return std::make_shared<AppenderConsole>(definition, colors, context.Console, context.Utc);
    };
    return info;
}

std::optional<AppenderConsole::ColorTable> AppenderConsole::ParseColors(std::string_view text)
{
    std::vector<std::string_view> tokens;
    for (std::string_view const token : Ambrose::Tokenize(text, ' ', false))
        if (!Ambrose::Trim(token).empty())
            tokens.push_back(token);
    if (tokens.size() != 6)
        return std::nullopt;
    ColorTable colors = DefaultColors;
    for (std::size_t i = 0; i < tokens.size(); ++i)
    {
        std::optional<ConsoleColor> const color = Ambrose::Logging::ParseConsoleColor(tokens[i]);
        if (!color)
            return std::nullopt;
        colors[static_cast<std::size_t>(LogLevel::Fatal) - i] = *color;
    }
    return colors;
}

bool AppenderConsole::ValidateFields(AppenderDefinition const& definition, LogConfigResult& result)
{
    if (definition.Fields.size() > MaxFields)
    {
        result.Errors.push_back(definition.MakeIssue("too many fields; Console takes Type,Level,Flags[,Colors]"));
        return false;
    }
    if (!definition.Fields.empty() && !Ambrose::Trim(definition.Fields[0]).empty() && !ParseColors(definition.Fields[0]))
    {
        result.Errors.push_back(definition.MakeIssue("colors must be six codes 0-15 separated by spaces, in the order fatal, error, warn, info, debug, trace"));
        return false;
    }
    return true;
}

AppenderConsole::ColorTable const& AppenderConsole::GetColors() const noexcept
{
    return _colors;
}

void AppenderConsole::Flush()
{
    _console.Flush();
}

void AppenderConsole::WriteMessage(LogMessage const& message)
{
    std::string lines;
    lines.reserve(message.Text.size() + 64);
    message.AppendLines(lines, GetFlags(), _utc);
    std::size_t const index = static_cast<std::size_t>(message.Level);
    _console.WriteLines(lines, index < _colors.size() ? _colors[index] : ConsoleColor::Default);
}
