/*
 * Project Ambrose by Imjustchico
 * Decides when to color console output and writes each line with ANSI sequences or legacy console attributes.
 */

#include "ConsoleWriter.h"
#include "Environment.h"

#include <array>
#include <string>
#include <utility>

ConsoleWriter::ConsoleWriter(std::unique_ptr<ConsoleDevice> device) : _device(std::move(device))
{
    std::optional<std::string> const noColor = Ambrose::GetEnv("NO_COLOR");
    _noColorEnvironment = noColor.has_value() && !noColor->empty();
    std::optional<std::string> const force = Ambrose::GetEnv("CLICOLOR_FORCE");
    _forceColorEnvironment = force.has_value() && *force == "1";
}

ConsoleWriter& ConsoleWriter::Instance()
{
    static ConsoleWriter* const instance = new ConsoleWriter(ConsoleDevice::CreateStandardOutput());
    return *instance;
}

void ConsoleWriter::SetColorMode(ConsoleColorMode mode)
{
    std::lock_guard lock(_mutex);
    _mode = mode;
}

ConsoleColorMode ConsoleWriter::GetColorMode() const
{
    std::lock_guard lock(_mutex);
    return _mode;
}

bool ConsoleWriter::UsesColor() const
{
    std::lock_guard lock(_mutex);
    return UsesColorLocked();
}

bool ConsoleWriter::UsesColorLocked() const
{
    if (_restored || !_device)
        return false;
    ConsoleColorMode mode = _mode;
    if (mode == ConsoleColorMode::Auto && _forceColorEnvironment)
        mode = ConsoleColorMode::Always;
    switch (mode)
    {
        case ConsoleColorMode::Never: return false;
        case ConsoleColorMode::Always: return true;
        case ConsoleColorMode::Auto: return _device->IsTerminal() && !_noColorEnvironment;
    }
    return false;
}

void ConsoleWriter::SetLineHooks(LineHook before, LineHook after)
{
    std::lock_guard lock(_mutex);
    _before = std::move(before);
    _after = std::move(after);
}

std::string_view ConsoleWriter::GetAnsiSequence(ConsoleColor color) noexcept
{
    static constexpr std::array<std::string_view, 15> Sequences{
        "\x1b[30m", "\x1b[31m", "\x1b[32m", "\x1b[33m", "\x1b[34m", "\x1b[35m", "\x1b[36m", "\x1b[37m",
        "\x1b[93m", "\x1b[91m", "\x1b[92m", "\x1b[94m", "\x1b[95m", "\x1b[96m", "\x1b[97m"
    };
    std::size_t const index = static_cast<std::size_t>(color);
    return index < Sequences.size() ? Sequences[index] : std::string_view();
}

void ConsoleWriter::WriteLines(std::string_view lines, ConsoleColor color)
{
    std::lock_guard lock(_mutex);
    if (!_device)
        return;
    if (_before)
        _before(*_device);
    bool const colored = UsesColorLocked() && color != ConsoleColor::Default;
    if (!colored)
        _device->Write(lines);
    else if (_device->SupportsVirtualTerminal() || !_device->IsTerminal())
    {
        std::string_view const sequence = GetAnsiSequence(color);
        std::string output;
        output.reserve(lines.size() + 16);
        std::size_t position = 0;
        while (position < lines.size())
        {
            std::size_t end = lines.find('\n', position);
            if (end == std::string_view::npos)
                end = lines.size();
            output.append(sequence);
            output.append(lines.substr(position, end - position));
            output.append("\x1b[0m");
            if (end < lines.size())
                output.push_back('\n');
            position = end + 1;
        }
        _device->Write(output);
    }
    else
    {
        _device->SetLegacyColor(color);
        _device->Write(lines);
        _device->ResetLegacyColor();
    }
    if (_after)
        _after(*_device);
    _device->Flush();
}

void ConsoleWriter::WithLock(std::function<void(ConsoleDevice&)> const& action)
{
    std::lock_guard lock(_mutex);
    if (_device && action)
        action(*_device);
}

void ConsoleWriter::Flush()
{
    std::lock_guard lock(_mutex);
    if (_device)
        _device->Flush();
}

void ConsoleWriter::Restore()
{
    std::lock_guard lock(_mutex);
    if (_restored || !_device)
        return;
    _device->Flush();
    _device->Restore();
    _restored = true;
}
