/*
 * Project Ambrose by Imjustchico
 * Walks every quest table row once, collecting each quest's goals, logic entries, dialogs and lines first, and reports every row whose quest, goal, logic entry, dialog or line does not exist, every unknown goal type, dialog owner, tag or logic kind, every logic entry that adds goals and completes the quest at once, every bounty goal without an adjective or a tally count, every actor template and persona object_template does not hold when it holds any, every quest whose Start dialog names an actor but which has no Prep line, and every key the locale text lacks when it is loaded.
 */

#include "QuestValidator.h"
#include "QuestStore.h"

#include <fmt/format.h>

#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <utility>

namespace
{
    using GoalKey = std::pair<std::string, std::string>;
    using DialogKey = std::tuple<std::string, std::string, std::string, std::string>;
    using LineKey = std::tuple<std::string, std::string, std::string, std::string, uint32>;

    std::string DialogName(std::string const& owner, std::string const& goal, std::string const& tag)
    {
        return owner == QuestStore::OwnerGoal ? fmt::format("goal {}'s {} dialog", goal, tag) : fmt::format("{} dialog", tag);
    }

    class Checker
    {
    public:
        Checker(QuestRows const& rows, QuestValidator::Context const& context) : _rows(rows), _context(context)
        {
        }

        std::vector<QuestValidationError> Run()
        {
            CollectQuests();
            CollectGoals();
            CheckStartGoals();
            CheckGoalLists();
            CheckLogic();
            CheckDialogs();
            CheckStarters();
            return std::move(_errors);
        }

    private:
        void Report(std::string const& quest, std::string message)
        {
            _errors.push_back(QuestValidationError{ quest, std::move(message) });
        }

        void Key(std::string const& quest, std::string_view where, std::string_view column, std::string const& key)
        {
            if (key.empty() || !_context.HasKey || _context.HasKey(key))
                return;
            Report(quest, fmt::format("quest {}: {} {} {} does not resolve in the locale text", quest, where, column, key));
        }

        bool HasQuest(std::string const& quest) const
        {
            return _quests.contains(quest);
        }

        bool HasGoal(std::string const& quest, std::string const& goal) const
        {
            return _goals.contains(GoalKey{ quest, goal });
        }

        bool NeedQuest(std::string const& quest, std::string_view table)
        {
            if (HasQuest(quest))
                return true;
            Report(quest, fmt::format("{} names quest {}, which quest_template does not hold", table, quest));
            return false;
        }

        void CollectQuests()
        {
            for (QuestTemplateRow const& row : _rows.Quests)
            {
                if (row.Name.empty())
                {
                    Report(row.Name, "quest_template holds a quest with no name");
                    continue;
                }
                if (!_quests.insert(row.Name).second)
                {
                    Report(row.Name, fmt::format("quest_template names quest {} twice", row.Name));
                    continue;
                }
                Key(row.Name, "quest_template", "title_key", row.TitleKey);
                Key(row.Name, "quest_template", "info_key", row.InfoKey);
                Key(row.Name, "quest_template", "prep_key", row.PrepKey);
                Key(row.Name, "quest_template", "underway_key", row.UnderwayKey);
                Key(row.Name, "quest_template", "complete_key", row.CompleteKey);
            }
        }

        void CollectGoals()
        {
            std::map<GoalKey, std::size_t> adjectives;
            for (QuestGoalAdjectiveRow const& row : _rows.Adjectives)
                ++adjectives[GoalKey{ row.Quest, row.Goal }];
            for (QuestGoalRow const& row : _rows.Goals)
            {
                if (!NeedQuest(row.Quest, "quest_goal"))
                    continue;
                if (row.Goal.empty())
                {
                    Report(row.Quest, fmt::format("quest {}: quest_goal holds a goal with no name", row.Quest));
                    continue;
                }
                if (!_goals.insert(GoalKey{ row.Quest, row.Goal }).second)
                {
                    Report(row.Quest, fmt::format("quest {}: quest_goal names goal {} twice", row.Quest, row.Goal));
                    continue;
                }
                std::optional<QuestGoalType> const type = QuestStore::ParseGoalType(row.Type);
                if (!type)
                    Report(row.Quest, fmt::format("quest {}: goal {} has type {}, which is not a goal type", row.Quest, row.Goal, row.Type));
                else if (*type == QuestGoalType::Persona)
                {
                    if (row.PersonaName.empty())
                        Report(row.Quest, fmt::format("quest {}: persona goal {} names no persona", row.Quest, row.Goal));
                    else if (_context.HasObjects() && !_context.ObjectNames.contains(row.PersonaName))
                        Report(row.Quest, fmt::format("quest {}: persona goal {} names {}, which is no object_template object_name", row.Quest, row.Goal, row.PersonaName));
                }
                else if (*type == QuestGoalType::Bounty || *type == QuestGoalType::BountyCollect)
                {
                    if (adjectives[GoalKey{ row.Quest, row.Goal }] == 0)
                        Report(row.Quest, fmt::format("quest {}: bounty goal {} has no adjective in quest_goal_adjective", row.Quest, row.Goal));
                    if (row.TallyCount == 0)
                        Report(row.Quest, fmt::format("quest {}: bounty goal {} has a tally_count of 0", row.Quest, row.Goal));
                }
                std::string const where = fmt::format("goal {}", row.Goal);
                Key(row.Quest, where, "title_key", row.TitleKey);
                Key(row.Quest, where, "underway_key", row.UnderwayKey);
                Key(row.Quest, where, "complete_key", row.CompleteKey);
                Key(row.Quest, where, "location_key", row.LocationKey);
                Key(row.Quest, where, "tally_descriptor_key", row.TallyDescriptorKey);
                Key(row.Quest, where, "tally_descriptor2_key", row.TallyDescriptor2Key);
            }
        }

