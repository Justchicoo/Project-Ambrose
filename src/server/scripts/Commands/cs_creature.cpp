/*
 * Project Ambrose by Imjustchico
 * The creature group: 'creature deck info <name or id>' lists a creature deck the server holds, each spell it carries with its copies and the deck's card total, found by id or by its name whatever its case; 'creature deck reload' reads every creature deck again from the world database, keeping the set serving when the new one fails and naming every failure.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "CreatureDeckMgr.h"
#include "ReloadMgr.h"
#include "ScriptMgr.h"
#include "StringUtil.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    class CreatureCommands : public CommandScript
    {
    public:
        CreatureCommands() : CommandScript("cs_creature") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "creature", .SecurityLevel = SEC_GAMEMASTER, .Help = "the creatures this server fights with", .Children = {
                    { .Name = "deck", .SecurityLevel = SEC_GAMEMASTER, .Help = "the decks creatures draw their cards from", .Children = {
                        { .Name = "info", .SecurityLevel = SEC_GAMEMASTER, .Help = "the spells one deck holds, found by its name or id", .Run = Info },
                        { .Name = "reload", .SecurityLevel = SEC_ADMINISTRATOR, .Help = "read every creature deck again from the world database", .Run = Reload },
                    } },
                } },
            };
        }

    private:
        static bool Info(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.empty())
            {
                caller.Reply("creature deck info takes a deck's name or id");
                return false;
            }
            std::shared_ptr<CreatureDeckSet const> const decks = sCreatureDeckMgr.GetDecks();
            std::string const wanted = fmt::format("{}", fmt::join(arguments, " "));
            CreatureDeck const* deck = decks->FindByName(wanted);
            if (deck == nullptr)
                if (std::optional<uint32> const id = Ambrose::StringTo<uint32>(wanted))
                    deck = decks->Find(*id);
            if (deck == nullptr)
            {
                caller.Reply(fmt::format("No creature deck is named {}, among the {} loaded", wanted, decks->Size()));
                return false;
            }
            caller.Reply(fmt::format("{} (deck {}) holds {} cards of {} spells:", deck->Name, deck->Id, deck->GetCardCount(), deck->Cards.size()));
            for (CreatureDeckCard const& card : deck->Cards)
                caller.Reply(fmt::format("  {} x{}, template {}", card.Spell, card.Count, card.SpellId));
            return true;
        }

        static bool Reload(CommandCaller& caller, std::vector<std::string> const&)
        {
            for (std::string const& line : ReloadMgr::Describe(sReloadMgr.Reload(CreatureDeckMgr::Target)))
                caller.Reply(line);
            return true;
        }
    };
}

void AddSC_cs_creature()
{
    new CreatureCommands();
}
