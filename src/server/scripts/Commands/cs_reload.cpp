/*
 * Project Ambrose by Imjustchico
 * The reload group: 'reload' on its own lists what this app can rebuild and which generation each is serving, 'reload all' rebuilds every one in the order they depend on, and 'reload <target>' rebuilds one by name, so nothing has to be added here when a later subsystem registers itself. A refusal is answered with every error the build found rather than the first, because an operator who is told only that it failed has to go to the log to learn why. 'journal export' writes the world edit journal into the world database's pending update folder, the one the updater reads, under Updates.SourcePath or the folder the build came from, rather than wherever the server happens to be running.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "ConfigMgr.h"
#include "DatabaseLoader.h"
#include "ReloadMgr.h"
#include "WorldEditJournal.h"
#include "ScriptMgr.h"

#include <fmt/format.h>

#include <filesystem>
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

    bool ExportJournal(CommandCaller& caller, std::vector<std::string> const&)
    {
        std::string error;
        std::optional<std::filesystem::path> const written = sWorldEditJournal.Export(DatabaseLoader::PendingUpdatesFolder(sConfigMgr, "world"), error);
        if (!written)
        {
            caller.Reply(error.empty() ? "The journal was not exported" : error);
            return true;
        }
        caller.Reply(fmt::format("{} edit(s) written to {}", sWorldEditJournal.Count(), written->string()));
        return true;
    }

    bool ShowJournal(CommandCaller& caller, std::vector<std::string> const&)
    {
        std::size_t const held = sWorldEditJournal.Count();
        if (held == 0)
        {
            caller.Reply("Nothing has been edited in the world database while this server has been running");
            return true;
        }
        caller.Reply(fmt::format("{} edit(s) made while this server has been running, newest last:", held));
        for (WorldEdit const& edit : sWorldEditJournal.Entries())
            caller.Reply(fmt::format("  {} by {} from {}: {}", WorldEditJournal::StampOf(edit.EpochMs), edit.Who, edit.Source, edit.Statement));
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
                { .Name = "journal", .SecurityLevel = SEC_ADMINISTRATOR, .Help = "what has been edited in the world database while this server has been running", .Run = ShowJournal, .Children = {
                    { .Name = "export", .SecurityLevel = SEC_ADMINISTRATOR, .Help = "write those edits as a pending update file", .Run = ExportJournal },
                } },
            };
        }
    };
}

void AddSC_cs_reload()
{
    new ReloadCommands();
}
