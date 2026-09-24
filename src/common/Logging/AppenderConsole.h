/*
 * Project Ambrose by Imjustchico
 * Console appender type 1 that colors a record by part rather than by level, laying a terminal line out in fixed columns while redirected output keeps its plain form.
 */

#ifndef AMBROSE_APPENDERCONSOLE_H
#define AMBROSE_APPENDERCONSOLE_H

#include "AppenderRegistry.h"
#include "LogMessage.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

struct ConsoleRoles
{
    ConsoleColor Body = ConsoleColor::Default;
    ConsoleColor Value = ConsoleColor::LightBlue;
    ConsoleColor Category = ConsoleColor::Grey;
    ConsoleColor Timestamp = ConsoleColor::Grey;
    ConsoleColor Mark = ConsoleColor::White;
    ConsoleColor Punctuation = ConsoleColor::Grey;
};

class AppenderConsole : public Appender
{
public:
    using ColorTable = std::array<ConsoleColor, 7>;

    struct ColorSettings
    {
        ColorTable Levels;
        ConsoleRoles Roles;
    };

    static constexpr ColorTable DefaultColors{ ConsoleColor::Default, ConsoleColor::Grey, ConsoleColor::Grey, ConsoleColor::LightCyan, ConsoleColor::Brown, ConsoleColor::LightRed, ConsoleColor::Red };
    static constexpr ConsoleColor PrefixColor = ConsoleColor::Grey;
    static constexpr std::size_t MaxFields = 1;

    AppenderConsole(AppenderDefinition const& definition, ColorSettings colors, ConsoleWriter& console, bool utc);
    AppenderConsole(AppenderDefinition const& definition, ColorSettings colors, ConsoleWriter& console, bool utc, LogLayout layout, bool repeatCategory);

    static AppenderTypeInfo GetTypeInfo();
    static std::optional<ColorTable> ParseColors(std::string_view text);
    static std::optional<ColorSettings> ParseColorField(std::string_view text);
    static bool ValidateFields(AppenderDefinition const& definition, LogConfigResult& result);

    ColorTable const& GetColors() const noexcept;
    ConsoleRoles const& GetRoles() const noexcept;
    LogLayout const& GetLayout() const noexcept;
    void Flush() override;

protected:
    void WriteMessage(LogMessage const& message) override;

private:
    ConsoleColor ColorFor(LogPart part, LogLevel level, bool newSecond) const noexcept;

    ColorSettings _colors;
    ConsoleWriter& _console;
    bool _utc;
    LogLayout _layout;
    bool _repeatCategory = true;
    std::string _lastCategory;
    int64 _lastSecond = 0;
    bool _seenLine = false;
};

#endif
