/*
 * Project Ambrose by Imjustchico
 * Reads a CSR package's QuestData plain-XML quest templates by property name into quest table rows, its m_requirements and each goal's m_goalRequirements encoded as zone data holds requirements and failing the quest when any of those lists does not read, attaching each goal-complete logic entry, with the count of its OR goals it needs, to every goal it waits on, keeping the dialog tags the tables hold and counting the rest, then checks each quest against the install, the names other sources hold and the quest validator, and writes the clean ones marked with the package as their source.
 */

#include "QuestPackage.h"
#include "KiwadArchive.h"
#include "ObjectSerializer.h"
#include "PropertyObject.h"
#include "QuestStore.h"
#include "Utf.h"
#include "XmlObjectReader.h"
#include "ZoneExtractor.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <tuple>
#include <utility>

namespace
{
    constexpr std::size_t NameColumn = 128;
    constexpr std::size_t TextColumn = 255;
    constexpr std::string_view QuestClass = "class QuestTemplate";
    constexpr std::array NotCarriedProperties{ std::string_view("m_startResults"), std::string_view("m_endResults"), std::string_view("m_completeResults"),
        std::string_view("m_activateResults"), std::string_view("m_tallyResults"), std::string_view("m_prepRequirements"),
        std::string_view("m_pruneRequirements"), std::string_view("m_goalRequirements"), std::string_view("m_dynaMods"), std::string_view("m_questEffectInfoList"),
        std::string_view("m_behaviors") };
    constexpr std::array<std::string_view, 67> ReadProperties{ "m_questName", "m_questNameID", "m_questTitle", "m_questInfo", "m_questPrep", "m_questUnderway", "m_questComplete",
        "m_questLevel", "m_questRepeat", "m_mainline", "m_noQuestHelper", "m_skipQHAutoSelect", "m_activityType", "m_prepAlways", "m_isHidden", "m_startGoals", "m_dialogList", "m_goals",
        "m_goalLogic", "m_goalName", "m_goalNameID", "m_goalTitle", "m_goalUnderway", "m_completeText", "m_locationName", "m_destinationZone", "m_displayImage1", "m_displayImage2",
        "m_personaName", "m_usePatron", "m_zoneEntry", "m_zoneExit", "m_zoneTag", "m_proximityTag", "m_rank", "m_schoolName", "m_hideGoalFloatyText", "m_tallyCounter", "m_count", "m_percentChance",
        "m_descriptor", "m_descriptor2", "m_bountyTotal", "m_itemTotal", "m_goalType", "m_npcAdjectives", "m_itemAdjectives", "m_clientTags", "m_goalsAND", "m_goalsOR", "m_goalsToAdd",
        "m_completeQuest", "m_dialogs", "m_dialogTag", "m_dialogEntries", "m_actorTemplateID", "m_dialog", "m_picture", "m_soundFile", "m_action", "m_dialogEvent",
        "m_dialogAnimationList", "m_madlibs", "m_index", "m_madlibBlock", "m_madlibToken", "m_madlibArgument" };

    std::string_view ShortName(std::string_view name)
    {
        for (std::string_view const prefix : { std::string_view("class "), std::string_view("struct "), std::string_view("enum ") })
            if (name.starts_with(prefix))
                return name.substr(prefix.size());
        return name;
    }

    std::optional<int64> WholeOf(PropertyValue const* value)
    {
        if (!value)
            return std::nullopt;
        if (auto const* held = value->GetIf<bool>()) return *held ? 1 : 0;
        if (auto const* held = value->GetIf<int8>()) return *held;
        if (auto const* held = value->GetIf<uint8>()) return *held;
        if (auto const* held = value->GetIf<int16>()) return *held;
        if (auto const* held = value->GetIf<uint16>()) return *held;
        if (auto const* held = value->GetIf<int32>()) return *held;
        if (auto const* held = value->GetIf<uint32>()) return *held;
        if (auto const* held = value->GetIf<int64>()) return *held;
        if (auto const* held = value->GetIf<uint64>()) return static_cast<int64>(*held);
        return std::nullopt;
    }

    std::string TextOf(PropertyValue const& value)
    {
        if (auto const* held = value.GetIf<std::string>())
            return *held;
        if (auto const* held = value.GetIf<std::u16string>())
            return Utf::Utf16ToUtf8(*held, Utf::InvalidPolicy::ReplaceWithU_FFFD).value_or(std::string());
        if (auto const* held = value.GetIf<float>())
            return fmt::format("{}", *held);
        if (auto const* held = value.GetIf<double>())
            return fmt::format("{}", *held);
        if (std::optional<int64> const whole = WholeOf(&value))
            return fmt::format("{}", *whole);
        return {};
    }

    class Fields
    {
    public:
        explicit Fields(PropertyObject const& object) : _object(object) { }

        std::string Text(std::string_view name) const
        {
            PropertyValue const* const value = _object.Get(name);
            return value ? TextOf(*value) : std::string();
        }

        int64 Whole(std::string_view name) const { return WholeOf(_object.Get(name)).value_or(0); }
        bool Flag(std::string_view name) const { return Whole(name) != 0; }
        bool Has(std::string_view name) const { return _object.GetClass().FindProperty(name) != nullptr; }

