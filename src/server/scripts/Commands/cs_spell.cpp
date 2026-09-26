/*
 * Project Ambrose by Imjustchico
 * The spell group: 'spell info <name or id>' describes a spell the server holds, its school, rank and pips, accuracy, type and every effect under the one that holds it, found by template id, by its exact name through the hash the client looks it up by, or by its name whatever its case, and lists the names holding the text given when none is exact; 'spell reload' reads every spell again from the install, keeping the set serving when the new one fails and naming every failure.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "ReloadMgr.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "StringUtil.h"
#include "TypeRegistry.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    constexpr std::size_t ListedMatches = 10;

    class SpellCommands : public CommandScript
    {
    public:
        SpellCommands() : CommandScript("cs_spell") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "spell", .SecurityLevel = SEC_GAMEMASTER, .Help = "the spells this server has read from the install", .Children = {
                    { .Name = "info", .SecurityLevel = SEC_GAMEMASTER, .Help = "what one spell does, found by its name or template id", .Run = Info },
                    { .Name = "reload", .SecurityLevel = SEC_ADMINISTRATOR, .Help = "read every spell again from the install", .Run = Reload },
                } },
            };
        }

    private:
        static bool Info(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.empty())
            {
                caller.Reply("spell info takes a spell's name or template id, such as Fire Cat");
                return false;
            }
            std::shared_ptr<SpellStore const> const spells = sSpellMgr.GetSpells();
            if (spells->Size() == 0)
            {
                caller.Reply("No spells are loaded");
                return false;
            }
            std::string const wanted = fmt::format("{}", fmt::join(arguments, " "));
            SpellInfo const* spell = nullptr;
            if (std::optional<uint32> const id = Ambrose::StringTo<uint32>(wanted))
                spell = spells->Find(*id);
            if (spell == nullptr)
                spell = spells->FindByName(wanted);
            if (spell == nullptr)
            {
                std::vector<SpellInfo const*> const matches = spells->Search(wanted);
                if (matches.empty())
                {
                    caller.Reply(fmt::format("No spell is named {} or holds it in its name", wanted));
                    return false;
                }
                caller.Reply(fmt::format("No spell is named {}, but {} hold(s) it in their names:", wanted, matches.size()));
                for (std::size_t index = 0; index < std::min(matches.size(), ListedMatches); ++index)
                    caller.Reply(fmt::format("  {}, template {}", matches[index]->Name, matches[index]->TemplateId));
                if (matches.size() > ListedMatches)
                    caller.Reply(fmt::format("  and {} more", matches.size() - ListedMatches));
                return false;
            }
            TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
            for (std::string const& line : spell->Describe(catalog.get()))
                caller.Reply(line);
            return true;
        }

        static bool Reload(CommandCaller& caller, std::vector<std::string> const&)
        {
            for (std::string const& line : ReloadMgr::Describe(sReloadMgr.Reload(SpellMgr::Target)))
                caller.Reply(line);
            return true;
        }
    };
}

void AddSC_cs_spell()
{
    new SpellCommands();
}
