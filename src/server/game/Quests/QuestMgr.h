/*
 * Project Ambrose by Imjustchico
 * The authored quests (sQuestMgr), read from the world database's quest tables and checked against object_template and, when it is loaded, the locale text: at start every quest that fails a check is left out and counted, and the reload target quest_template rebuilds every table and index off to the side and swaps them in only when every check passes, keeping the quests serving and reporting every error otherwise. Quest availability checks use the requirement manager's current immutable generation, and a caller holds the generation it was given for as long as it needs it.
 */

#ifndef AMBROSE_QUESTMGR_H
#define AMBROSE_QUESTMGR_H

#include "QuestRows.h"
#include "QuestStore.h"
#include "QuestValidator.h"
#include "ReloadableStore.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

class RequirementContext;

struct QuestLoadResult
{
    bool Loaded = false;
    std::size_t Quests = 0;
    std::size_t Goals = 0;
    std::vector<std::string> Errors;
};

class QuestMgr
{
public:
    static constexpr std::string_view Target = "quest_template";

    using RowSource = std::function<bool(QuestRows&, std::vector<std::string>&)>;
    using KeyLookup = QuestValidator::KeyLookup;

    static QuestMgr& Instance();

    QuestMgr() = default;
    QuestMgr(QuestMgr const&) = delete;
    QuestMgr& operator=(QuestMgr const&) = delete;

    static bool ReadWorldRows(QuestRows& rows, std::vector<std::string>& errors);

    void SetRowSource(RowSource source);
    void SetKeyLookup(KeyLookup lookup);
    void RegisterReloadTargets();
    QuestLoadResult LoadSkippingInvalid();
    bool Load(std::vector<std::string>& errors);
    bool CanOffer(std::string_view questName, RequirementContext const& context) const;
    std::vector<std::string> GetQuestsOfferedBy(uint32 templateId, RequirementContext const& context) const;
    std::shared_ptr<QuestStore const> GetQuests() const { return _quests.Get(); }
    uint64 GetGeneration() const noexcept { return _quests.GetGeneration(); }
    void Clear();

private:
    bool Read(QuestRows& rows, std::vector<std::string>& errors) const;
    QuestValidator::Context MakeContext(QuestRows const& rows) const;

    mutable std::mutex _sourceMutex;
    RowSource _source;
    KeyLookup _keys;
    ReloadableStore<QuestStore> _quests;
};

#define sQuestMgr QuestMgr::Instance()

#endif