        std::optional<double> Real(std::string_view name) const
        {
            PropertyValue const* const value = _object.Get(name);
            if (!value)
                return std::nullopt;
            if (auto const* held = value->GetIf<float>())
                return *held;
            if (auto const* held = value->GetIf<double>())
                return *held;
            if (std::optional<int64> const whole = WholeOf(value))
                return static_cast<double>(*whole);
            return std::nullopt;
        }

        std::string Option(std::string_view name) const
        {
            PropertyInfo const* const property = _object.GetClass().FindProperty(name);
            std::optional<int64> const value = WholeOf(_object.Get(name));
            if (!property || !value)
                return {};
            std::optional<std::string_view> const option = property->FindOptionName(*value);
            return option ? std::string(*option) : fmt::format("{}", *value);
        }

        std::vector<std::string> Texts(std::string_view name) const
        {
            std::vector<std::string> texts;
            PropertyValue const* const value = _object.Get(name);
            if (PropertyValue::List const* const list = value ? value->GetList() : nullptr)
                for (PropertyValue const& entry : *list)
                    texts.push_back(TextOf(entry));
            return texts;
        }

        PropertyObject const* Object(std::string_view name) const
        {
            PropertyValue const* const value = _object.Get(name);
            return value ? value->AsObject() : nullptr;
        }

        std::vector<PropertyObject const*> Objects(std::string_view name) const
        {
            std::vector<PropertyObject const*> objects;
            PropertyValue const* const value = _object.Get(name);
            if (PropertyValue::List const* const list = value ? value->GetList() : nullptr)
                for (PropertyValue const& entry : *list)
                    if (PropertyObject const* const object = entry.AsObject())
                        objects.push_back(object);
            return objects;
        }

    private:
        PropertyObject const& _object;
    };

    std::optional<std::string> GoalType(std::string const& option)
    {
        static constexpr std::array<std::pair<std::string_view, std::string_view>, 7> Types{ {
            { "GOAL_TYPE_BOUNTY", "bounty" }, { "GOAL_TYPE_BOUNTYCOLLECT", "bounty_collect" }, { "GOAL_TYPE_SCAVENGE", "scavenge" }, { "GOAL_TYPE_PERSONA", "persona" },
            { "GOAL_TYPE_WAYPOINT", "waypoint" }, { "GOAL_TYPE_ACHIEVERANK", "achieve_rank" }, { "GOAL_TYPE_USAGE", "usage" } } };
        for (auto const& [name, type] : Types)
            if (option == name)
                return std::string(type);
        return std::nullopt;
    }

    class QuestReader
    {
    public:
        QuestReader(QuestPackageRead& read, QuestPackageQuest& quest) : _read(read), _quest(quest) { }

        void Fail(std::string category, std::string detail)
        {
            if (_quest.Category.empty())
                _quest.Category = category;
            _quest.Problems.push_back(fmt::format("{}: {}", category, detail));
        }

        void Read(PropertyObject const& root)
        {
            Fields const fields(root);
            QuestTemplateRow row;
            row.Name = fields.Text("m_questName");
            row.NameId = static_cast<uint32>(fields.Whole("m_questNameID"));
            row.TitleKey = Key(fields.Text("m_questTitle"));
            row.InfoKey = Key(fields.Text("m_questInfo"));
            row.PrepKey = Key(fields.Text("m_questPrep"));
            row.UnderwayKey = Key(fields.Text("m_questUnderway"));
            row.CompleteKey = Key(fields.Text("m_questComplete"));
            row.Level = static_cast<uint32>(std::max<int64>(0, fields.Whole("m_questLevel")));
            int64 const repeat = fields.Whole("m_questRepeat");
            if (repeat < 0)
                NotCarried("negative m_questRepeat");
            row.Repeat = static_cast<uint32>(std::max<int64>(0, repeat));
            row.Mainline = fields.Flag("m_mainline");
            row.NoQuestHelper = fields.Flag("m_noQuestHelper");
            row.SkipQhAutoselect = fields.Flag("m_skipQHAutoSelect");
            row.ActivityType = static_cast<uint32>(std::max<int64>(0, fields.Whole("m_activityType")));
            row.PrepAlways = fields.Flag("m_prepAlways");
            row.IsHidden = fields.Flag("m_isHidden");
            _quest.Name = row.Name;
            if (row.Name.empty())
            {
                Fail("no quest name", "m_questName is empty");
                return;
            }
            _quest.Rows.Quests.push_back(row);

            for (std::string const& goal : fields.Texts("m_startGoals"))
                _quest.Rows.StartGoals.push_back(QuestStartGoalRow{ row.Name, goal });
            Gate(fields.Object("m_requirements"), _quest.Rows.Quests.back().Requirements);
            for (std::string_view const list : { "m_prepRequirements", "m_pruneRequirements" })
                Requirements(fields.Object(list), list);
            for (std::string_view const results : { "m_startResults", "m_endResults" })
                Results(fields.Object(results), results);
            for (std::string_view const script : { "m_onStartQuestScript", "m_onEndQuestScript" })
                if (!fields.Text(script).empty())
                    NotCarried(script);
            for (std::string_view const list : { "m_clientTags", "m_missionDoors", "m_defaultDialogAnimation" })
                if (!fields.Texts(list).empty())
                    NotCarried(list);
            if (!fields.Objects("m_dynaMods").empty())
                NotCarried("m_dynaMods");
            if (!fields.Objects("m_questEffectInfoList").empty())
                NotCarried("m_questEffectInfoList");
            if (fields.Flag("m_playAsYourPetNPC"))
                NotCarried("m_playAsYourPetNPC");
            Dialogs(fields.Object("m_dialogList"), std::string(QuestStore::OwnerQuest), {});

            for (PropertyObject const* goal : fields.Objects("m_goals"))
                Goal(*goal);
            std::vector<PropertyObject const*> const logic = fields.Objects("m_goalLogic");
            for (std::size_t index = 0; index < logic.size(); ++index)
                Logic(*logic[index], static_cast<uint32>(index));
        }

