/*
 * Project Ambrose by Imjustchico
 * The prefix a client types is taken off first, so a console and a chat line reach the same table. A line is split on spaces with quoted words kept whole, then walked down the tree taking the longest run of words that names a command, so 'character gold 500' finds the gold command inside character and leaves 500 as its argument rather than guessing where the name ends. A command the caller may not run is answered exactly as one that does not exist, and a group named on its own lists what is under it that the caller may actually see.
 */

#include "CommandMgr.h"
#include "Log.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <utility>

namespace
{
    std::string_view Unprefixed(std::string_view line, std::string_view prefix)
    {
        std::string_view trimmed = Ambrose::Trim(line);
        if (!prefix.empty() && trimmed.starts_with(prefix))
            trimmed.remove_prefix(prefix.size());
        return trimmed;
    }
}

CommandMgr& CommandMgr::Instance()
{
    static CommandMgr instance;
    return instance;
}

std::vector<std::string> CommandMgr::Split(std::string_view line)
{
    std::vector<std::string> words;
    std::string current;
    bool quoted = false;
    bool holding = false;
    for (char const c : line)
    {
        if (c == '"')
        {
            quoted = !quoted;
            holding = true;
            continue;
        }
        if (!quoted && (c == ' ' || c == '\t'))
        {
            if (holding)
            {
                words.push_back(current);
                current.clear();
                holding = false;
            }
            continue;
        }
        current.push_back(c);
        holding = true;
    }
    if (holding)
        words.push_back(current);
    return words;
}

CommandMgr::Node CommandMgr::Build(ChatCommand const& command, std::string const& parentPath)
{
    Node node;
    node.Name = Ambrose::ToLower(command.Name);
    node.Path = parentPath.empty() ? node.Name : parentPath + " " + node.Name;
    node.SecurityLevel = command.SecurityLevel;
    node.AvailableInGame = command.AvailableInGame;
    node.AvailableOnConsole = command.AvailableOnConsole;
    node.Help = command.Help;
    node.Sensitive = command.Sensitive;
    node.Run = command.Run;
    for (ChatCommand const& child : command.Children)
        node.Children.push_back(Build(child, node.Path));
    return node;
}

void CommandMgr::ApplyOverrides(Node& node)
{
    if (auto const found = _overrides.find(node.Path); found != _overrides.end())
        node.SecurityLevel = found->second;
    for (Node& child : node.Children)
    {
        child.SecurityLevel = std::max(child.SecurityLevel, node.SecurityLevel);
        ApplyOverrides(child);
    }
}

void CommandMgr::Load(std::vector<ChatCommand> commands)
{
    std::lock_guard const lock(_mutex);
    _roots.clear();
    _count = 0;
    for (ChatCommand const& command : commands)
        _roots.push_back(Build(command, std::string()));
    for (Node& root : _roots)
        ApplyOverrides(root);

    auto const count = [](auto&& self, Node const& node) -> std::size_t
    {
        std::size_t total = node.Run ? 1u : 0u;
        for (Node const& child : node.Children)
            total += self(self, child);
        return total;
    };
    for (Node const& root : _roots)
        _count += count(count, root);
}

void CommandMgr::SetOverrides(std::map<std::string, uint8, std::less<>> overrides)
{
    std::lock_guard const lock(_mutex);
    _overrides = std::move(overrides);
    for (Node& root : _roots)
        ApplyOverrides(root);
}

void CommandMgr::SetPrefix(std::string prefix)
{
    std::lock_guard const lock(_mutex);
    _prefix = std::move(prefix);
}

std::string CommandMgr::GetPrefix() const
{
    std::lock_guard const lock(_mutex);
    return _prefix;
}

void CommandMgr::SetLogging(bool logging)
{
    std::lock_guard const lock(_mutex);
    _logging = logging;
}

bool CommandMgr::GetLogging() const
{
    std::lock_guard const lock(_mutex);
    return _logging;
}

void CommandMgr::Clear()
{
    std::lock_guard const lock(_mutex);
    _roots.clear();
    _overrides.clear();
    _count = 0;
}

std::size_t CommandMgr::GetCommandCount() const
{
    std::lock_guard const lock(_mutex);
    return _count;
}

CommandMgr::Node const* CommandMgr::FindIn(std::vector<Node> const& nodes, std::vector<std::string> const& words, std::size_t& index)
{
    if (index >= words.size())
        return nullptr;
    std::string const word = Ambrose::ToLower(words[index]);
    for (Node const& node : nodes)
    {
        if (node.Name != word)
            continue;
        ++index;
        std::size_t deeper = index;
        if (Node const* const child = FindIn(node.Children, words, deeper))
        {
            index = deeper;
            return child;
        }
        return &node;
    }
    return nullptr;
}

