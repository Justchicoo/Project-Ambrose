/*
 * Project Ambrose by Imjustchico
 * A console holds the highest level there is, because whoever reaches it already has the machine; the recording caller is what a test runs a command as, keeping every line the command replied with so the test reads what an operator would have seen.
 */

#include "CommandCaller.h"
#include "AccountMgr.h"

#include <utility>

ConsoleCaller::ConsoleCaller(Sink reply) : _reply(std::move(reply))
{
}

uint8 ConsoleCaller::GetSecurityLevel() const
{
    return SEC_CONSOLE;
}

void ConsoleCaller::Reply(std::string_view line)
{
    if (_reply)
        _reply(line);
}

RecordingCaller::RecordingCaller(uint8 securityLevel, bool console, std::string name)
    : _securityLevel(securityLevel), _console(console), _name(std::move(name))
{
}

void RecordingCaller::Reply(std::string_view line)
{
    _lines.emplace_back(line);
}