    private:
        std::string Key(std::string key)
        {
            if (!key.empty())
                _read.Keys.insert(key);
            return key;
        }

        void NotCarried(std::string_view what)
        {
            ++_read.NotCarried[std::string(what)];
        }

        void Requirements(PropertyObject const* list, std::string_view where)
        {
            if (!list)
                return;
            NotCarried(where);
            CountRequirements(*list);
        }

        void Gate(PropertyObject const* list, std::vector<uint8>& bytes)
        {
            if (!list)
                return;
            CountRequirements(*list);
            if (Fields(*list).Objects("m_requirements").empty())
                return;
            EncodeResult encoded = ObjectSerializer::Encode(list, ZoneExtractor::DataOptions());
            if (!encoded.Ok())
            {
                Fail("requirements that do not encode", encoded.Detail.empty() ? std::string(ObjectSerializer::GetStatusName(encoded.Status)) : std::move(encoded.Detail));
                return;
            }
            bytes = std::move(encoded.Bytes);
        }

        void CountRequirements(PropertyObject const& requirement)
        {
            ++_read.RequirementClasses[std::string(ShortName(requirement.GetClass().Name))];
            Fields const fields(requirement);
            for (PropertyObject const* child : fields.Objects("m_requirements"))
                CountRequirements(*child);
        }

        void Results(PropertyObject const* list, std::string_view where)
        {
            if (list && !Fields(*list).Objects("m_results").empty())
                NotCarried(where);
        }

        void Goal(PropertyObject const& goal)
        {
            Fields const fields(goal);
            std::string const& quest = _quest.Name;
            QuestGoalRow row;
            row.Quest = quest;
            row.Goal = fields.Text("m_goalName");
            if (row.Goal.empty())
            {
                Fail("goal with no name", fmt::format("a {} has no m_goalName", ShortName(goal.GetClass().Name)));
                return;
            }
            std::string const option = fields.Option("m_goalType");
            std::optional<std::string> const type = GoalType(option);
            if (!type)
            {
                Fail(fmt::format("goal type {} the tables do not hold", option.empty() ? std::string(ShortName(goal.GetClass().Name)) : option),
                    fmt::format("goal {} is a {}", row.Goal, ShortName(goal.GetClass().Name)));
                return;
            }
            row.Type = *type;
            row.NameId = static_cast<uint32>(fields.Whole("m_goalNameID"));
            row.TitleKey = Key(fields.Text("m_goalTitle"));
            row.UnderwayKey = Key(fields.Text("m_goalUnderway"));
            row.CompleteKey = Key(fields.Text("m_completeText"));
            row.LocationKey = Key(fields.Text("m_locationName"));
            row.DestinationZone = fields.Text("m_destinationZone");
            row.Image1 = fields.Text("m_displayImage1");
            row.Image2 = fields.Text("m_displayImage2");
            row.PersonaName = fields.Text("m_personaName");
            row.UsePatron = fields.Flag("m_usePatron");
            row.ZoneEntry = fields.Flag("m_zoneEntry");
            row.ZoneExit = fields.Flag("m_zoneExit");
            row.ZoneTag = fields.Text("m_zoneTag");
            row.ProximityTag = fields.Text("m_proximityTag");
            row.Rank = static_cast<uint32>(std::max<int64>(0, fields.Whole("m_rank")));
            row.School = fields.Text("m_schoolName");
            row.HideFloatyText = fields.Flag("m_hideGoalFloatyText");
            if (PropertyObject const* const tally = fields.Object("m_tallyCounter"))
            {
                Fields const counter(*tally);
                row.TallyCount = static_cast<uint32>(std::max<int64>(0, counter.Whole("m_count")));
                row.TallyPercent = static_cast<uint32>(std::max(0.0, std::round(counter.Real("m_percentChance").value_or(0.0))));
                row.TallyDescriptorKey = Key(counter.Text("m_descriptor"));
                row.TallyDescriptor2Key = Key(counter.Text("m_descriptor2"));
                Results(counter.Object("m_tallyResults"), "m_tallyResults");
            }
            for (std::string_view const total : { "m_bountyTotal", "m_itemTotal" })
                if (fields.Has(total))
                    row.TallyCount = static_cast<uint32>(std::max<int64>(0, fields.Whole(total)));
            if (fields.Flag("m_hideGoal"))
                NotCarried("m_hideGoal");
            if (fields.Flag("m_petOnlyQuest"))
                NotCarried("m_petOnlyQuest");
            if (!fields.Text("m_hyperlink").empty())
                NotCarried("m_hyperlink");
            if (!fields.Text("m_requiresMagicWeavingSchool").empty())
                NotCarried("m_requiresMagicWeavingSchool");
            if (!fields.Texts("m_genericEvents").empty())
                NotCarried("m_genericEvents");
            Gate(fields.Object("m_goalRequirements"), row.Requirements);
            for (std::string_view const results : { "m_completeResults", "m_activateResults" })
                Results(fields.Object(results), results);
            _quest.Rows.Goals.push_back(row);

            uint32 position = 0;
            for (std::string_view const list : { "m_npcAdjectives", "m_itemAdjectives" })
                for (std::string const& adjective : fields.Texts(list))
                    _quest.Rows.Adjectives.push_back(QuestGoalAdjectiveRow{ quest, row.Goal, position++, adjective });
            position = 0;
            for (std::string const& tag : fields.Texts("m_clientTags"))
                _quest.Rows.ClientTags.push_back(QuestGoalClientTagRow{ quest, row.Goal, position++, tag });
            Dialogs(fields.Object("m_dialogList"), std::string(QuestStore::OwnerGoal), row.Goal);
        }

