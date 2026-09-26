/*
 * Project Ambrose by Imjustchico
 * Changes a wizard's spellbook while it plays: 'learn <spell> <wizard>' teaches a spell, named by its template id or its name, in quotes when it holds spaces, to a wizard in the world named by its character id or its name, and 'unlearn <spell> <wizard>' takes one back, which a spell no longer in the install may also be by its id. The change runs on the world thread, where the spellbook is kept, and is answered once it has, so the reply says whether the wizard learned the spell or already knew it; a name two wizards share names neither, and the ids to choose between are listed instead.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "GameSession.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
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
    struct Target
    {
        std::shared_ptr<GameSession> Session;
        std::string Name;
    };

    std::optional<Target> FindWizard(CommandCaller& caller, std::string_view command, std::vector<std::string> const& arguments)
    {
        if (arguments.size() < 2)
        {
            caller.Reply(fmt::format("{} takes a spell, by template id or by name in quotes, and then the wizard, by character id or name, such as {} \"Fire Cat\" 555", command, command));
            return std::nullopt;
        }
        std::string const wizard = fmt::format("{}", fmt::join(arguments.begin() + 1, arguments.end(), " "));
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

    std::optional<SpellbookChange> Change(std::shared_ptr<GameSession> const& session, uint32 spellId, bool learn)
    {
        auto const change = std::make_shared<std::optional<SpellbookChange>>();
        bool const ran = sWorld.RunFor(session, [change, spellId, learn](GameSession& wizard)
        {
            *change = learn ? wizard.LearnSpell(spellId) : wizard.UnlearnSpell(spellId);
        }, World::CommandTimeout);
        return ran ? *change : std::nullopt;
    }

    class LearnCommands : public CommandScript
    {
    public:
        LearnCommands() : CommandScript("cs_learn") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "learn", .SecurityLevel = SEC_GAMEMASTER, .Help = "teach a wizard in the world a spell, which its spellbook shows at once and keeps", .Run = Learn },
                { .Name = "unlearn", .SecurityLevel = SEC_GAMEMASTER, .Help = "take a spell out of a wizard's spellbook", .Run = Unlearn },
            };
        }

    private:
        static bool Learn(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            std::optional<Target> const target = FindWizard(caller, "learn", arguments);
            if (!target)
                return false;
            std::shared_ptr<SpellStore const> const spells = sSpellMgr.GetSpells();
            SpellInfo const* const spell = spells->FindByIdOrName(arguments.front());
            if (spell == nullptr)
            {
                caller.Reply(fmt::format("No spell has the template id or name {}; spell info {} lists the names that hold it", arguments.front(), arguments.front()));
                return false;
            }
            std::optional<SpellbookChange> const change = Change(target->Session, spell->TemplateId, true);
            if (!change)
            {
                caller.Reply(fmt::format("The world did not answer within {} s, so {} may yet learn {}", World::CommandTimeout.count(), target->Name, spell->Name));
                return false;
            }
            switch (*change)
            {
                case SpellbookChange::Learned:
                    caller.Reply(fmt::format("{} learned {} (template {}), which its spellbook shows now and keeps", target->Name, spell->Name, spell->TemplateId));
                    return true;
                case SpellbookChange::AlreadyKnown:
                    caller.Reply(fmt::format("{} already knows {}", target->Name, spell->Name));
                    return false;
                case SpellbookChange::NoSuchSpell:
                    caller.Reply(fmt::format("The spells were read again without {} before {} could learn it", spell->Name, target->Name));
                    return false;
                default:
                    caller.Reply(fmt::format("{} left the world before it could learn {}", target->Name, spell->Name));
                    return false;
            }
        }

        static bool Unlearn(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            std::optional<Target> const target = FindWizard(caller, "unlearn", arguments);
            if (!target)
                return false;
            std::shared_ptr<SpellStore const> const spells = sSpellMgr.GetSpells();
            SpellInfo const* const spell = spells->FindByIdOrName(arguments.front());
            std::optional<uint32> const id = spell != nullptr ? std::optional<uint32>(spell->TemplateId) : Ambrose::StringTo<uint32>(arguments.front());
            if (!id)
            {
                caller.Reply(fmt::format("No spell has the template id or name {}; spell info {} lists the names that hold it", arguments.front(), arguments.front()));
                return false;
            }
            std::string const named = spell != nullptr ? spell->Name : fmt::format("spell {}", *id);
            std::optional<SpellbookChange> const change = Change(target->Session, *id, false);
            if (!change)
            {
                caller.Reply(fmt::format("The world did not answer within {} s, so {} may yet lose {}", World::CommandTimeout.count(), target->Name, named));
                return false;
            }
            switch (*change)
            {
                case SpellbookChange::Unlearned:
                    caller.Reply(fmt::format("{} no longer knows {}", target->Name, named));
                    return true;
                case SpellbookChange::NotKnown:
                    caller.Reply(fmt::format("{} does not know {}", target->Name, named));
                    return false;
                default:
                    caller.Reply(fmt::format("{} left the world before it could lose {}", target->Name, named));
                    return false;
            }
        }
    };
}

void AddSC_cs_learn()
{
    new LearnCommands();
}
