/*
 * Project Ambrose by Imjustchico
 * Reads every quest table and object_template's ids and names from the world database, or from the row source a test gives it, checks them with the quest validator, and either builds the quests that passed and counts the rest, at start, or swaps in a generation only when nothing failed, on a reload; both log how many quests and goals are serving and how many validation errors were found.
 */

#include "QuestMgr.h"
#include "RequirementMgr.h"
#include "DatabaseEnv.h"
#include "LocaleStore.h"
#include "Log.h"
#include "ReloadMgr.h"

#include <fmt/format.h>

#include <set>
#include <utility>

namespace
{
    constexpr char const* QuestLog = "server.loading";

    template<typename Read>
    bool Select(std::string_view table, std::string_view sql, std::vector<std::string>& errors, Read read)
    {
        QueryResult result;
        if (!WorldDatabase.TryQuery(sql, result))
        {
            errors.push_back(fmt::format("{} could not be read", table));
            return false;
        }
        if (result)
        {
            do
                read(result->Fetch());
            while (result->NextRow());
        }
        return true;
    }

    void LogLoaded(std::size_t quests, std::size_t goals, std::size_t errors)
    {
        LOG_INFO(QuestLog, "Loaded {} quests, {} goals, {} validation errors", quests, goals, errors);
    }
}

QuestMgr& QuestMgr::Instance()
{
    static QuestMgr instance;
    return instance;
}

