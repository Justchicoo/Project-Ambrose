/*
 * Project Ambrose by Imjustchico
 * Stores and loads a wizard's quest log in the characters database: read in one batch of queries through the wizard, so a missing wizard and a failed read come back as nothing, and saved whole in one transaction under the log's revision, which writes only while that revision is the newest, with the highest quest and goal GID kept in id_sequences, and the highest of those ever used, for the GID line to resume above.
 */

#ifndef AMBROSE_CHARACTERQUESTREPOSITORY_H
#define AMBROSE_CHARACTERQUESTREPOSITORY_H

#include "CharacterQuests.h"
#include "CharacterRepository.h"

#include <memory>
#include <optional>
#include <string_view>

struct CharacterQuestsLoad
{
    CharacterOpResult Result = CharacterOpResult::DatabaseError;
    std::optional<CharacterQuests> Quests;
};

class CharacterQuestRepository
{
public:
    using LoadHolder = std::shared_ptr<SQLQueryHolder<CharacterDatabaseConnection>>;

    static constexpr std::string_view GuidSequence = "quest";
    static constexpr std::size_t MaxNameBytes = 128;

    CharacterQuestRepository() = delete;

    static CharacterQuestsLoad Load(uint64 guid);
    static CharacterOpResult Save(uint64 guid, CharacterQuests const& quests);
    static std::optional<uint64> GetMaxGuid();

    static LoadHolder PrepareLoad(uint64 guid);
    static std::optional<CharacterQuests> Read(SQLQueryHolderBase const& holder);
    static CharacterRepository::CreateTransaction PrepareSave(uint64 guid, CharacterQuests const& quests);
    static bool IsValid(CharacterQuests const& quests) noexcept;
};

#endif
