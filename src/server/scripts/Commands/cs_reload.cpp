/*
 * Project Ambrose by Imjustchico
 * The reload group: 'reload' on its own lists what this app can rebuild and which generation each is serving, 'reload all' rebuilds every one in the order they depend on, and 'reload <target>' rebuilds one by name, so nothing has to be added here when a later subsystem registers itself. A refusal is answered with every error the build found rather than the first, because an operator who is told only that it failed has to go to the log to learn why.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "ReloadMgr.h"
#include "ScriptMgr.h"

#include <fmt/format.h>

#include <optional>
#include <string>
#include <vector>

namespace
{
    void Say(CommandCaller& caller, std::vector<std::string> const& lines)
    {
        for (std::string const& line : lines)
            caller.Reply(line);
    }

    bool Run(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        if (arguments.empty())
        {
            Say(caller, sReloadMgr.DescribeTargets());
            return true;
        }

        std::string const& name = arguments.front();
        if (name == "all")
        {
            std::vector<ReloadOutcome> const outcomes = sReloadMgr.ReloadAll();
            if (outcomes.empty())
                Say(caller, sReloadMgr.DescribeTargets());
            for (ReloadOutcome const& outcome : outcomes)
                Say(caller, ReloadMgr::Describe(outcome));
            return true;
        }

        if (!sReloadMgr.IsRegistered(name))
        {
            caller.Reply(fmt::format("Nothing is registered by the name {} on this app", name));
            Say(caller, sReloadMgr.DescribeTargets());
            return true;
        }
        Say(caller, ReloadMgr::Describe(sReloadMgr.Reload(name)));
        return true;
    }

    class ReloadCommands : public CommandScript
    {
    public:
        ReloadCommands() : CommandScript("cs_reload") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "reload", .SecurityLevel = SEC_ADMINISTRATOR, .Help = "list what can be rebuilt without a restart, or rebuild one by name, or all", .Run = Run },
            };
        }
    };
}

void AddSC_cs_reload()
{
    new ReloadCommands();
}
