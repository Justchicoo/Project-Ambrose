/*
 * Project Ambrose by Imjustchico
 * The sigil group: 'sigil info <name or id>' describes a sigil the server holds, its circles by kind and, for a combat sigil, its engage radius and each game mode's scalars and limits, found by template id, by its exact name through the hash the client looks it up by, or by its name whatever its case, and lists the names holding the text given when none is exact; 'sigil reload' reads every sigil again from the install, keeping the set serving when the new one fails and naming every failure.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "ReloadMgr.h"
#include "ScriptMgr.h"
#include "SigilMgr.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace
{
    constexpr std::size_t ListedMatches = 10;

    class SigilCommands : public CommandScript
    {
    public:
        SigilCommands() : CommandScript("cs_sigil") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "sigil", .SecurityLevel = SEC_GAMEMASTER, .Help = "the sigils this server has read from the install", .Children = {
                    { .Name = "info", .SecurityLevel = SEC_GAMEMASTER, .Help = "one sigil's circles and limits, found by its name or template id", .Run = Info },
                    { .Name = "reload", .SecurityLevel = SEC_ADMINISTRATOR, .Help = "read every sigil again from the install", .Run = Reload },
                } },
            };
        }

    private:
        static bool Info(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.empty())
            {
                caller.Reply("sigil info takes a sigil's name or template id, such as CombatSigil8Actor");
                return false;
            }
            std::shared_ptr<SigilStore const> const sigils = sSigilMgr.GetSigils();
            if (sigils->Size() == 0)
            {
                caller.Reply("No sigils are loaded");
                return false;
            }
            std::string const wanted = fmt::format("{}", fmt::join(arguments, " "));
            SigilInfo const* const sigil = sigils->FindByIdOrName(wanted);
            if (sigil == nullptr)
            {
                std::vector<SigilInfo const*> const matches = sigils->Search(wanted);
                if (matches.empty())
                {
                    caller.Reply(fmt::format("No sigil is named {} or holds it in its name", wanted));
                    return false;
                }
                caller.Reply(fmt::format("No sigil is named {}, but {} hold(s) it in their names:", wanted, matches.size()));
                for (std::size_t index = 0; index < std::min(matches.size(), ListedMatches); ++index)
                    caller.Reply(fmt::format("  {}, template {}", matches[index]->Name, matches[index]->TemplateId));
                if (matches.size() > ListedMatches)
                    caller.Reply(fmt::format("  and {} more", matches.size() - ListedMatches));
                return false;
            }
            for (std::string const& line : sigil->Describe())
                caller.Reply(line);
            return true;
        }

        static bool Reload(CommandCaller& caller, std::vector<std::string> const&)
        {
            for (std::string const& line : ReloadMgr::Describe(sReloadMgr.Reload(SigilMgr::Target)))
                caller.Reply(line);
            return true;
        }
    };
}

void AddSC_cs_sigil()
{
    new SigilCommands();
}
