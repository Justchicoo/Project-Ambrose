/*
 * Project Ambrose by Imjustchico
 * Console appender type 1 that renders per-level colored lines through ConsoleWriter, with timestamps, thread ids and categories in the quiet prefix color.
 */

#ifndef AMBROSE_APPENDERCONSOLE_H
#define AMBROSE_APPENDERCONSOLE_H

#include "AppenderRegistry.h"

#include <array>
#include <optional>
#include <string_view>

class AppenderConsole : public Appender
{
public:
    using ColorTable = std::array<ConsoleColor, 7>;

    static constexpr ColorTable DefaultColors{ ConsoleColor::Default, ConsoleColor::Grey, ConsoleColor::Grey, ConsoleColor::LightCyan, ConsoleColor::Brown, ConsoleColor::LightRed, ConsoleColor::Red };
    static constexpr ConsoleColor PrefixColor = ConsoleColor::Grey;
    static constexpr std::size_t MaxFields = 1;

    AppenderConsole(AppenderDefinition const& definition, ColorTable colors, ConsoleWriter& console, bool utc);

    static AppenderTypeInfo GetTypeInfo();
    static std::optional<ColorTable> ParseColors(std::string_view text);
    static bool ValidateFields(AppenderDefinition const& definition, LogConfigResult& result);

    ColorTable const& GetColors() const noexcept;
    void Flush() override;

protected:
    void WriteMessage(LogMessage const& message) override;

private:
    ColorTable _colors;
    ConsoleWriter& _console;
    bool _utc;
};

#endif
