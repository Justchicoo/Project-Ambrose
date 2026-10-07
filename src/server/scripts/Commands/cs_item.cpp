/*
 * Project Ambrose by Imjustchico
 * Changes a wizard's backpack while it plays: 'additem <template> [count] [wizard]' gives items of a template named by its id or its name, in quotes when it holds spaces, one at a time until the count is given or the backpack is full, and 'removeitem <item id> [wizard]' takes one item away by its global id; a game master in game changes its own wizard's backpack when it names none, and the console must name one by character id or name. The change runs on the world thread, where the backpack is kept, and is answered once it has; a name two wizards share names neither, and the ids to choose between are listed instead.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "GameSession.h"
#include "ItemMgr.h"
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
    constexpr uint32 MaxItemsPerCommand = 100;

    struct Target
    {
        std::shared_ptr<GameSession> Session;
        std::string Name;
    };

    std::optional<Target> FindWizard(CommandCaller& caller, std::vector<std::string>::const_iterator first, std::vector<std::string>::const_iterator last)
    {
        std::string wizard = fmt::format("{}", fmt::join(first, last, " "));
        if (wizard.empty())
        {
            if (caller.GetCharacterId() == 0)
            {
                caller.Reply("Name the wizard by character id or name, since the console plays none");
                return std::nullopt;
            }
            wizard = fmt::format("{}", caller.GetCharacterId());
        }
        std::vector<std::shared_ptr<GameSession>> const found = sWorld.FindInWorld(wizard);
        if (found.empty())
        {
            caller.Reply(fmt::format("No wizard in the world has the character id or name {}", wizard));
            return std::nullopt;
        }
        if (found.size() > 1)
        {
            caller.Reply(fmt::format("{} wizards in the world are named {}; name one by its character id:", found.size(), wizard));
            for (std::shared_ptr<GameSession> const& session : found)
                caller.Reply(fmt::format("  {} on session {}", session->GetCharacterId(), session->GetSessionId()));
            return std::nullopt;
        }
        std::string name = found.front()->GetCharacterName();
        if (name.empty())
            name = fmt::format("wizard {}", found.front()->GetCharacterId());
        return Target{ found.front(), std::move(name) };
    }

    class ItemCommands : public CommandScript
    {
    public:
        ItemCommands() : CommandScript("cs_item") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "additem", .SecurityLevel = SEC_GAMEMASTER, .Help = "give a wizard items of a template, which its backpack shows at once and keeps: additem <template> [count] [wizard]",
                    .Run = AddItem },
                { .Name = "removeitem", .SecurityLevel = SEC_GAMEMASTER, .Help = "take an item out of a wizard's backpack by its id: removeitem <item id> [wizard]", .Run = RemoveItem },
            };
        }

    private:
        static bool AddItem(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.empty())
            {
                caller.Reply("additem takes a template, by id or by name in quotes, then how many and the wizard, such as additem \"Apprentice Hat\" 1 555");
                return false;
            }
            std::shared_ptr<ItemTemplateStore const> const items = sItemMgr.GetItems();
            ItemTemplateRecord const* const found = items ? items->FindByIdOrName(arguments.front()) : nullptr;
            if (found == nullptr)
            {
                caller.Reply(fmt::format("No item has the template id or name {}", arguments.front()));
                return false;
            }
            uint32 count = 1;
            auto wizard = arguments.begin() + 1;
            if (wizard != arguments.end())
            {
                std::optional<uint32> const asked = Ambrose::StringTo<uint32>(*wizard);
                if (!asked || *asked == 0 || *asked > MaxItemsPerCommand)
                {
                    caller.Reply(fmt::format("The count must be a whole number from 1 to {}", MaxItemsPerCommand));
                    return false;
                }
                count = *asked;
                ++wizard;
            }
            std::optional<Target> const target = FindWizard(caller, wizard, arguments.end());
            if (!target)
                return false;

            ItemTemplateRecord const itemTemplate = *found;
            auto const outcome = std::make_shared<std::optional<std::pair<uint32, BackpackAddResult>>>();
            bool const ran = sWorld.RunFor(target->Session, [outcome, itemTemplate, count](GameSession& session)
            {
                uint32 given = 0;
                BackpackAddResult last = BackpackAddResult::Added;
                while (given < count && last == BackpackAddResult::Added)
                {
                    last = session.AddItem(itemTemplate, 1).Result;
                    if (last == BackpackAddResult::Added)
                        ++given;
                }
                *outcome = std::make_pair(given, last);
            }, World::CommandTimeout);
            if (!ran || !*outcome)
            {
                caller.Reply(fmt::format("The world did not answer within {} s, so {} may yet be given {}", World::CommandTimeout.count(), target->Name, itemTemplate.ObjectName));
                return false;
            }
            auto const [given, last] = **outcome;
            if (given == count)
            {
                caller.Reply(fmt::format("{} was given {} {} (template {}), which its backpack shows now and keeps", target->Name, given, itemTemplate.ObjectName, itemTemplate.TemplateId));
                return true;
            }
            switch (last)
            {
                case BackpackAddResult::Full:
                    caller.Reply(fmt::format("{} was given {} of {} {}, since its backpack is full", target->Name, given, count, itemTemplate.ObjectName));
                    break;
                case BackpackAddResult::NoItemId:
                    caller.Reply(fmt::format("{} was given {} of {} {}, since no item id is left", target->Name, given, count, itemTemplate.ObjectName));
                    break;
                default:
                    caller.Reply(fmt::format("{} left the world before it could be given {}", target->Name, itemTemplate.ObjectName));
                    break;
            }
            return given > 0;
        }

        static bool RemoveItem(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            std::optional<uint64> const itemGuid = arguments.empty() ? std::nullopt : Ambrose::StringTo<uint64>(arguments.front());
            if (!itemGuid)
            {
                caller.Reply("removeitem takes the item's id, then the wizard, such as removeitem 281474976710657 555");
                return false;
            }
            std::optional<Target> const target = FindWizard(caller, arguments.begin() + 1, arguments.end());
            if (!target)
                return false;
            auto const removed = std::make_shared<std::optional<std::optional<CharacterItem>>>();
            bool const ran = sWorld.RunFor(target->Session, [removed, guid = *itemGuid](GameSession& session)
            {
                *removed = session.RemoveItem(guid);
            }, World::CommandTimeout);
            if (!ran || !*removed)
            {
                caller.Reply(fmt::format("The world did not answer within {} s, so {} may yet lose item {}", World::CommandTimeout.count(), target->Name, *itemGuid));
                return false;
            }
            if (!**removed)
            {
                caller.Reply(fmt::format("{} holds no item {}", target->Name, *itemGuid));
                return false;
            }
            caller.Reply(fmt::format("{} no longer holds item {} of template {}", target->Name, *itemGuid, (**removed)->TemplateId));
            return true;
        }
    };
}

void AddSC_cs_item()
{
    new ItemCommands();
}
