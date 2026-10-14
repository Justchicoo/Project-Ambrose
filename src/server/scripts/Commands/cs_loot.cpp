/*
 * Project Ambrose by Imjustchico
 * Lets a game master try a loot table: 'loot test <table or creature template id> [wizard]' rolls the table, or every table creature_loot gives that template, once for a wizard by its own school, level and quests on the world thread, and lists what it would get without giving any of it; a game master in game rolls for its own wizard when it names none, and the console must name one by character id or name.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "GameSession.h"
#include "LootMgr.h"
#include "ScriptMgr.h"
#include "StringUtil.h"
#include "World.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    class LootCommands : public CommandScript
    {
    public:
        LootCommands() : CommandScript("cs_loot") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "loot", .SecurityLevel = SEC_GAMEMASTER, .Help = "try loot tables", .Children = {
                    { .Name = "test", .SecurityLevel = SEC_GAMEMASTER,
                        .Help = "roll a loot table, or a creature template's tables, once for a wizard and list what it would get, giving nothing: loot test <table or creature template id> [wizard]",
                        .Run = Test },
                } },
            };
        }

    private:
        static bool Test(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.empty())
            {
                caller.Reply("loot test takes a loot table or a creature template id, then the wizard, such as loot test LT-Drop-BronzeGear-04 555");
                return false;
            }
            std::vector<std::string> tables;
            if (std::optional<uint32> const creature = Ambrose::StringTo<uint32>(arguments.front()); creature && !sLootMgr.HasTable(arguments.front()))
            {
                tables = sLootMgr.GetCreatureTables(*creature);
                if (tables.empty())
                {
                    caller.Reply(fmt::format("creature_loot gives template {} no loot table", *creature));
                    return false;
                }
            }
            else if (sLootMgr.HasTable(arguments.front()))
                tables.push_back(arguments.front());
            else
            {
                caller.Reply(fmt::format("No loot table is named {}", arguments.front()));
                return false;
            }

            std::string wizard = fmt::format("{}", fmt::join(arguments.begin() + 1, arguments.end(), " "));
            if (wizard.empty())
            {
                if (caller.GetCharacterId() == 0)
                {
                    caller.Reply("Name the wizard by character id or name, since the console plays none");
                    return false;
                }
                wizard = fmt::format("{}", caller.GetCharacterId());
            }
            std::vector<std::shared_ptr<GameSession>> const found = sWorld.FindInWorld(wizard);
            if (found.size() != 1)
            {
                caller.Reply(found.empty() ? fmt::format("No wizard in the world has the character id or name {}", wizard)
                                           : fmt::format("{} wizards in the world are named {}; name one by its character id", found.size(), wizard));
                return false;
            }

            auto const rolled = std::make_shared<std::optional<std::vector<std::vector<LootDrop>>>>();
            bool const ran = sWorld.RunFor(found.front(), [rolled, tables](GameSession& session)
            {
                std::vector<std::vector<LootDrop>> drops;
                for (std::string const& table : tables)
                    drops.push_back(session.RollLoot(table));
                *rolled = std::move(drops);
            }, World::CommandTimeout);
            if (!ran || !*rolled)
            {
                caller.Reply(fmt::format("The world did not answer within {} s", World::CommandTimeout.count()));
                return false;
            }
            for (std::size_t i = 0; i < tables.size(); ++i)
            {
                std::vector<LootDrop> const& drops = (**rolled)[i];
                if (drops.empty())
                {
                    caller.Reply(fmt::format("{} gives {} nothing this roll", tables[i], wizard));
                    continue;
                }
                std::vector<std::string> lines;
                for (LootDrop const& drop : drops)
                    lines.push_back(drop.TemplateId == 0 ? fmt::format("{} {}", drop.Count, LootMgr::TypeName(drop.Type))
                                                         : fmt::format("{} {} of template {}", drop.Count, LootMgr::TypeName(drop.Type), drop.TemplateId));
                caller.Reply(fmt::format("{} would give {}: {}", tables[i], wizard, fmt::join(lines, ", ")));
            }
            return true;
        }
    };
}

void AddSC_cs_loot()
{
    new LootCommands();
}
