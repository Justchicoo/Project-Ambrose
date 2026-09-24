/*
 * Project Ambrose by Imjustchico
 * Reading the request, refusing what should not run, running the rest through the app's own console table and writing down what happened. A body is held to one shape and one size before anything is executed, because a command line is the last place to be generous about what a caller sent. The record is appended as one JSON object a line, which is what a later page can read back without a parser of its own, and it carries what the table says the line was rather than the line itself.
 */

#include "AdminCommand.h"

#include "AdminRouter.h"
#include "ConsoleCommandTable.h"
#include "Log.h"
#include "LogTimestamp.h"
#include "StringUtil.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>

namespace
{
    constexpr std::string_view CommandCategory = "server.admin";

    constexpr std::array<std::string_view, 6> DestructiveWords{ "shutdown", "restart", "delete", "drop", "wipe", "ban" };

    void Append(std::filesystem::path const& file, nlohmann::json const& row)
    {
        if (file.empty())
            return;
        std::error_code code;
        std::filesystem::create_directories(file.parent_path(), code);
        std::ofstream out(file, std::ios::app | std::ios::binary);
        if (!out)
        {
            LOG_WARN(CommandCategory, "A remote command could not be written to its record at {}", file.generic_string());
            return;
        }
        out << row.dump() << '\n';
    }
}

bool AdminCommand::IsDestructive(std::string_view line) noexcept
{
    std::vector<std::string> const words = ConsoleCommandTable::Split(line);
    for (std::string const& word : words)
    {
        std::string const folded = Ambrose::ToLower(word);
        for (std::string_view const destructive : DestructiveWords)
            if (folded == destructive)
                return true;
    }
    return false;
}

AdminCommandOutcome AdminCommand::RunThroughTable(ConsoleCommandTable const& table, std::string const& line, uint8 level, bool confirmed)
{
    AdminCommandOutcome outcome;
    if (level < ConsoleLevel)
    {
        outcome.Refused = true;
        outcome.Reason = "there is no such command";
        return outcome;
    }
    if (IsDestructive(line) && !confirmed)
    {
        outcome.Refused = true;
        outcome.Reason = "this command changes something that cannot be undone, so it needs confirm";
        return outcome;
    }
    ConsoleCommandTable::Result const result = table.Execute(line, [&outcome](std::string_view text) { outcome.Lines.emplace_back(text); });
    switch (result)
    {
        case ConsoleCommandTable::Result::Ran:
            outcome.Ran = true;
            return outcome;
        case ConsoleCommandTable::Result::Empty:
            outcome.Refused = true;
            outcome.Reason = "there was no command to run";
            return outcome;
        case ConsoleCommandTable::Result::Unknown:
            outcome.Refused = true;
            outcome.Reason = "there is no such command";
            return outcome;
        case ConsoleCommandTable::Result::Usage:
            outcome.Refused = true;
            outcome.Reason = "the command was not used the way it takes";
            return outcome;
    }
    outcome.Refused = true;
    outcome.Reason = "the command did not say what it did";
    return outcome;
}

std::vector<std::string> const& AdminCommand::Fields()
{
    static std::vector<std::string> const fields{ "time", "epoch_ms", "app", "who", "address", "request", "command", "level", "confirmed", "ran", "refused", "reason" };
    return fields;
}

void AdminCommand::Register(AdminRouter& router, ConsoleCommandTable const& table, std::string appName, std::filesystem::path auditFile)
{
    Register(router, [&table](std::string const& line, uint8 level, bool confirmed) { return RunThroughTable(table, line, level, confirmed); },
        table, std::move(appName), std::move(auditFile));
}

void AdminCommand::Register(AdminRouter& router, Runner runner, ConsoleCommandTable const& table, std::string appName, std::filesystem::path auditFile)
{
    router.AddGuarded("POST", "/api/command", "console.write", [runner = std::move(runner), &table, appName = std::move(appName), auditFile = std::move(auditFile)](AdminRequest const& request)
    {
        if (request.Body.size() > MaxCommandBytes)
            return AdminResponse::Invalid("A command is at most 4096 bytes", { { "command", "this is longer than 4096 bytes" } });

        nlohmann::json const body = nlohmann::json::parse(request.Body, nullptr, false);
        if (!body.is_object())
            return AdminResponse::Invalid("Running a command takes a JSON object", { { "command", "Enter the command to run" } });

        std::vector<std::pair<std::string, std::string>> fields;
        for (auto const& [key, value] : body.items())
            if (key != "command" && key != "level" && key != "confirm")
                fields.emplace_back(key, "Running a command takes only command, level and confirm");
        auto const command = body.find("command");
        if (command == body.end() || !command->is_string() || Ambrose::Trim(command->get_ref<std::string const&>()).empty())
            fields.emplace_back("command", "Enter the command to run");
        auto const level = body.find("level");
        if (level != body.end() && (!level->is_number_unsigned() || level->get<uint64>() > ConsoleLevel))
            fields.emplace_back("level", "A level is 0 to 4");
        auto const confirm = body.find("confirm");
        if (confirm != body.end() && !confirm->is_boolean())
            fields.emplace_back("confirm", "Confirm is true or false");
        if (!fields.empty())
            return AdminResponse::Invalid("That is not a command this app can run", std::move(fields));

        std::string const line(Ambrose::Trim(command->get_ref<std::string const&>()));
        uint8 const asked = level == body.end() ? ConsoleLevel : static_cast<uint8>(level->get<uint64>());
        bool const confirmed = confirm != body.end() && confirm->get<bool>();
        std::string const written = table.DescribeForLog(line);

        AdminCommandOutcome const outcome = runner(line, asked, confirmed);
        auto const now = std::chrono::system_clock::now();

        nlohmann::json row;
        row["time"] = std::string(LogTimestamp::FormatPrefix(now, true));
        row["epoch_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        row["app"] = appName;
        row["who"] = request.Principal;
        row["address"] = request.RemoteAddress;
        row["request"] = request.Id;
        row["command"] = written;
        row["level"] = asked;
        row["confirmed"] = confirmed;
        row["ran"] = outcome.Ran;
        row["refused"] = outcome.Refused;
        row["reason"] = outcome.Reason;
        Append(auditFile, row);
        LOG_INFO(CommandCategory, "{} from {} {} {} (request {})", request.Principal.empty() ? std::string("an operator") : request.Principal,
            request.RemoteAddress, outcome.Ran ? "ran" : "was refused", written, request.Id);
        if (outcome.Refused)
            LOG_INFO(CommandCategory, "{} was refused because {}", written, outcome.Reason);

        nlohmann::json answer;
        answer["command"] = written;
        answer["success"] = outcome.Ran;
        answer["refused"] = outcome.Refused;
        answer["reason"] = outcome.Reason;
        answer["request_id"] = request.Id;
        answer["lines"] = outcome.Lines;
        return AdminResponse::Json(outcome.Ran ? 200 : 409, answer.dump());
    });
}
