/*
 * Project Ambrose by Imjustchico
 * Decides when to color console output and writes each run of text with ANSI sequences or legacy console attributes, resetting before every line break.
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

bool ConsoleWriter::IsTerminal() const
{
    std::lock_guard lock(_mutex);
    return _device && !_restored && _device->IsTerminal();
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
        "\x1b[30m", "\x1b[31m", "\x1b[32m", "\x1b[33m", "\x1b[34m", "\x1b[35m", "\x1b[36m", "\x1b[90m",
        "\x1b[93m", "\x1b[91m", "\x1b[92m", "\x1b[94m", "\x1b[95m", "\x1b[96m", "\x1b[97m"
    };
    std::size_t const index = static_cast<std::size_t>(color);
    return index < Sequences.size() ? Sequences[index] : std::string_view();
}

void ConsoleWriter::WriteLines(std::string_view lines, ConsoleColor color)
{
    ConsoleSegment const segment{ lines, color };
    WriteLines(std::span<ConsoleSegment const>(&segment, 1));
}

void ConsoleWriter::WriteLines(std::span<ConsoleSegment const> segments)
{
    std::lock_guard lock(_mutex);
    if (!_device)
        return;
    if (_before)
        _before(*_device);
    WriteLocked(segments);
    if (_after)
        _after(*_device);
    _device->Flush();
}

void ConsoleWriter::WriteInline(std::span<ConsoleSegment const> segments)
{
    std::lock_guard lock(_mutex);
    if (!_device)
        return;
    WriteLocked(segments);
    _device->Flush();
}

void ConsoleWriter::WriteLocked(std::span<ConsoleSegment const> segments)
{
    bool const colored = UsesColorLocked();
    if (!colored)
    {
        for (ConsoleSegment const& segment : segments)
            _device->Write(segment.Text);
        return;
    }
    if (_device->SupportsVirtualTerminal() || !_device->IsTerminal())
    {
        std::size_t size = 16;
        for (ConsoleSegment const& segment : segments)
            size += segment.Text.size() + 16;
        std::string output;
        output.reserve(size);
        for (ConsoleSegment const& segment : segments)
        {
            std::string_view remaining = segment.Text;
            while (!remaining.empty())
            {
                std::size_t const end = remaining.find('\n');
                std::string_view const piece = remaining.substr(0, end == std::string_view::npos ? remaining.size() : end);
                if (!piece.empty() && segment.Color != ConsoleColor::Default)
                {
                    output.append(GetAnsiSequence(segment.Color));
                    output.append(piece);
                    output.append("\x1b[0m");
                }
                else
                    output.append(piece);
                if (end == std::string_view::npos)
                    break;
                output.push_back('\n');
                remaining.remove_prefix(end + 1);
            }
        }
        _device->Write(output);
        return;
    }
    ConsoleColor current = ConsoleColor::Default;
    bool applied = false;
    for (ConsoleSegment const& segment : segments)
    {
        if (segment.Text.empty())
            continue;
        if (segment.Color != current)
        {
            if (segment.Color == ConsoleColor::Default)
            {
                if (applied)
                    _device->ResetLegacyColor();
                applied = false;
            }
            else
            {
                _device->SetLegacyColor(segment.Color);
                applied = true;
            }
            current = segment.Color;
        }
        _device->Write(segment.Text);
    }
    if (applied)
        _device->ResetLegacyColor();
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
