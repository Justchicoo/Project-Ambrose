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
    void Report(CommandCaller& caller, ReloadOutcome const& outcome)
    {
        if (outcome.Ok)
        {
            caller.Reply(fmt::format("{} is now generation {}", outcome.Target, outcome.Generation));
            return;
        }
        caller.Reply(fmt::format("{} was not reloaded and generation {} goes on serving", outcome.Target, outcome.Generation));
        for (std::string const& error : outcome.Errors)
            caller.Reply(fmt::format("  {}", error));
    }

    bool List(CommandCaller& caller)
    {
        std::vector<std::string> const targets = sReloadMgr.GetOrderedTargets();
        if (targets.empty())
        {
            caller.Reply("This app has nothing registered that can be reloaded on its own");
            return true;
        }
        caller.Reply(fmt::format("{} target(s), in the order they are reloaded:", targets.size()));
        for (std::string const& target : targets)
        {
            std::optional<ReloadOutcome> const last = sReloadMgr.GetLastOutcome(target);
            if (!last)
                caller.Reply(fmt::format("  {} at generation {}, not reloaded since this app started", target, sReloadMgr.GetGeneration(target)));
            else
                caller.Reply(fmt::format("  {} at generation {}, last attempt {}", target, last->Generation, last->Ok ? "held" : "kept the one before"));
        }
        return true;
    }

    bool Run(CommandCaller& caller, std::vector<std::string> const& arguments)
    {
        if (arguments.empty())
            return List(caller);

        std::string const& name = arguments.front();
        if (name == "all")
        {
            std::vector<ReloadOutcome> const outcomes = sReloadMgr.ReloadAll();
            if (outcomes.empty())
            {
                caller.Reply("This app has nothing registered that can be reloaded on its own");
                return true;
            }
            for (ReloadOutcome const& outcome : outcomes)
                Report(caller, outcome);
            return true;
        }

        if (!sReloadMgr.IsRegistered(name))
        {
            caller.Reply(fmt::format("Nothing is registered by the name {} on this app", name));
            return List(caller);
        }
        Report(caller, sReloadMgr.Reload(name));
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
