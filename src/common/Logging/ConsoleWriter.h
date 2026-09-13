/*
 * Project Ambrose by Imjustchico
 * Serialized colored line output with before and after hooks so a console prompt can redraw around log lines.
 */

#ifndef AMBROSE_CONSOLEWRITER_H
#define AMBROSE_CONSOLEWRITER_H

#include "ConsoleDevice.h"

#include <functional>
#include <mutex>

class ConsoleWriter
{
public:
    using LineHook = std::function<void(ConsoleDevice&)>;

    explicit ConsoleWriter(std::unique_ptr<ConsoleDevice> device);

    ConsoleWriter(ConsoleWriter const&) = delete;
    ConsoleWriter& operator=(ConsoleWriter const&) = delete;

    static ConsoleWriter& Instance();

    void SetColorMode(ConsoleColorMode mode);
    ConsoleColorMode GetColorMode() const;
    bool UsesColor() const;
    void SetLineHooks(LineHook before, LineHook after);
    void WriteLines(std::string_view lines, ConsoleColor color);
    void WithLock(std::function<void(ConsoleDevice&)> const& action);
    void Flush();
    void Restore();

    static std::string_view GetAnsiSequence(ConsoleColor color) noexcept;

private:
    bool UsesColorLocked() const;

    mutable std::recursive_mutex _mutex;
    std::unique_ptr<ConsoleDevice> _device;
    ConsoleColorMode _mode = ConsoleColorMode::Auto;
    bool _noColorEnvironment = false;
    bool _forceColorEnvironment = false;
    bool _restored = false;
    LineHook _before;
    LineHook _after;
};

#endif
