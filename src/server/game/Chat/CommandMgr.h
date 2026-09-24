/*
 * Project Ambrose by Imjustchico
 * The one place a command is looked up and judged: the tables every CommandScript offers are gathered into one tree, a line is split into words and walked down that tree to the deepest command that matches, what is left over is the arguments, and the level the caller holds is checked against the level the command carries, which a command_security row may have raised or lowered. A line may carry the prefix a client types before a command, which GM.CommandPrefix names, and it is taken off before the words are read, so the same table answers a console that types no prefix and a chat line that does. A caller who does not hold the level is told there is no such command, never that there is one they may not run, so the table gives nothing away. Every command that runs is written to the log with who ran it and how it ended, unless GM.LogCommands says otherwise, and a command whose arguments are secret is logged without them.
 */

#ifndef AMBROSE_COMMANDMGR_H
#define AMBROSE_COMMANDMGR_H

#include "ChatCommand.h"
#include "CommandCaller.h"
#include "Types.h"

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class CommandResult : uint8
{
    Ran,
    Refused,
    Unknown,
    Usage,
    Empty
};

struct CommandMatch
{
    std::string Name;
    uint8 SecurityLevel = 0;
    std::vector<std::string> Arguments;
    bool Found = false;
};

class CommandMgr
{
public:
    static constexpr std::size_t MaxCommandBytes = 4096;

    static CommandMgr& Instance();

    CommandMgr(CommandMgr const&) = delete;
    CommandMgr& operator=(CommandMgr const&) = delete;

    void Load(std::vector<ChatCommand> commands);
    void SetOverrides(std::map<std::string, uint8, std::less<>> overrides);
    void SetPrefix(std::string prefix);
    std::string GetPrefix() const;
    void SetLogging(bool logging);
    bool GetLogging() const;
    void Clear();

    std::size_t GetCommandCount() const;
    std::vector<std::string> Describe(uint8 securityLevel, bool console) const;

    CommandMatch Parse(std::string_view line) const;
    std::string DescribeForLog(std::string_view line) const;
    CommandResult Execute(CommandCaller& caller, std::string_view line) const;

    static std::vector<std::string> Split(std::string_view line);

    static constexpr std::string_view DefaultPrefix = ".";

private:
    CommandMgr() = default;

    struct Node
    {
        std::string Name;
        std::string Path;
        uint8 SecurityLevel = 0;
        bool AvailableInGame = true;
        bool AvailableOnConsole = true;
        std::string Help;
        bool Sensitive = false;
        ChatCommand::Handler Run;
        std::vector<Node> Children;
    };

    static Node Build(ChatCommand const& command, std::string const& parentPath);
    void ApplyOverrides(Node& node);
    Node const* Find(std::vector<std::string> const& words, std::size_t& used) const;
    static Node const* FindIn(std::vector<Node> const& nodes, std::vector<std::string> const& words, std::size_t& index);
    void Collect(Node const& node, uint8 securityLevel, bool console, std::vector<std::string>& lines) const;

    mutable std::mutex _mutex;
    std::vector<Node> _roots;
    std::map<std::string, uint8, std::less<>> _overrides;
    std::string _prefix = ".";
    bool _logging = true;
    std::size_t _count = 0;
};

#define sCommandMgr CommandMgr::Instance()

#endif
