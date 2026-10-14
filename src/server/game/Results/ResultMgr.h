/*
 * Project Ambrose by Imjustchico
 * The results runner (sResultMgr): reads a result list's rows, each a result class and the versionable bytes the data holds it in, into decoded results once, and runs a list, named by the trigger or quest that holds it, in order for a wizard through the handler each owning system registers for a class, skipping and counting a class nobody handles, and stops at a ResWait, handing back the results after it and how long to wait before running them.
 */

#ifndef AMBROSE_RESULTMGR_H
#define AMBROSE_RESULTMGR_H

#include "TypeRegistry.h"
#include "Types.h"

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class GameSession;
class PropertyObject;

struct ResultRow
{
    uint32 Position = 0;
    std::string Class;
    std::vector<uint8> Data;
};

struct ResultEntry
{
    std::string Class;
    std::shared_ptr<PropertyObject const> Object;
    uint32 Position = 0;
};

using ResultList = std::vector<ResultEntry>;

struct ResultSource
{
    std::string Name;
    ResultList Results;
};

struct ResultContext
{
    GameSession* Session = nullptr;
    uint64 Wizard = 0;
    uint32 MapId = 0;
    std::string Zone;
    std::string Source;
    std::chrono::steady_clock::time_point Now;
};

using ResultHandler = std::function<void(PropertyObject const&, ResultContext const&)>;

struct ResultRun
{
    std::size_t Ran = 0;
    ResultList Rest;
    std::chrono::milliseconds Wait{ 0 };
};

class ResultMgr
{
public:
    static constexpr std::string_view WaitClass = "class ResWait";

    static ResultMgr& Instance();
    static std::shared_ptr<PropertyObject const> Decode(TypeCatalogPtr const& catalog, std::span<uint8 const> data, std::string& error);
    static ResultList Read(TypeCatalogPtr const& catalog, std::vector<ResultRow> const& rows, std::string_view source, std::vector<std::string>& errors);

    ResultMgr(ResultMgr const&) = delete;
    ResultMgr& operator=(ResultMgr const&) = delete;

    void Register(std::string_view className, ResultHandler handler);
    ResultRun Run(ResultList const& results, ResultContext const& context);

private:
    ResultMgr() = default;

    mutable std::mutex _mutex;
    std::map<std::string, ResultHandler, std::less<>> _handlers;
    std::map<std::string, std::size_t, std::less<>> _unhandled;
};

#define sResultMgr ResultMgr::Instance()

#endif
