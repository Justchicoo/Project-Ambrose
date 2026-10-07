/*
 * Project Ambrose by Imjustchico
 * Whoever is running a command, reporting its security, console status, identity, optional game session and character, and receiving its replies.
 */

#ifndef AMBROSE_COMMANDCALLER_H
#define AMBROSE_COMMANDCALLER_H

#include "Types.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

class GameSession;

class CommandCaller
{
public:
    virtual ~CommandCaller() = default;

    virtual uint8 GetSecurityLevel() const = 0;
    virtual bool IsConsole() const = 0;
    virtual std::string GetName() const = 0;
    virtual GameSession* GetGameSession() const { return nullptr; }
    virtual uint64 GetCharacterId() const { return 0; }
    virtual void Reply(std::string_view line) = 0;
};

class ConsoleCaller : public CommandCaller
{
public:
    using Sink = std::function<void(std::string_view)>;

    explicit ConsoleCaller(Sink reply);

    uint8 GetSecurityLevel() const override;
    bool IsConsole() const override { return true; }
    std::string GetName() const override { return "console"; }
    void Reply(std::string_view line) override;

private:
    Sink _reply;
};

class RecordingCaller : public CommandCaller
{
public:
    RecordingCaller(uint8 securityLevel, bool console, std::string name = "test");

    uint8 GetSecurityLevel() const override { return _securityLevel; }
    bool IsConsole() const override { return _console; }
    std::string GetName() const override { return _name; }
    void Reply(std::string_view line) override;

    std::vector<std::string> const& GetLines() const { return _lines; }
    void Clear() { _lines.clear(); }

private:
    uint8 _securityLevel;
    bool _console;
    std::string _name;
    std::vector<std::string> _lines;
};

#endif
