/*
 * Project Ambrose by Imjustchico
 * The effect group: 'effect info <name or id>' describes a game effect template the server holds and lists the names holding the text given when none is exact; 'effect add <effect> <wizard>' puts the effect a template makes on a wizard in the world, which the wizard and everyone who sees it are shown at once; 'effect remove <internal id> <wizard>' takes one away by the internal id 'effect list <wizard>' gives; and 'effect reload' reads every template again from the install. A change runs on the world thread, where the effects are kept, and is answered once it has; a name two wizards share names neither, and the ids to choose between are listed instead.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "GameEffectMgr.h"
#include "GameSession.h"
#include "ReloadMgr.h"
#include "ScriptMgr.h"
#include "StringUtil.h"
#include "World.h"

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

    struct Target
    {
        std::shared_ptr<GameSession> Session;
        std::string Name;
    };

    struct Listed
    {
        int32 InternalId = 0;
        uint32 EffectNameId = 0;
        std::string Class;
    };

    std::optional<Target> FindWizard(CommandCaller& caller, std::vector<std::string>::const_iterator begin, std::vector<std::string>::const_iterator end)
    {
        std::string const wizard = fmt::format("{}", fmt::join(begin, end, " "));
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

    std::string NameOf(uint32 effectNameId)
    {
        GameEffectInfo const* const effect = sGameEffectMgr.GetEffects()->Find(effectNameId);
        return effect != nullptr ? effect->Name : fmt::format("effect {}", effectNameId);
    }

    class EffectCommands : public CommandScript
    {
    public:
        EffectCommands() : CommandScript("cs_effect") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "effect", .SecurityLevel = SEC_GAMEMASTER, .Help = "the game effects a wizard carries and the templates they are made from", .Children = {
                    { .Name = "info", .SecurityLevel = SEC_GAMEMASTER, .Help = "one game effect template, found by its name or template id", .Run = Info },
                    { .Name = "add", .SecurityLevel = SEC_GAMEMASTER, .Help = "put an effect on a wizard in the world, shown to it and everyone who sees it", .Run = Add },
                    { .Name = "remove", .SecurityLevel = SEC_GAMEMASTER, .Help = "take an effect off a wizard by its internal id", .Run = Remove },
                    { .Name = "list", .SecurityLevel = SEC_GAMEMASTER, .Help = "the effects a wizard carries, with their internal ids", .Run = List },
                    { .Name = "reload", .SecurityLevel = SEC_ADMINISTRATOR, .Help = "read every game effect template again from the install", .Run = Reload },
                } },
            };
        }

    private:
        static GameEffectInfo const* FindEffect(CommandCaller& caller, std::string const& wanted)
        {
            std::shared_ptr<GameEffectStore const> const effects = sGameEffectMgr.GetEffects();
            if (effects->Size() == 0)
            {
                caller.Reply("No game effect templates are loaded");
                return nullptr;
            }
            GameEffectInfo const* const effect = effects->FindByIdOrName(wanted);
            if (effect != nullptr)
                return effect;
            std::vector<GameEffectInfo const*> const matches = effects->Search(wanted);
            if (matches.empty())
            {
                caller.Reply(fmt::format("No game effect is named {} or holds it in its name", wanted));
                return nullptr;
            }
            caller.Reply(fmt::format("No game effect is named {}, but {} hold(s) it in their names:", wanted, matches.size()));
            for (std::size_t index = 0; index < std::min(matches.size(), ListedMatches); ++index)
                caller.Reply(fmt::format("  {}, template {}", matches[index]->Name, matches[index]->TemplateId));
            if (matches.size() > ListedMatches)
                caller.Reply(fmt::format("  and {} more", matches.size() - ListedMatches));
            return nullptr;
        }

        static bool Info(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.empty())
            {
                caller.Reply("effect info takes a game effect's name or template id, such as PostCombatEffect");
                return false;
            }
            GameEffectInfo const* const effect = FindEffect(caller, fmt::format("{}", fmt::join(arguments, " ")));
            if (effect == nullptr)
                return false;
            for (std::string const& line : effect->Describe())
                caller.Reply(line);
            return true;
        }

        static bool Add(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.size() < 2)
            {
                caller.Reply("effect add takes a game effect, by template id or name, and then the wizard, by character id or name, such as effect add PostCombatEffect 555");
                return false;
            }
            GameEffectInfo const* const effect = FindEffect(caller, arguments.front());
            if (effect == nullptr)
                return false;
            std::optional<Target> const target = FindWizard(caller, arguments.begin() + 1, arguments.end());
            if (!target)
                return false;
            std::string problem;
            PropertyObjectPtr made = effect->MakeEffect(problem);
            if (!made)
            {
                caller.Reply(fmt::format("{} makes no effect: {}", effect->Name, problem));
                return false;
            }
            auto const shared = std::make_shared<PropertyObjectPtr>(std::move(made));
            auto const added = std::make_shared<std::optional<int32>>();
            auto const why = std::make_shared<std::string>();
            bool const ran = sWorld.RunFor(target->Session, [shared, added, why](GameSession& wizard) { *added = wizard.AddGameEffect(std::move(*shared), *why); },
                World::CommandTimeout);
            if (!ran)
            {
                caller.Reply(fmt::format("The world did not answer within {} s, so {} may yet carry {}", World::CommandTimeout.count(), target->Name, effect->Name));
                return false;
            }
            if (!*added)
            {
                caller.Reply(fmt::format("{} cannot carry {}: {}", target->Name, effect->Name, *why));
                return false;
            }
            caller.Reply(fmt::format("{} carries {} (template {}) as internal id {}, shown to it and everyone who sees it", target->Name, effect->Name, effect->TemplateId, **added));
            return true;
        }

        static bool Remove(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            std::optional<int32> const id = arguments.size() < 2 ? std::nullopt : Ambrose::StringTo<int32>(arguments.front());
            if (!id)
            {
                caller.Reply("effect remove takes the internal id effect list gives and then the wizard, by character id or name, such as effect remove 1 555");
                return false;
            }
            std::optional<Target> const target = FindWizard(caller, arguments.begin() + 1, arguments.end());
            if (!target)
                return false;
            auto const removed = std::make_shared<std::optional<ActiveGameEffect>>();
            bool const ran = sWorld.RunFor(target->Session, [removed, id](GameSession& wizard) { *removed = wizard.RemoveGameEffect(*id); }, World::CommandTimeout);
            if (!ran)
            {
                caller.Reply(fmt::format("The world did not answer within {} s, so {} may yet lose effect {}", World::CommandTimeout.count(), target->Name, *id));
                return false;
            }
            if (!*removed)
            {
                caller.Reply(fmt::format("{} carries no effect with internal id {}", target->Name, *id));
                return false;
            }
            caller.Reply(fmt::format("{} no longer carries {} (internal id {})", target->Name, NameOf((*removed)->EffectNameId), *id));
            return true;
        }

        static bool List(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.empty())
            {
                caller.Reply("effect list takes the wizard, by character id or name");
                return false;
            }
            std::optional<Target> const target = FindWizard(caller, arguments.begin(), arguments.end());
            if (!target)
                return false;
            auto const listed = std::make_shared<std::vector<Listed>>();
            bool const ran = sWorld.RunFor(target->Session, [listed](GameSession& wizard)
            {
                for (ActiveGameEffect const& active : wizard.GetGameEffects().GetEffects())
                    listed->push_back(Listed{ active.InternalId, active.EffectNameId, active.Effect->GetClass().Name });
            }, World::CommandTimeout);
            if (!ran)
            {
                caller.Reply(fmt::format("The world did not answer within {} s", World::CommandTimeout.count()));
                return false;
            }
            if (listed->empty())
            {
                caller.Reply(fmt::format("{} carries no effect", target->Name));
                return true;
            }
            caller.Reply(fmt::format("{} carries {} effect(s):", target->Name, listed->size()));
            for (Listed const& effect : *listed)
                caller.Reply(fmt::format("  {}: {}, a {}", effect.InternalId, NameOf(effect.EffectNameId), effect.Class));
            return true;
        }

        static bool Reload(CommandCaller& caller, std::vector<std::string> const&)
        {
            for (std::string const& line : ReloadMgr::Describe(sReloadMgr.Reload(GameEffectMgr::Target)))
                caller.Reply(line);
            return true;
        }
    };
}

void AddSC_cs_effect()
{
    new EffectCommands();
}