        void Logic(PropertyObject const& logic, uint32 position)
        {
            Fields const fields(logic);
            std::vector<std::string> const all = fields.Texts("m_goalsAND");
            std::vector<std::string> const any = fields.Texts("m_goalsOR");
            std::vector<std::string> const adds = fields.Texts("m_goalsToAdd");
            bool const completes = fields.Flag("m_completeQuest");
            std::set<std::string, std::less<>> owners(all.begin(), all.end());
            owners.insert(any.begin(), any.end());
            if (owners.empty())
            {
                NotCarried("goal logic waiting on no goal");
                return;
            }
            for (std::string const& owner : owners)
            {
                _quest.Rows.Logic.push_back(QuestGoalLogicRow{ _quest.Name, owner, position, completes, static_cast<uint32>(std::max<int64>(1, fields.Whole("m_requiredORCount"))) });
                auto const members = [&](std::string_view kind, std::vector<std::string> const& goals)
                {
                    for (std::size_t index = 0; index < goals.size(); ++index)
                        _quest.Rows.LogicMembers.push_back(QuestGoalLogicMemberRow{ _quest.Name, owner, position, std::string(kind), static_cast<uint32>(index), goals[index] });
                };
                members(QuestStore::KindAnd, all);
                members(QuestStore::KindOr, any);
                members(QuestStore::KindAdd, adds);
            }
        }

        void Dialogs(PropertyObject const* list, std::string const& owner, std::string const& goal)
        {
            if (!list)
                return;
            std::set<std::string, std::less<>> seen;
            for (PropertyObject const* dialog : Fields(*list).Objects("m_dialogs"))
            {
                Fields const fields(*dialog);
                std::string const tag = fields.Text("m_dialogTag");
                if (!QuestStore::IsDialogTag(tag))
                {
                    NotCarried(fmt::format("dialog tag {}", tag.empty() ? "(none)" : tag));
                    continue;
                }
                if (!seen.insert(tag).second)
                {
                    NotCarried(fmt::format("second {} dialog", tag));
                    continue;
                }
                _quest.Rows.Dialogs.push_back(QuestDialogRow{ _quest.Name, owner, goal, tag });
                std::vector<PropertyObject const*> const entries = fields.Objects("m_dialogEntries");
                for (std::size_t index = 0; index < entries.size(); ++index)
                {
                    Fields const entry(*entries[index]);
                    std::vector<std::string> const animations = entry.Texts("m_dialogAnimationList");
                    if (animations.size() > 1)
                        NotCarried("second dialog animation");
                    QuestDialogEntryRow line{ _quest.Name, owner, goal, tag, static_cast<uint32>(index), static_cast<uint32>(entry.Whole("m_actorTemplateID")),
                        Key(entry.Text("m_dialog")), entry.Text("m_picture"), entry.Text("m_soundFile"), entry.Text("m_action"), entry.Text("m_dialogEvent"),
                        animations.empty() ? std::string() : animations.front() };
                    _quest.Rows.DialogEntries.push_back(std::move(line));
                }
                for (PropertyObject const* madlib : fields.Objects("m_madlibs"))
                {
                    Fields const actor(*madlib);
                    uint32 const index = static_cast<uint32>(actor.Whole("m_index"));
                    PropertyObject const* const block = actor.Object("m_madlibBlock");
                    if (!block)
                        continue;
                    for (PropertyObject const* argument : Fields(*block).Objects("m_madlibs"))
                    {
                        Fields const value(*argument);
                        _quest.Rows.Madlibs.push_back(QuestDialogMadlibRow{ _quest.Name, owner, goal, tag, index, value.Text("m_madlibToken"), value.Text("m_madlibArgument") });
                    }
                }
            }
        }