CommandMgr::Node const* CommandMgr::Find(std::vector<std::string> const& words, std::size_t& used) const
{
    used = 0;
    return FindIn(_roots, words, used);
}

CommandMatch CommandMgr::Parse(std::string_view line) const
{
    CommandMatch match;
    std::lock_guard const lock(_mutex);
    std::vector<std::string> const words = Split(Unprefixed(line, _prefix));
    if (words.empty())
        return match;
    std::size_t used = 0;
    Node const* const node = Find(words, used);
    if (!node)
        return match;
    match.Found = true;
    match.Name = node->Path;
    match.SecurityLevel = node->SecurityLevel;
    match.Arguments.assign(words.begin() + static_cast<std::ptrdiff_t>(used), words.end());
    return match;
}

std::string CommandMgr::DescribeForLog(std::string_view line) const
{
    CommandMatch const match = Parse(line);
    if (!match.Found)
        return std::string(Ambrose::Trim(line));
    bool sensitive = false;
    {
        std::lock_guard const lock(_mutex);
        std::vector<std::string> const words = Split(Unprefixed(line, _prefix));
        std::size_t used = 0;
        if (Node const* const node = Find(words, used))
            sensitive = node->Sensitive;
    }
    std::string said = match.Name;
    if (sensitive)
        return match.Arguments.empty() ? said : said + " ***";
    for (std::string const& argument : match.Arguments)
        said += " " + argument;
    return said;
}

void CommandMgr::Collect(Node const& node, uint8 securityLevel, bool console, std::vector<std::string>& lines) const
{
    if (securityLevel < node.SecurityLevel)
        return;
    if (node.Run && (console ? node.AvailableOnConsole : node.AvailableInGame))
        lines.push_back(node.Help.empty() ? node.Path : fmt::format("{} - {}", node.Path, node.Help));
    for (Node const& child : node.Children)
        Collect(child, securityLevel, console, lines);
}

std::vector<std::string> CommandMgr::Describe(uint8 securityLevel, bool console) const
{
    std::lock_guard const lock(_mutex);
    std::vector<std::string> lines;
    for (Node const& root : _roots)
        Collect(root, securityLevel, console, lines);
    std::sort(lines.begin(), lines.end());
    return lines;
}

CommandResult CommandMgr::Execute(CommandCaller& caller, std::string_view line) const
{
    if (line.size() > MaxCommandBytes)
    {
        caller.Reply(fmt::format("That command is longer than the {} bytes a command may be", MaxCommandBytes));
        return CommandResult::Refused;
    }

    std::vector<std::string> words;
    Node const* node = nullptr;
    std::size_t used = 0;
    std::vector<std::string> arguments;
    ChatCommand::Handler run;
    std::vector<std::string> help;
    std::string path;
    bool sensitive = false;
    bool logging = false;
    {
        std::lock_guard const lock(_mutex);
        words = Split(Unprefixed(line, _prefix));
        if (words.empty())
            return CommandResult::Empty;
        node = Find(words, used);
        if (node)
        {
            if (caller.GetSecurityLevel() < node->SecurityLevel || (caller.IsConsole() ? !node->AvailableOnConsole : !node->AvailableInGame))
                node = nullptr;
            else
            {
                arguments.assign(words.begin() + static_cast<std::ptrdiff_t>(used), words.end());
                run = node->Run;
                path = node->Path;
                sensitive = node->Sensitive;
                logging = _logging;
                if (!run)
                    Collect(*node, caller.GetSecurityLevel(), caller.IsConsole(), help);
            }
        }
    }

    if (!node)
    {
        caller.Reply("There is no such command");
        return CommandResult::Unknown;
    }
    if (!run)
    {
        std::sort(help.begin(), help.end());
        for (std::string const& text : help)
            caller.Reply(text);
        return help.empty() ? CommandResult::Unknown : CommandResult::Usage;
    }
    bool const ran = run(caller, arguments);
    if (logging)
    {
        std::string said = path;
        if (sensitive)
            said += arguments.empty() ? "" : " ***";
        else
            for (std::string const& argument : arguments)
                said += " " + argument;
        LOG_INFO("server.commands", "{} ran {}, which {}", caller.GetName(), said, ran ? "worked" : "was not used correctly");
    }
    return ran ? CommandResult::Ran : CommandResult::Usage;
}
