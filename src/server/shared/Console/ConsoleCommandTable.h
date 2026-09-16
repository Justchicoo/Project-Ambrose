/*
 * Project Ambrose by Imjustchico
 * Console commands with multi-word names, argument hints, help text and a sensitive flag, matched case-insensitively on whole words and run with quoted-argument splitting.
 */

#ifndef AMBROSE_CONSOLECOMMANDTABLE_H
#define AMBROSE_CONSOLECOMMANDTABLE_H

#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

class ConsoleCommandTable
{
public:
    using Reply = std::function<void(std::string_view)>;
    using Handler = std::function<bool(std::vector<std::string> const& arguments, Reply const& reply)>;

    struct Command
    {
        std::string Name;
        std::string Arguments;
        std::string Help;
        bool Sensitive = false;
        Handler Run;
    };

    enum class Result
    {
        Ran,
        Empty,
        Unknown,
        Usage
    };

    bool Register(Command command);
    bool Unregister(std::string_view name);
    Result Execute(std::string_view line, Reply const& reply) const;
    std::string DescribeForLog(std::string_view line) const;
    std::vector<std::string> DescribeCommands(std::string_view prefix = {}) const;

    static std::vector<std::string> Split(std::string_view line);

private:
    struct Entry
    {
        Command Definition;
        std::vector<std::string> Words;
    };

    Entry const* Find(std::vector<std::string> const& words) const;
    std::vector<std::string> DescribeMatching(std::vector<std::string> const& prefix) const;

    mutable std::mutex _mutex;
    std::vector<Entry> _entries;
};

#endif
