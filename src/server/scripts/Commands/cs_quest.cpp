/*
 * Project Ambrose by Imjustchico
 * The quest group, for testing what a wizard's progress changes: 'quest add <quest>' gives the caller's wizard a quest with its start goals, 'quest goal <quest> <goal>' completes one of its goals, starting it first if the wizard lacks it, 'quest complete <quest>' finishes it, leaving 'Complete' = 1 in its registry, 'quest remove <quest>' drops it with nothing left behind, 'quest entry <entry> <value> [quest]' sets a registry entry, a quest's own when one is named, and 'quest list' shows the quests and goals the wizard holds. A quest the server holds no template for is given all the same, with a warning that loading the character again drops it, because spawn requirements name quests the world database may not hold yet. A change runs on the world thread, where the quest log is kept, is saved at once, and shows the wizard the objects its new progress lets it see at the next tick.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "CommandCaller.h"
#include "GameSession.h"
#include "ObjectGuid.h"
#include "QuestMgr.h"
#include "QuestStore.h"
#include "ScriptMgr.h"
#include "StringUtil.h"
#include "World.h"

#include <fmt/format.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    struct Outcome
    {
        bool Done = false;
        std::vector<std::string> Lines;
    };

    std::shared_ptr<GameSession> CallerWizard(CommandCaller& caller)
    {
        if (caller.GetCharacterId() == 0)
        {
            caller.Reply("This command changes your own wizard's quests, so it is typed in game");
            return nullptr;
        }
        std::shared_ptr<GameSession> const session = sWorld.FindSessionByCharacterId(caller.GetCharacterId());
        if (!session)
            caller.Reply("Your wizard is not in the world");
        return session;
    }

    uint64 Now()
    {
        return static_cast<uint64>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    }

    template<typename Change>
    bool Run(CommandCaller& caller, Change change)
    {
        std::shared_ptr<GameSession> const wizard = CallerWizard(caller);
        if (!wizard)
            return false;
        auto const outcome = std::make_shared<Outcome>();
        bool const ran = sWorld.RunFor(wizard, [outcome, change](GameSession& session)
        {
            QuestLog* const log = session.GetQuestLog();
            if (!log)
            {
                outcome->Lines.emplace_back("Your wizard has no quest log loaded");
                return;
            }
            change(*log, *outcome);
            if (outcome->Done)
                session.SaveQuestLogIfDirty();
        }, World::CommandTimeout);
        if (!ran)
        {
            caller.Reply(fmt::format("The world did not answer within {} s, so the change may yet be made", World::CommandTimeout.count()));
            return false;
        }
        for (std::string const& line : outcome->Lines)
            caller.Reply(line);
        return outcome->Done;
    }

    std::string TemplateNote(std::string const& quest)
    {
        std::shared_ptr<QuestStore const> const quests = sQuestMgr.GetQuests();
        if (quests && quests->Find(quest))
            return std::string();
        return fmt::format("; the server holds no template for {}, so loading the character again drops it", quest);
    }

    class QuestCommands : public CommandScript
    {
    public:
        QuestCommands() : CommandScript("cs_quest") {}

        std::vector<ChatCommand> GetCommands() const override
        {
            return {
                { .Name = "quest", .SecurityLevel = SEC_GAMEMASTER, .Help = "the quests your own wizard holds, to test what its progress changes", .Children = {
                    { .Name = "add", .SecurityLevel = SEC_GAMEMASTER, .Help = "give your wizard a quest with its start goals", .Run = Add },
                    { .Name = "goal", .SecurityLevel = SEC_GAMEMASTER, .Help = "complete one of a quest's goals", .Run = Goal },
                    { .Name = "complete", .SecurityLevel = SEC_GAMEMASTER, .Help = "finish a quest, leaving Complete = 1 in its registry", .Run = Complete },
                    { .Name = "remove", .SecurityLevel = SEC_GAMEMASTER, .Help = "drop a quest with nothing left behind", .Run = Remove },
                    { .Name = "entry", .SecurityLevel = SEC_GAMEMASTER, .Help = "set a registry entry, a quest's own when one is named", .Run = Entry },
                    { .Name = "list", .SecurityLevel = SEC_GAMEMASTER, .Help = "the quests and goals your wizard holds", .Run = List },
                } },
            };
        }

    private:
        static bool Add(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.size() != 1)
            {
                caller.Reply("quest add takes the quest's name, such as WC-MAIN-C01-001");
                return false;
            }
            std::string const quest = arguments[0];
            std::shared_ptr<QuestStore const> const quests = sQuestMgr.GetQuests();
            QuestInfo const* const info = quests ? quests->Find(quest) : nullptr;
            std::vector<std::string> const goals = info ? info->StartGoals : std::vector<std::string>();
            std::string const note = TemplateNote(quest);
            return Run(caller, [quest, goals, note](QuestLog& log, Outcome& outcome)
            {
                if (!log.Add(quest, ObjectGuid::QuestGuids(), Now()))
                {
                    outcome.Lines.push_back(fmt::format("Your wizard already holds {}, or no quest id is left", quest));
                    return;
                }
                for (std::string const& goal : goals)
                    log.StartGoal(quest, goal, ObjectGuid::QuestGuids());
                outcome.Done = true;
                outcome.Lines.push_back(fmt::format("Your wizard holds {} with {} start goal(s){}", quest, goals.size(), note));
            });
        }

        static bool Goal(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.size() != 2)
            {
                caller.Reply("quest goal takes the quest's name and then the goal's");
                return false;
            }
            std::string const quest = arguments[0];
            std::string const goal = arguments[1];
            return Run(caller, [quest, goal](QuestLog& log, Outcome& outcome)
            {
                if (!log.HasQuest(quest))
                {
                    outcome.Lines.push_back(fmt::format("Your wizard does not hold {}; give it with quest add first", quest));
                    return;
                }
                if (!log.IsGoalActive(quest, goal) && !log.IsGoalCompleted(quest, goal))
                    log.StartGoal(quest, goal, ObjectGuid::QuestGuids());
                if (!log.CompleteGoal(quest, goal))
                {
                    outcome.Lines.push_back(fmt::format("Goal {} of {} is already complete", goal, quest));
                    return;
                }
                outcome.Done = true;
                outcome.Lines.push_back(fmt::format("Goal {} of {} is complete", goal, quest));
            });
        }

        static bool Complete(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.size() != 1)
            {
                caller.Reply("quest complete takes the quest's name");
                return false;
            }
            std::string const quest = arguments[0];
            return Run(caller, [quest](QuestLog& log, Outcome& outcome)
            {
                if (!log.CompleteQuest(quest))
                {
                    outcome.Lines.push_back(fmt::format("Your wizard does not hold {}", quest));
                    return;
                }
                outcome.Done = true;
                outcome.Lines.push_back(fmt::format("{} is complete", quest));
            });
        }

        static bool Remove(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            if (arguments.size() != 1)
            {
                caller.Reply("quest remove takes the quest's name");
                return false;
            }
            std::string const quest = arguments[0];
            return Run(caller, [quest](QuestLog& log, Outcome& outcome)
            {
                if (!log.Remove(quest))
                {
                    outcome.Lines.push_back(fmt::format("Your wizard does not hold {}", quest));
                    return;
                }
                outcome.Done = true;
                outcome.Lines.push_back(fmt::format("{} is dropped", quest));
            });
        }

        static bool Entry(CommandCaller& caller, std::vector<std::string> const& arguments)
        {
            std::optional<double> const value = arguments.size() < 2 || arguments.size() > 3 ? std::nullopt : Ambrose::StringTo<double>(arguments[1]);
            if (!value)
            {
                caller.Reply("quest entry takes the entry's name, its value and, for a quest's own registry, the quest's name");
                return false;
            }
            std::string const entry = arguments[0];
            std::string const quest = arguments.size() == 3 ? arguments[2] : std::string();
            double const set = *value;
            return Run(caller, [entry, quest, set](QuestLog& log, Outcome& outcome)
            {
                if (quest.empty())
                    log.SetRegistry(entry, set);
                else
                    log.SetQuestRegistry(quest, entry, set);
                outcome.Done = true;
                outcome.Lines.push_back(quest.empty() ? fmt::format("Registry entry {} is {}", entry, set) : fmt::format("{}'s registry entry {} is {}", quest, entry, set));
            });
        }

        static bool List(CommandCaller& caller, std::vector<std::string> const&)
        {
            return Run(caller, [](QuestLog& log, Outcome& outcome)
            {
                outcome.Done = true;
                if (log.GetQuests().empty())
                {
                    outcome.Lines.emplace_back("Your wizard holds no quest");
                    return;
                }
                for (QuestLogQuest const& quest : log.GetQuests())
                {
                    outcome.Lines.push_back(quest.Name);
                    for (QuestLogGoal const& goal : quest.Goals)
                        outcome.Lines.push_back(fmt::format("  {}{}", goal.Name, goal.Complete ? " (complete)" : ""));
                }
            });
        }
    };
}

void AddSC_cs_quest()
{
    new QuestCommands();
}