        QuestPackageRead& _read;
        QuestPackageQuest& _quest;
    };

    std::string UnknownClassName(DecodeIssue const& issue)
    {
        std::size_t const open = issue.Detail.find('\'');
        std::size_t const close = open == std::string::npos ? std::string::npos : issue.Detail.find('\'', open + 1);
        if (close == std::string::npos)
            return fmt::format("#{}", issue.Hash);
        return std::string(ShortName(std::string_view(issue.Detail).substr(open + 1, close - open - 1)));
    }

    std::string_view LastProperty(std::string_view path)
    {
        if (std::size_t const bracket = path.rfind('['); bracket != std::string_view::npos && path.back() == ']')
            path = path.substr(0, bracket);
        std::size_t const dot = path.rfind('.');
        return dot == std::string_view::npos ? path : path.substr(dot + 1);
    }

    bool InRequirements(std::string_view path)
    {
        std::size_t const dot = path.find('.');
        return dot != std::string_view::npos && path.substr(dot + 1).starts_with("m_requirements");
    }

    bool InCarriedPart(std::string_view path)
    {
        return std::none_of(NotCarriedProperties.begin(), NotCarriedProperties.end(), [path](std::string_view property)
        {
            return path.find(fmt::format(".{}", property)) != std::string_view::npos;
        });
    }

    template<typename Row>
    void Lengths(QuestPackageQuest& quest, std::vector<Row> const& rows, std::string_view table, auto const& columns)
    {
        for (Row const& row : rows)
            for (auto const& [column, text, limit] : columns(row))
                if (text.size() > limit)
                {
                    if (quest.Category.empty())
                        quest.Category = "value longer than its column";
                    quest.Problems.push_back(fmt::format("value longer than its column: {}.{} holds {} bytes, more than {}", table, column, text.size(), limit));
                }
    }

    using Column = std::tuple<std::string_view, std::string const&, std::size_t>;

    void CheckLengths(QuestPackageQuest& quest)
    {
        QuestRows const& rows = quest.Rows;
        Lengths(quest, rows.Quests, "quest_template", [](QuestTemplateRow const& row) { return std::vector<Column>{ { "name", row.Name, NameColumn }, { "title_key", row.TitleKey, TextColumn },
            { "info_key", row.InfoKey, TextColumn }, { "prep_key", row.PrepKey, TextColumn }, { "underway_key", row.UnderwayKey, TextColumn }, { "complete_key", row.CompleteKey, TextColumn } }; });
        Lengths(quest, rows.StartGoals, "quest_start_goal", [](QuestStartGoalRow const& row) { return std::vector<Column>{ { "goal_name", row.Goal, NameColumn } }; });
        Lengths(quest, rows.Goals, "quest_goal", [](QuestGoalRow const& row) { return std::vector<Column>{ { "goal_name", row.Goal, NameColumn }, { "title_key", row.TitleKey, TextColumn },
            { "underway_key", row.UnderwayKey, TextColumn }, { "complete_key", row.CompleteKey, TextColumn }, { "location_key", row.LocationKey, TextColumn },
            { "destination_zone", row.DestinationZone, TextColumn }, { "image1", row.Image1, TextColumn }, { "image2", row.Image2, TextColumn }, { "persona_name", row.PersonaName, TextColumn },
            { "tally_descriptor_key", row.TallyDescriptorKey, TextColumn }, { "tally_descriptor2_key", row.TallyDescriptor2Key, TextColumn }, { "zone_tag", row.ZoneTag, TextColumn },
            { "proximity_tag", row.ProximityTag, TextColumn }, { "school", row.School, NameColumn } }; });
        Lengths(quest, rows.Adjectives, "quest_goal_adjective", [](QuestGoalAdjectiveRow const& row) { return std::vector<Column>{ { "adjective", row.Adjective, TextColumn } }; });
        Lengths(quest, rows.ClientTags, "quest_goal_client_tag", [](QuestGoalClientTagRow const& row) { return std::vector<Column>{ { "tag", row.Tag, TextColumn } }; });
        Lengths(quest, rows.LogicMembers, "quest_goal_logic_member", [](QuestGoalLogicMemberRow const& row) { return std::vector<Column>{ { "goal_name", row.Goal, NameColumn },
            { "member_goal", row.Member, NameColumn } }; });
        Lengths(quest, rows.DialogEntries, "quest_dialog_entry", [](QuestDialogEntryRow const& row) { return std::vector<Column>{ { "dialog_key", row.DialogKey, TextColumn },
            { "picture", row.Picture, TextColumn }, { "sound", row.Sound, TextColumn }, { "action", row.Action, TextColumn }, { "dialog_event", row.DialogEvent, TextColumn },
            { "animation", row.Animation, TextColumn } }; });
        Lengths(quest, rows.Madlibs, "quest_dialog_madlib", [](QuestDialogMadlibRow const& row) { return std::vector<Column>{ { "identifier", row.Identifier, NameColumn },
            { "value", row.Value, TextColumn } }; });
    }

