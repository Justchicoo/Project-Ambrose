/*
 * Project Ambrose by Imjustchico
 * Records console bytes, legacy color changes, restores and a settable window width for console appender tests.
 */

#include "FakeConsoleDevice.h"

FakeConsoleDevice::FakeConsoleDevice(bool terminal, bool virtualTerminal) : _terminal(terminal), _virtualTerminal(virtualTerminal)
{
}

bool FakeConsoleDevice::IsTerminal() const noexcept
{
    return _terminal;
}

bool FakeConsoleDevice::SupportsVirtualTerminal() const noexcept
{
    return _virtualTerminal;
}

std::size_t FakeConsoleDevice::GetColumns() const
{
    std::lock_guard lock(_mutex);
    return _columns;
}

void FakeConsoleDevice::SetColumns(std::size_t columns)
{
    std::lock_guard lock(_mutex);
    _columns = columns;
}

void FakeConsoleDevice::Write(std::string_view utf8)
{
    std::lock_guard lock(_mutex);
    _output.append(utf8);
}

void FakeConsoleDevice::SetLegacyColor(ConsoleColor color)
{
    std::lock_guard lock(_mutex);
    _legacy.push_back(color);
}

void FakeConsoleDevice::ResetLegacyColor()
{
    std::lock_guard lock(_mutex);
    ++_resets;
}

void FakeConsoleDevice::Flush()
{
}

void FakeConsoleDevice::Restore()
{
    std::lock_guard lock(_mutex);
    ++_restores;
}

std::string FakeConsoleDevice::Output() const
{
    std::lock_guard lock(_mutex);
    return _output;
}

std::vector<ConsoleColor> FakeConsoleDevice::LegacyColors() const
{
    std::lock_guard lock(_mutex);
    return _legacy;
}

uint32 FakeConsoleDevice::ResetCount() const
{
    std::lock_guard lock(_mutex);
    return _resets;
}

uint32 FakeConsoleDevice::RestoreCount() const
{
    std::lock_guard lock(_mutex);
    return _restores;
}