bool QuestMgr::ReadWorldRows(QuestRows& rows, std::vector<std::string>& errors)
{
    if (!WorldDatabase.IsOpen())
    {
        errors.push_back("the world database is not open, so no quest was read");
        return false;
    }
    bool ok = true;
    ok &= Select("quest_template", "SELECT `name`, `name_id`, `title_key`, `info_key`, `prep_key`, `underway_key`, `complete_key`, `level`, `repeat`, `mainline`, `no_quest_helper`, "
        "`skip_qh_autoselect`, `pet_only`, `activity_type`, `prep_always`, `is_hidden`, `requirement_list_id` FROM `quest_template` ORDER BY `name`", errors, [&rows](Field const* f)
    {
        QuestTemplateRow row{ f[0].Get<std::string>(), f[1].Get<uint32>(), f[2].Get<std::string>(), f[3].Get<std::string>(), f[4].Get<std::string>(),
            f[5].Get<std::string>(), f[6].Get<std::string>(), f[7].Get<uint32>(), f[8].Get<uint32>(), f[9].Get<bool>(), f[10].Get<bool>(), f[11].Get<bool>(), f[12].Get<bool>(),
            f[13].Get<uint32>(), f[14].Get<bool>(), f[15].Get<bool>(), {} };
        if (!f[16].IsNull())
            row.RequirementListId = f[16].Get<std::string>();
        rows.Quests.push_back(std::move(row));
    });
    ok &= Select("quest_start_goal", "SELECT `quest`, `goal_name` FROM `quest_start_goal` ORDER BY `quest`, `goal_name`", errors, [&rows](Field const* f)
    {
        rows.StartGoals.push_back(QuestStartGoalRow{ f[0].Get<std::string>(), f[1].Get<std::string>() });
    });
    ok &= Select("quest_goal", "SELECT `quest`, `goal_name`, `name_id`, `type`, `title_key`, `underway_key`, `complete_key`, `location_key`, `destination_zone`, `image1`, `image2`, "
        "`persona_name`, `use_patron`, `tally_count`, `tally_percent`, `tally_descriptor_key`, `tally_descriptor2_key`, `zone_entry`, `zone_exit`, `zone_tag`, `proximity_tag`, `rank`, "
        "`hide_floaty_text`, `hide_location` FROM `quest_goal` ORDER BY `quest`, `goal_name`", errors, [&rows](Field const* f)
    {
        rows.Goals.push_back(QuestGoalRow{ f[0].Get<std::string>(), f[1].Get<std::string>(), f[2].Get<uint32>(), f[3].Get<std::string>(), f[4].Get<std::string>(),
            f[5].Get<std::string>(), f[6].Get<std::string>(), f[7].Get<std::string>(), f[8].Get<std::string>(), f[9].Get<std::string>(), f[10].Get<std::string>(),
            f[11].Get<std::string>(), f[12].Get<bool>(), f[13].Get<uint32>(), f[14].Get<uint32>(), f[15].Get<std::string>(), f[16].Get<std::string>(), f[17].Get<bool>(),
            f[18].Get<bool>(), f[19].Get<std::string>(), f[20].Get<std::string>(), f[21].Get<uint32>(), f[22].Get<bool>(), f[23].Get<bool>() });
    });
    ok &= Select("quest_goal_adjective", "SELECT `quest`, `goal_name`, `position`, `adjective` FROM `quest_goal_adjective` ORDER BY `quest`, `goal_name`, `position`", errors, [&rows](Field const* f)
    {
        rows.Adjectives.push_back(QuestGoalAdjectiveRow{ f[0].Get<std::string>(), f[1].Get<std::string>(), f[2].Get<uint32>(), f[3].Get<std::string>() });
    });
    ok &= Select("quest_goal_client_tag", "SELECT `quest`, `goal_name`, `position`, `tag` FROM `quest_goal_client_tag` ORDER BY `quest`, `goal_name`, `position`", errors, [&rows](Field const* f)
    {
        rows.ClientTags.push_back(QuestGoalClientTagRow{ f[0].Get<std::string>(), f[1].Get<std::string>(), f[2].Get<uint32>(), f[3].Get<std::string>() });
    });
    ok &= Select("quest_goal_logic", "SELECT `quest`, `goal_name`, `position`, `complete_quest` FROM `quest_goal_logic` ORDER BY `quest`, `goal_name`, `position`", errors, [&rows](Field const* f)
    {
        rows.Logic.push_back(QuestGoalLogicRow{ f[0].Get<std::string>(), f[1].Get<std::string>(), f[2].Get<uint32>(), f[3].Get<bool>() });
    });
    ok &= Select("quest_goal_logic_member", "SELECT `quest`, `goal_name`, `logic`, `kind`, `position`, `member_goal` FROM `quest_goal_logic_member` "
        "ORDER BY `quest`, `goal_name`, `logic`, `kind`, `position`", errors, [&rows](Field const* f)
    {
        rows.LogicMembers.push_back(QuestGoalLogicMemberRow{ f[0].Get<std::string>(), f[1].Get<std::string>(), f[2].Get<uint32>(), f[3].Get<std::string>(), f[4].Get<uint32>(), f[5].Get<std::string>() });
    });
    ok &= Select("quest_dialog", "SELECT `quest`, `owner`, `goal_name`, `tag` FROM `quest_dialog` ORDER BY `quest`, `owner`, `goal_name`, `tag`", errors, [&rows](Field const* f)
    {
        rows.Dialogs.push_back(QuestDialogRow{ f[0].Get<std::string>(), f[1].Get<std::string>(), f[2].Get<std::string>(), f[3].Get<std::string>() });
    });
    ok &= Select("quest_dialog_entry", "SELECT `quest`, `owner`, `goal_name`, `tag`, `position`, `actor_template_id`, `dialog_key`, `picture`, `sound`, `action`, `dialog_event`, `animation` "
        "FROM `quest_dialog_entry` ORDER BY `quest`, `owner`, `goal_name`, `tag`, `position`", errors, [&rows](Field const* f)
    {
        rows.DialogEntries.push_back(QuestDialogEntryRow{ f[0].Get<std::string>(), f[1].Get<std::string>(), f[2].Get<std::string>(), f[3].Get<std::string>(), f[4].Get<uint32>(),
            f[5].Get<uint32>(), f[6].Get<std::string>(), f[7].Get<std::string>(), f[8].Get<std::string>(), f[9].Get<std::string>(), f[10].Get<std::string>(), f[11].Get<std::string>() });
    });
    ok &= Select("quest_dialog_madlib", "SELECT `quest`, `owner`, `goal_name`, `tag`, `entry`, `identifier`, `value` FROM `quest_dialog_madlib` "
        "ORDER BY `quest`, `owner`, `goal_name`, `tag`, `entry`, `identifier`", errors, [&rows](Field const* f)
    {
        rows.Madlibs.push_back(QuestDialogMadlibRow{ f[0].Get<std::string>(), f[1].Get<std::string>(), f[2].Get<std::string>(), f[3].Get<std::string>(), f[4].Get<uint32>(),
            f[5].Get<std::string>(), f[6].Get<std::string>() });
    });
    ok &= Select("object_template", "SELECT `template_id`, `object_name` FROM `object_template`", errors, [&rows](Field const* f)
    {
        rows.Objects.push_back(QuestObjectRow{ f[0].Get<uint32>(), f[1].Get<std::string>() });
    });
    return ok;
}

