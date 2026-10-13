/*
 * Project Ambrose by Imjustchico
 * Decodes each result row's versionable bytes, refusing one that does not read or holds another class than its row names, and runs a list's results in order through their registered handlers, an empty handler taking a class's away, logging the first result of each class no system handles and counting every one.
 */

#include "ResultMgr.h"
#include "Log.h"
#include "ObjectSerializer.h"
#include "PropertyObject.h"

#include <fmt/format.h>

#include <utility>

ResultMgr& ResultMgr::Instance()
{
    static ResultMgr instance;
    return instance;
}

std::shared_ptr<PropertyObject const> ResultMgr::Decode(TypeCatalogPtr const& catalog, std::span<uint8 const> data, std::string& error)
{
    if (!catalog)
    {
        error = "no type catalog is loaded";
        return nullptr;
    }
    SerializerOptions options;
    options.Versionable = true;
    options.Flags = SerializerFlag::None;
    options.Mask = 0;
    options.AllowNullRoot = false;
    options.AllowTrailingBytes = false;
    DecodeResult decoded = ObjectSerializer::Decode(catalog, data, options);
    if (!decoded.Ok() || !decoded.Object)
    {
        error = decoded.Detail.empty() ? std::string(ObjectSerializer::GetStatusName(decoded.Status)) : decoded.Detail;
        return nullptr;
    }
    return std::shared_ptr<PropertyObject const>(std::move(decoded.Object));
}

ResultList ResultMgr::Read(TypeCatalogPtr const& catalog, std::vector<ResultRow> const& rows, std::string_view source, std::vector<std::string>& errors)
{
    ResultList results;
    results.reserve(rows.size());
    for (ResultRow const& row : rows)
    {
        std::string error;
        std::shared_ptr<PropertyObject const> object = Decode(catalog, row.Data, error);
        if (object && !object->IsA(row.Class))
            error = fmt::format("it holds a {}", object->GetClass().Name);
        if (!error.empty())
        {
            errors.push_back(fmt::format("{} has a {} result at {} that does not read: {}", source, row.Class, row.Position, error));
            continue;
        }
        results.push_back({ row.Class, std::move(object), row.Position });
    }
    return results;
}

void ResultMgr::Register(std::string_view className, ResultHandler handler)
{
    std::lock_guard const lock(_mutex);
    if (!handler)
        _handlers.erase(std::string(className));
    else
        _handlers.insert_or_assign(std::string(className), std::move(handler));
}

std::size_t ResultMgr::Run(ResultList const& results, ResultContext const& context)
{
    std::size_t ran = 0;
    for (ResultEntry const& result : results)
    {
        ResultHandler handler;
        {
            std::lock_guard const lock(_mutex);
            auto const found = _handlers.find(result.Class);
            if (found == _handlers.end())
            {
                if (_unhandled[result.Class]++ == 0)
                    LOG_INFO("server.results", "{} runs a {} result, which no system handles yet, so it and every later one is skipped", context.Source, result.Class);
                continue;
            }
            handler = found->second;
        }
        if (result.Object)
        {
            handler(*result.Object, context);
            ++ran;
        }
    }
    return ran;
}
