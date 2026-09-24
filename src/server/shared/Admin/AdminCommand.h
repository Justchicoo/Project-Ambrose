/*
 * Project Ambrose by Imjustchico
 * The route that runs one command for a signed-in operator and the record it leaves behind. The command runs through the console table the app already fills, which on a game server is every command CommandMgr holds, so there is one set of commands and not a second one reachable only from a browser. A command that changes something irreversibly is refused without a confirmation the caller has to send on purpose, a command above the level the caller asked to run at is answered as though it did not exist, and what is written down is the line as the table describes it for a log, so an argument a command marked sensitive never reaches the record or the answer.
 */

#ifndef AMBROSE_ADMINCOMMAND_H
#define AMBROSE_ADMINCOMMAND_H

#include "Types.h"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

class AdminRouter;
class ConsoleCommandTable;

struct AdminCommandOutcome
{
    bool Ran = false;
    bool Refused = false;
    std::string Reason;
    std::vector<std::string> Lines;
};

class AdminCommand
{
public:
    static constexpr std::size_t MaxCommandBytes = 4096;
    static constexpr uint8 ConsoleLevel = 4;

    using Runner = std::function<AdminCommandOutcome(std::string const& line, uint8 level, bool confirmed)>;

    AdminCommand() = delete;

    static bool IsDestructive(std::string_view line) noexcept;
    static AdminCommandOutcome RunThroughTable(ConsoleCommandTable const& table, std::string const& line, uint8 level, bool confirmed);
    static void Register(AdminRouter& router, ConsoleCommandTable const& table, std::string appName, std::filesystem::path auditFile);
    static void Register(AdminRouter& router, Runner runner, ConsoleCommandTable const& table, std::string appName, std::filesystem::path auditFile);

    static std::vector<std::string> const& Fields();
};

#endif