        void CheckStartGoals()
        {
            for (QuestStartGoalRow const& row : _rows.StartGoals)
                if (NeedQuest(row.Quest, "quest_start_goal") && !HasGoal(row.Quest, row.Goal))
                    Report(row.Quest, fmt::format("quest {}: start goal {} is not a goal of the quest", row.Quest, row.Goal));
        }

        void CheckGoalLists()
        {
            for (QuestGoalAdjectiveRow const& row : _rows.Adjectives)
                if (NeedQuest(row.Quest, "quest_goal_adjective") && !HasGoal(row.Quest, row.Goal))
                    Report(row.Quest, fmt::format("quest {}: quest_goal_adjective names goal {}, which the quest does not have", row.Quest, row.Goal));
            for (QuestGoalClientTagRow const& row : _rows.ClientTags)
                if (NeedQuest(row.Quest, "quest_goal_client_tag") && !HasGoal(row.Quest, row.Goal))
                    Report(row.Quest, fmt::format("quest {}: quest_goal_client_tag names goal {}, which the quest does not have", row.Quest, row.Goal));
        }

        void CheckLogic()
        {
            std::map<std::tuple<std::string, std::string, uint32>, bool> logic;
            for (QuestGoalLogicRow const& row : _rows.Logic)
            {
                if (!NeedQuest(row.Quest, "quest_goal_logic"))
                    continue;
                if (!HasGoal(row.Quest, row.Goal))
                    Report(row.Quest, fmt::format("quest {}: quest_goal_logic names goal {}, which the quest does not have", row.Quest, row.Goal));
                logic[{ row.Quest, row.Goal, row.Position }] = row.CompleteQuest;
            }
            std::set<std::tuple<std::string, std::string, uint32>> adding;
            for (QuestGoalLogicMemberRow const& row : _rows.LogicMembers)
            {
                if (!NeedQuest(row.Quest, "quest_goal_logic_member"))
                    continue;
                auto const entry = logic.find({ row.Quest, row.Goal, row.Logic });
                if (entry == logic.end())
                {
                    Report(row.Quest, fmt::format("quest {}: quest_goal_logic_member names logic entry {} of goal {}, which quest_goal_logic does not hold", row.Quest, row.Logic, row.Goal));
                    continue;
                }
                if (!QuestStore::IsLogicKind(row.Kind))
                {
                    Report(row.Quest, fmt::format("quest {}: goal {} logic entry {} has a member of kind {}, which is not AND, OR or ADD", row.Quest, row.Goal, row.Logic, row.Kind));
                    continue;
                }
                bool const adds = row.Kind == QuestStore::KindAdd;
                if (!HasGoal(row.Quest, row.Member))
                    Report(row.Quest, fmt::format("quest {}: goal {} logic entry {} {} goal {}, which the quest does not have", row.Quest, row.Goal, row.Logic, adds ? "adds (goalsToAdd)" : "requires", row.Member));
                if (adds && entry->second && adding.insert(entry->first).second)
                    Report(row.Quest, fmt::format("quest {}: goal {} logic entry {} both completes the quest (completeQuest) and adds goals (goalsToAdd)", row.Quest, row.Goal, row.Logic));
            }
        }

