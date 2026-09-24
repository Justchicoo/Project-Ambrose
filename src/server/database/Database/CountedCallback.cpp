/*
 * Project Ambrose by Imjustchico
 * Runs the waiting caller once the row count is settled, and treats a statement that threw the same as one that never ran, because a caller that cannot learn how many rows it changed must not act as though it changed any.
 */

#include "CountedCallback.h"
#include "Log.h"

#include <chrono>
#include <exception>
#include <utility>

CountedCallback::CountedCallback(std::future<std::optional<uint64>>&& result) : _result(std::move(result))
{
}

CountedCallback&& CountedCallback::AfterComplete(std::function<void(std::optional<uint64>)>&& callback)
{
    _callback = std::move(callback);
    return std::move(*this);
}

bool CountedCallback::IsReady() const
{
    return _result.valid() && _result.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

bool CountedCallback::InvokeIfReady()
{
    if (_finished)
        return true;
    if (!IsReady())
        return false;
    _finished = true;
    std::optional<uint64> affected;
    try
    {
        affected = _result.get();
    }
    catch (std::exception const& exception)
    {
        LOG_ERROR("sql.sql", "An async counted statement failed: {}", exception.what());
    }
    catch (...)
    {
        LOG_ERROR("sql.sql", "An async counted statement failed with an unknown exception");
    }
    if (_callback)
        _callback(affected);
    return true;
}
