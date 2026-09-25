/*
 * Project Ambrose by Imjustchico
 * The settings group in game: '.settings list', 'get' and 'history' read the live settings at game master level, and 'set' and 'reset' change them at administrator level, through the same command the console's settings runs, with the change audited under the name of whoever made it. The console keeps the settings command every app registers, so this group is offered in game only.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "ScriptMgr.h"
#include "Settings.h"
#include "SettingsCommand.h"

#include <string>
#include <string_view>
#include <vector>

namespace
{
    ChatCommand::Handler Action(std::string action)
    {
        return [action = std::move(action)](CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            std::vector<std::string> line{ action };
            line.insert(line.end(), arguments.begin(), arguments.end());
            SettingAuthor const author{ caller.GetName(), 0, caller.IsConsole() ? std::string("console") : std::string("game") };
            return SettingsCommand::Run(sSettings, line, author, [&caller](std::string_view text) { caller.Reply(text); });
        };
    }

    class SettingsCommands : public CommandScript
    {
    public:
        SettingsCommands() : CommandScript("cs_settings") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "settings", .SecurityLevel = SEC_GAMEMASTER, .AvailableOnConsole = false, .Help = std::string(SettingsCommand::Help), .Children = {
                    { .Name = "list", .SecurityLevel = SEC_GAMEMASTER, .AvailableOnConsole = false, .Help = "every live setting, or those of one category", .Run = Action("list") },
                    { .Name = "get", .SecurityLevel = SEC_GAMEMASTER, .AvailableOnConsole = false, .Help = "one setting's value, where it comes from, its bounds and what it does", .Run = Action("get") },
                    { .Name = "history", .SecurityLevel = SEC_GAMEMASTER, .AvailableOnConsole = false, .Help = "who changed a setting live, when and why", .Run = Action("history") },
                    { .Name = "set", .SecurityLevel = SEC_ADMINISTRATOR, .AvailableOnConsole = false, .Help = "change a setting live, with a reason", .Run = Action("set") },
                    { .Name = "reset", .SecurityLevel = SEC_ADMINISTRATOR, .AvailableOnConsole = false, .Help = "return a setting to its config value", .Run = Action("reset") },
                } },
            };
        }
    };
}

void AddSC_cs_settings()
{
    new SettingsCommands();
}
