/*
 * Project Ambrose by Imjustchico
 * The world data a new wizard is made from (sCharacterCreateStore): the schools a wizard may be given, read from character_create_school, and the state each school starts life in, read from playercreateinfo with school 0 standing for every school that has no row of its own. Both are read in one pass into a snapshot that is swapped in only when every row is valid, so a bad row leaves the server creating wizards from the rows it already had rather than from half of new ones, and every problem is named. A reader takes the snapshot by pointer and holds it for as long as it needs it, so a reload during a creation cannot pull the rows out from under it.
 */

#ifndef AMBROSE_CHARACTERCREATESTORE_H
#define AMBROSE_CHARACTERCREATESTORE_H

#include "Types.h"

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct CharacterStartState
{
    uint32 SchoolId = 0;
    int32 World = 0;
    std::string Zone;
    std::string ZoneDisplay;
    float PositionX = 0.0f;
    float PositionY = 0.0f;
    float PositionZ = 0.0f;
    float Orientation = 0.0f;
    int32 Level = 1;
    int32 Experience = 0;
};

struct CharacterSchool
{
    uint32 SchoolId = 0;
    std::string Name;
    uint16 SortOrder = 0;
};

class CharacterCreateSet
{
public:
    CharacterCreateSet(std::map<uint32, CharacterSchool> schools, std::map<uint32, CharacterStartState> starts);

    bool HasSchool(uint32 schoolId) const;
    std::optional<CharacterSchool> GetSchool(uint32 schoolId) const;
    CharacterStartState const* GetStart(uint32 schoolId) const;
    std::size_t GetSchoolCount() const noexcept;
    std::size_t GetStartCount() const noexcept;

private:
    std::map<uint32, CharacterSchool> _schools;
    std::map<uint32, CharacterStartState> _starts;
};

struct CharacterCreateLoadResult
{
    bool Loaded = false;
    std::vector<std::string> Errors;
    std::vector<std::string> Warnings;
    std::size_t Schools = 0;
    std::size_t Starts = 0;
};

class CharacterCreateStore
{
public:
    static CharacterCreateStore& Instance();

    CharacterCreateStore();
    CharacterCreateStore(CharacterCreateStore const&) = delete;
    CharacterCreateStore& operator=(CharacterCreateStore const&) = delete;

    CharacterCreateLoadResult Load();
    std::shared_ptr<CharacterCreateSet const> Get() const;
    uint64 GetGeneration() const;
    void Set(std::shared_ptr<CharacterCreateSet const> rows);
    void Clear();

private:
    mutable std::mutex _guard;
    std::shared_ptr<CharacterCreateSet const> _rows;
    uint64 _generation = 0;
};

#define sCharacterCreateStore CharacterCreateStore::Instance()

#endif
