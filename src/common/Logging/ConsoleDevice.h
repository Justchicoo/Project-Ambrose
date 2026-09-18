/*
 * Project Ambrose by Imjustchico
 * Standard output device: terminal and mintty detection, the window's column count, VT enable and restore, UTF-16 console writes, raw redirected bytes.
 */

#ifndef AMBROSE_CONSOLEDEVICE_H
#define AMBROSE_CONSOLEDEVICE_H

#include "LogCommon.h"

#include <cstddef>
#include <memory>
#include <string_view>

class ConsoleDevice
{
public:
    virtual ~ConsoleDevice() = default;

    virtual bool IsTerminal() const noexcept = 0;
    virtual bool SupportsVirtualTerminal() const noexcept = 0;
    virtual std::size_t GetColumns() const = 0;
    virtual void Write(std::string_view utf8) = 0;
    virtual void SetLegacyColor(ConsoleColor color) = 0;
    virtual void ResetLegacyColor() = 0;
    virtual void Flush() = 0;
    virtual void Restore() = 0;

    static std::unique_ptr<ConsoleDevice> CreateStandardOutput();
};

#endif
