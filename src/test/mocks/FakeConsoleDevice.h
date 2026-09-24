/*
 * Project Ambrose by Imjustchico
 * Console device double that records bytes and legacy color calls with switchable terminal, VT state and window width.
 */

#ifndef AMBROSE_FAKECONSOLEDEVICE_H
#define AMBROSE_FAKECONSOLEDEVICE_H

#include "ConsoleDevice.h"

#include <mutex>
#include <string>
#include <vector>

class FakeConsoleDevice : public ConsoleDevice
{
public:
    FakeConsoleDevice(bool terminal, bool virtualTerminal);

    bool IsTerminal() const noexcept override;
    bool SupportsVirtualTerminal() const noexcept override;
    std::size_t GetColumns() const override;
    void Write(std::string_view utf8) override;
    void SetLegacyColor(ConsoleColor color) override;
    void ResetLegacyColor() override;
    void Flush() override;
    void Restore() override;

    void SetColumns(std::size_t columns);
    std::string Output() const;
    std::vector<ConsoleColor> LegacyColors() const;
    uint32 ResetCount() const;
    uint32 RestoreCount() const;

private:
    mutable std::mutex _mutex;
    bool _terminal;
    bool _virtualTerminal;
    std::size_t _columns = 0;
    std::string _output;
    std::vector<ConsoleColor> _legacy;
    uint32 _resets = 0;
    uint32 _restores = 0;
};

#endif