        void CheckDialogs()
        {
            for (QuestDialogRow const& row : _rows.Dialogs)
            {
                if (!NeedQuest(row.Quest, "quest_dialog"))
                    continue;
                if (!CheckOwner(row.Quest, row.Owner, row.Goal, "quest_dialog"))
                    continue;
                if (!QuestStore::IsDialogTag(row.Tag))
                {
                    Report(row.Quest, fmt::format("quest {}: quest_dialog has tag {}, which is not Start, Prep, Underway, Completion or Complete", row.Quest, row.Tag));
                    continue;
                }
                if (!_dialogs.insert(DialogKey{ row.Quest, row.Owner, row.Goal, row.Tag }).second)
                    Report(row.Quest, fmt::format("quest {}: quest_dialog holds the {} twice", row.Quest, DialogName(row.Owner, row.Goal, row.Tag)));
            }
            for (QuestDialogEntryRow const& row : _rows.DialogEntries)
            {
                if (!NeedQuest(row.Quest, "quest_dialog_entry"))
                    continue;
                std::string const dialog = DialogName(row.Owner, row.Goal, row.Tag);
                if (!_dialogs.contains(DialogKey{ row.Quest, row.Owner, row.Goal, row.Tag }))
                {
                    Report(row.Quest, fmt::format("quest {}: quest_dialog_entry {} belongs to the {}, which quest_dialog does not hold", row.Quest, row.Position, dialog));
                    continue;
                }
                _lines.insert(LineKey{ row.Quest, row.Owner, row.Goal, row.Tag, row.Position });
                if (row.ActorTemplateId != 0 && _context.HasObjects() && !_context.TemplateIds.contains(row.ActorTemplateId))
                    Report(row.Quest, fmt::format("quest {}: line {} of the {} names actor template {}, which object_template does not hold", row.Quest, row.Position, dialog, row.ActorTemplateId));
                Key(row.Quest, fmt::format("line {} of the {}", row.Position, dialog), "dialog_key", row.DialogKey);
                if (row.Owner == QuestStore::OwnerQuest)
                {
                    auto const [first, added] = _firstLines.try_emplace({ row.Quest, row.Tag }, row.Position, row.ActorTemplateId);
                    if (!added && row.Position < first->second.first)
                        first->second = { row.Position, row.ActorTemplateId };
                }
            }
            for (QuestDialogMadlibRow const& row : _rows.Madlibs)
                if (NeedQuest(row.Quest, "quest_dialog_madlib") && !_lines.contains(LineKey{ row.Quest, row.Owner, row.Goal, row.Tag, row.Entry }))
                    Report(row.Quest, fmt::format("quest {}: quest_dialog_madlib {} names line {} of the {}, which quest_dialog_entry does not hold", row.Quest, row.Identifier, row.Entry, DialogName(row.Owner, row.Goal, row.Tag)));
        }

        bool CheckOwner(std::string const& quest, std::string const& owner, std::string const& goal, std::string_view table)
        {
            if (owner == QuestStore::OwnerQuest)
            {
                if (goal.empty())
                    return true;
                Report(quest, fmt::format("quest {}: {} row owned by the quest names goal {}", quest, table, goal));
                return false;
            }
            if (owner != QuestStore::OwnerGoal)
            {
                Report(quest, fmt::format("quest {}: {} has owner {}, which is not quest or goal", quest, table, owner));
                return false;
            }
            if (HasGoal(quest, goal))
                return true;
            Report(quest, fmt::format("quest {}: {} names goal {}, which the quest does not have", quest, table, goal));
            return false;
        }

        void CheckStarters()
        {
            for (std::string const& quest : _quests)
            {
                auto const start = _firstLines.find({ quest, std::string(QuestStore::TagStart) });
                if (start == _firstLines.end() || start->second.second == 0)
                    continue;
                if (!_firstLines.contains({ quest, std::string(QuestStore::TagPrep) }))
                    Report(quest, fmt::format("quest {}: its Start dialog opens with actor template {}, so it has a starter, but it has no Prep dialog line", quest, start->second.second));
            }
        }

        QuestRows const& _rows;
        QuestValidator::Context const& _context;
        std::vector<QuestValidationError> _errors;
        std::set<std::string> _quests;
        std::set<GoalKey> _goals;
        std::set<DialogKey> _dialogs;
        std::set<LineKey> _lines;
        std::map<std::pair<std::string, std::string>, std::pair<uint32, uint32>> _firstLines;
    };
}

QuestValidator::Context QuestValidator::Context::From(std::vector<QuestObjectRow> const& objects, KeyLookup hasKey)
{
    Context context;
    for (QuestObjectRow const& object : objects)
    {
        context.TemplateIds.insert(object.TemplateId);
        if (!object.ObjectName.empty())
            context.ObjectNames.insert(object.ObjectName);
    }
    context.HasKey = std::move(hasKey);
    return context;
}

std::vector<QuestValidationError> QuestValidator::Validate(QuestRows const& rows, Context const& context)
{
    return Checker(rows, context).Run();
}