    void Mark(QuestPackageQuest& quest, std::string category, std::string detail)
    {
        if (quest.Category.empty())
            quest.Category = category;
        quest.Problems.push_back(fmt::format("{}: {}", category, std::move(detail)));
    }

    template<typename Rows>
    void Append(Rows& into, Rows const& from)
    {
        into.insert(into.end(), from.begin(), from.end());
    }
}

std::size_t QuestPackageRead::GetCleanCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(Quests.begin(), Quests.end(), [](QuestPackageQuest const& quest) { return quest.IsClean(); }));
}

QuestPackageRead::Counts QuestPackageRead::GetCategories() const
{
    Counts categories;
    for (QuestPackageQuest const& quest : Quests)
        if (!quest.IsClean())
            ++categories[quest.Category];
    return categories;
}

std::vector<std::string> QuestPackage::GetQuestEntries(KiwadArchive const& archive)
{
    std::vector<std::string> names;
    for (KiwadEntry const& entry : archive.GetEntries())
    {
        std::string const name = KiwadArchive::NormalizeSlashes(entry.Name);
        if (name.starts_with(QuestFolder) && name.ends_with(".xml"))
            names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

QuestPackageRead QuestPackage::Read(TypeCatalogPtr const& catalog, std::vector<QuestPackageEntry> const& entries)
{
    QuestPackageRead read;
    ClassInfo const* const questClass = catalog ? catalog->FindClass(QuestClass) : nullptr;
    for (QuestPackageEntry const& entry : entries)
    {
        ++read.Files;
        if (!questClass)
        {
            ++read.Unread["the type dump lists no QuestTemplate"];
            continue;
        }
        XmlReadResult const xml = XmlObjectReader::Read(catalog, entry.Text);
        if (!xml.Ok())
        {
            ++read.Unread[fmt::format("not readable XML objects: {}", XmlObjectReader::GetStatusName(xml.Status))];
            continue;
        }
        bool unknownInCarried = false;
        std::string firstUnknown;
        for (DecodeIssue const& issue : xml.Issues)
        {
            if (issue.Kind != DecodeIssueKind::UnknownClass)
                continue;
            std::string const name = UnknownClassName(issue);
            ++read.UnknownClasses[name];
            if (InCarriedPart(issue.Path) && !unknownInCarried)
            {
                unknownInCarried = true;
                firstUnknown = fmt::format("{} at {}", name, issue.Path);
            }
        }
        if (xml.Objects.size() != 1 || !xml.Objects.front())
        {
            ++read.Unread[fmt::format("{} root objects, not one", xml.Objects.size())];
            continue;
        }
        PropertyObject const& root = *xml.Objects.front();
        ++read.RootClasses[std::string(ShortName(root.GetClass().Name))];
        if (!root.IsA(*questClass))
        {
            ++read.Unread[fmt::format("root {} is no QuestTemplate", ShortName(root.GetClass().Name))];
            continue;
        }
        QuestPackageQuest& quest = read.Quests.emplace_back();
        quest.Entry = entry.Name;
        QuestReader reader(read, quest);
        reader.Read(root);
        if (unknownInCarried)
            reader.Fail(fmt::format("unknown class {}", firstUnknown.substr(0, firstUnknown.find(' '))), firstUnknown);
        for (DecodeIssue const& issue : xml.Issues)
        {
            if (issue.Kind == DecodeIssueKind::UnknownClass)
                continue;
            if (InRequirements(issue.Path) && !(issue.Kind == DecodeIssueKind::InvalidValue && issue.Detail.starts_with("holds ''")))
            {
                reader.Fail(fmt::format("requirement not fully read ({})", ObjectSerializer::GetIssueName(issue.Kind)), fmt::format("{}: {}", issue.Path, issue.Detail));
                continue;
            }
            if (issue.Kind == DecodeIssueKind::UnknownProperty)
            {
                std::size_t const open = issue.Detail.find('<');
                std::size_t const close = open == std::string::npos ? std::string::npos : issue.Detail.find('>', open);
                ++read.NotCarried[fmt::format("property the type dump does not list: {}", close == std::string::npos ? issue.Detail : issue.Detail.substr(open + 1, close - open - 1))];
                continue;
            }
            std::string_view const property = LastProperty(issue.Path);
            if (issue.Kind == DecodeIssueKind::InvalidValue && issue.Detail.starts_with("holds ''"))
                ++read.Adjusted[fmt::format("empty {} read as its default", property)];
            else if (InCarriedPart(issue.Path) && std::find(ReadProperties.begin(), ReadProperties.end(), property) != ReadProperties.end())
                reader.Fail(fmt::format("unreadable value ({})", ObjectSerializer::GetIssueName(issue.Kind)), fmt::format("{}: {}", issue.Path, issue.Detail));
            else
                ++read.NotCarried[fmt::format("unreadable {}", property)];
        }
    }
    return read;
}

void QuestPackage::Check(QuestPackageRead& read, QuestValidator::Context const& context, std::set<std::string, std::less<>> const& heldElsewhere)
{
    std::map<std::string, std::size_t, std::less<>> names;
    for (QuestPackageQuest const& quest : read.Quests)
        if (!quest.Name.empty())
            ++names[quest.Name];
    for (QuestPackageQuest& quest : read.Quests)
    {
        if (!quest.IsClean())
            continue;
        if (names[quest.Name] > 1)
            Mark(quest, "quest name in more than one file", quest.Name);
        if (heldElsewhere.contains(quest.Name))
            Mark(quest, "quest name another source holds", quest.Name);
        CheckLengths(quest);
        std::set<std::string, std::less<>> starts;
        for (QuestStartGoalRow const& start : quest.Rows.StartGoals)
            if (!starts.insert(start.Goal).second)
                Mark(quest, "start goal listed twice", start.Goal);
        std::set<std::tuple<std::string, std::string, std::string, uint32, std::string>> madlibs;
        for (QuestDialogMadlibRow const& madlib : quest.Rows.Madlibs)
            if (!madlibs.emplace(madlib.Owner, madlib.Goal, madlib.Tag, madlib.Entry, madlib.Identifier).second)
                Mark(quest, "madlib given twice for one line", fmt::format("{} on line {} of the {} dialog", madlib.Identifier, madlib.Entry, madlib.Tag));
        for (QuestDialogEntryRow const& line : quest.Rows.DialogEntries)
            if (line.ActorTemplateId != 0 && context.HasObjects() && !context.TemplateIds.contains(line.ActorTemplateId))
                Mark(quest, "actor template the install lacks", fmt::format("{} in the {} dialog", line.ActorTemplateId, line.Tag));
        for (QuestGoalRow& goal : quest.Rows.Goals)
        {
            if (goal.Type != "persona" || !context.HasObjects() || goal.PersonaName.empty() || context.ObjectNames.contains(goal.PersonaName))
                continue;
            std::string_view const persona = goal.PersonaName;
            if (persona.ends_with(PersonaSuffix) && context.ObjectNames.contains(std::string(persona.substr(0, persona.size() - PersonaSuffix.size()))))
            {
                goal.PersonaName.resize(persona.size() - PersonaSuffix.size());
                ++read.Adjusted[fmt::format("persona named by its {} template, read as the object it names", PersonaSuffix)];
                continue;
            }
            Mark(quest, "persona the install lacks", fmt::format("goal {} names {}", goal.Goal, goal.PersonaName));
        }
        if (!quest.IsClean())
            continue;
        std::vector<QuestValidationError> const errors = QuestValidator::Validate(quest.Rows, context);
        for (QuestValidationError const& error : errors)
        {
            std::string_view const message = error.Message;
            std::string category = "refused by the quest validator";
            if (message.find("does not resolve in the locale text") != std::string_view::npos)
                category = "locale key the install lacks";
            else if (message.find("which the quest does not have") != std::string_view::npos || message.find("is not a goal of the quest") != std::string_view::npos)
                category = "names a goal the quest does not have";
            else if (message.find("names no persona") != std::string_view::npos)
                category = "persona goal naming no persona";
            Mark(quest, category, error.Message);
        }
    }
}

std::size_t QuestPackage::CountMissingKeys(QuestPackageRead const& read, QuestValidator::KeyLookup const& hasKey)
{
    if (!hasKey)
        return 0;
    return static_cast<std::size_t>(std::count_if(read.Keys.begin(), read.Keys.end(), [&hasKey](std::string const& key) { return !hasKey(key); }));
}

std::string QuestPackage::SourceOf(std::string_view packageName)
{
    return fmt::format("{}{}", SourcePrefix, packageName);
}

std::vector<std::string_view> QuestPackage::GetTables()
{
    return { "quest_template", "quest_start_goal", "quest_goal", "quest_goal_adjective", "quest_goal_client_tag", "quest_goal_logic", "quest_goal_logic_member", "quest_dialog",
        "quest_dialog_entry", "quest_dialog_madlib" };
}

WorldSqlScript QuestPackage::BuildScript(QuestPackageRead const& read, std::string const& source)
{
    QuestRows rows;
    for (QuestPackageQuest const& quest : read.Quests)
    {
        if (!quest.IsClean())
            continue;
        Append(rows.Quests, quest.Rows.Quests);
        Append(rows.StartGoals, quest.Rows.StartGoals);
        Append(rows.Goals, quest.Rows.Goals);
        Append(rows.Adjectives, quest.Rows.Adjectives);
        Append(rows.ClientTags, quest.Rows.ClientTags);
        Append(rows.Logic, quest.Rows.Logic);
        Append(rows.LogicMembers, quest.Rows.LogicMembers);
        Append(rows.Dialogs, quest.Rows.Dialogs);
        Append(rows.DialogEntries, quest.Rows.DialogEntries);
        Append(rows.Madlibs, quest.Rows.Madlibs);
    }
    using Value = WorldSqlScript::Value;
    auto const number = [](auto value) { return Value{ static_cast<uint64>(value) }; };
    auto const convert = [&source](auto const& from, auto const& make)
    {
        std::vector<WorldSqlScript::Row> converted;
        converted.reserve(from.size());
        for (auto const& row : from)
        {
            WorldSqlScript::Row values = make(row);
            values.emplace_back(source);
            converted.push_back(std::move(values));
        }
        return converted;
    };

    WorldSqlScript script;
    script.ReplaceRows("quest_template", "source", Value{ source }, { "name", "name_id", "title_key", "info_key", "prep_key", "underway_key", "complete_key", "level", "repeat", "mainline",
        "no_quest_helper", "skip_qh_autoselect", "pet_only", "activity_type", "prep_always", "is_hidden", "requirements", "source" }, convert(rows.Quests, [&](QuestTemplateRow const& row)
    {
        return WorldSqlScript::Row{ row.Name, number(row.NameId), row.TitleKey, row.InfoKey, row.PrepKey, row.UnderwayKey, row.CompleteKey, number(row.Level), number(row.Repeat),
            number(row.Mainline), number(row.NoQuestHelper), number(row.SkipQhAutoselect), number(row.PetOnly), number(row.ActivityType), number(row.PrepAlways), number(row.IsHidden),
            row.Requirements.empty() ? Value{ std::monostate{} } : Value{ std::string(row.Requirements.begin(), row.Requirements.end()) } };
    }));
    script.InsertRows("quest_start_goal", { "quest", "goal_name", "source" }, convert(rows.StartGoals, [](QuestStartGoalRow const& row)
    {
        return WorldSqlScript::Row{ row.Quest, row.Goal };
    }));
    script.InsertRows("quest_goal", { "quest", "goal_name", "name_id", "type", "title_key", "underway_key", "complete_key", "location_key", "destination_zone", "image1", "image2",
        "persona_name", "use_patron", "tally_count", "tally_percent", "tally_descriptor_key", "tally_descriptor2_key", "zone_entry", "zone_exit", "zone_tag", "proximity_tag", "rank",
        "hide_floaty_text", "hide_location", "requirements", "school", "source" }, convert(rows.Goals, [&](QuestGoalRow const& row)
    {
        return WorldSqlScript::Row{ row.Quest, row.Goal, number(row.NameId), row.Type, row.TitleKey, row.UnderwayKey, row.CompleteKey, row.LocationKey, row.DestinationZone, row.Image1,
            row.Image2, row.PersonaName, number(row.UsePatron), number(row.TallyCount), number(row.TallyPercent), row.TallyDescriptorKey, row.TallyDescriptor2Key, number(row.ZoneEntry),
            number(row.ZoneExit), row.ZoneTag, row.ProximityTag, number(row.Rank), number(row.HideFloatyText), number(row.HideLocation),
            row.Requirements.empty() ? Value{ std::monostate{} } : Value{ std::string(row.Requirements.begin(), row.Requirements.end()) }, row.School };
    }));
    script.InsertRows("quest_goal_adjective", { "quest", "goal_name", "position", "adjective", "source" }, convert(rows.Adjectives, [&](QuestGoalAdjectiveRow const& row)
    {
        return WorldSqlScript::Row{ row.Quest, row.Goal, number(row.Position), row.Adjective };
    }));
    script.InsertRows("quest_goal_client_tag", { "quest", "goal_name", "position", "tag", "source" }, convert(rows.ClientTags, [&](QuestGoalClientTagRow const& row)
    {
        return WorldSqlScript::Row{ row.Quest, row.Goal, number(row.Position), row.Tag };
    }));
    script.InsertRows("quest_goal_logic", { "quest", "goal_name", "position", "complete_quest", "required_or_count", "source" }, convert(rows.Logic, [&](QuestGoalLogicRow const& row)
    {
        return WorldSqlScript::Row{ row.Quest, row.Goal, number(row.Position), number(row.CompleteQuest), number(row.RequiredOrCount) };
    }));
    script.InsertRows("quest_goal_logic_member", { "quest", "goal_name", "logic", "kind", "position", "member_goal", "source" }, convert(rows.LogicMembers, [&](QuestGoalLogicMemberRow const& row)
    {
        return WorldSqlScript::Row{ row.Quest, row.Goal, number(row.Logic), row.Kind, number(row.Position), row.Member };
    }));
    script.InsertRows("quest_dialog", { "quest", "owner", "goal_name", "tag", "source" }, convert(rows.Dialogs, [](QuestDialogRow const& row)
    {
        return WorldSqlScript::Row{ row.Quest, row.Owner, row.Goal, row.Tag };
    }));
    script.InsertRows("quest_dialog_entry", { "quest", "owner", "goal_name", "tag", "position", "actor_template_id", "dialog_key", "picture", "sound", "action", "dialog_event", "animation",
        "source" }, convert(rows.DialogEntries, [&](QuestDialogEntryRow const& row)
    {
        return WorldSqlScript::Row{ row.Quest, row.Owner, row.Goal, row.Tag, number(row.Position), number(row.ActorTemplateId), row.DialogKey, row.Picture, row.Sound, row.Action,
            row.DialogEvent, row.Animation };
    }));
    script.InsertRows("quest_dialog_madlib", { "quest", "owner", "goal_name", "tag", "entry", "identifier", "value", "source" }, convert(rows.Madlibs, [&](QuestDialogMadlibRow const& row)
    {
        return WorldSqlScript::Row{ row.Quest, row.Owner, row.Goal, row.Tag, number(row.Entry), row.Identifier, row.Value };
    }));
    return script;
}