void QuestMgr::SetRowSource(RowSource source)
{
    std::lock_guard const lock(_sourceMutex);
    _source = std::move(source);
}

void QuestMgr::SetKeyLookup(KeyLookup lookup)
{
    std::lock_guard const lock(_sourceMutex);
    _keys = std::move(lookup);
}

void QuestMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(Target), [this](std::vector<std::string>& errors) { return Load(errors); }, { std::string(RequirementMgr::Target) });
}

bool QuestMgr::CanOffer(std::string_view questName, RequirementContext const& context) const
{
    auto const quests = _quests.Get();
    QuestInfo const* quest = quests->Find(questName);
    if (!quest)
        return false;
    return quest->Row.RequirementListId.empty() || sRequirementMgr.Evaluate(quest->Row.RequirementListId, context);
}

std::vector<std::string> QuestMgr::GetQuestsOfferedBy(uint32 templateId, RequirementContext const& context) const
{
    std::vector<std::string> offered = _quests.Get()->GetQuestsOfferedBy(templateId);
    std::erase_if(offered, [this, &context](std::string const& questName) { return !CanOffer(questName, context); });
    return offered;
}

bool QuestMgr::Read(QuestRows& rows, std::vector<std::string>& errors) const
{
    RowSource source;
    {
        std::lock_guard const lock(_sourceMutex);
        source = _source;
    }
    return source ? source(rows, errors) : ReadWorldRows(rows, errors);
}

QuestValidator::Context QuestMgr::MakeContext(QuestRows const& rows) const
{
    KeyLookup keys;
    {
        std::lock_guard const lock(_sourceMutex);
        keys = _keys;
    }
    if (!keys && sLocaleStore.IsLoaded())
        keys = [](std::string_view key) { return sLocaleStore.HasKey(key); };
    return QuestValidator::Context::From(rows.Objects, std::move(keys));
}

QuestLoadResult QuestMgr::LoadSkippingInvalid()
{
    QuestLoadResult result;
    QuestRows rows;
    if (!Read(rows, result.Errors))
        return result;
    std::vector<QuestValidationError> const problems = QuestValidator::Validate(rows, MakeContext(rows));
    std::set<std::string, std::less<>> skipped;
    for (QuestValidationError const& problem : problems)
    {
        skipped.insert(problem.Quest);
        result.Errors.push_back(problem.Message);
    }
    std::shared_ptr<QuestStore const> store = QuestStore::Build(rows, skipped);
    result.Loaded = true;
    result.Quests = store->GetQuestCount();
    result.Goals = store->GetGoalCount();
    _quests.Replace(std::move(store));
    LogLoaded(result.Quests, result.Goals, problems.size());
    return result;
}

bool QuestMgr::Load(std::vector<std::string>& errors)
{
    QuestRows rows;
    if (!Read(rows, errors))
        return false;
    std::vector<QuestValidationError> const problems = QuestValidator::Validate(rows, MakeContext(rows));
    if (!problems.empty())
    {
        for (QuestValidationError const& problem : problems)
            errors.push_back(problem.Message);
        return false;
    }
    std::shared_ptr<QuestStore const> store = QuestStore::Build(rows);
    std::size_t const quests = store->GetQuestCount();
    std::size_t const goals = store->GetGoalCount();
    _quests.Replace(std::move(store));
    LogLoaded(quests, goals, 0);
    return true;
}

void QuestMgr::Clear()
{
    _quests.Replace(std::make_shared<QuestStore const>());
    std::lock_guard const lock(_sourceMutex);
    _source = nullptr;
    _keys = nullptr;
}
