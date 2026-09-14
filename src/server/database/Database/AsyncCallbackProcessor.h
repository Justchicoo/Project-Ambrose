/*
 * Project Ambrose by Imjustchico
 * Holds pending async database callbacks for one owner and runs the ready ones when its update loop polls, isolating a callback that throws and keeping callbacks added while processing for the next poll.
 */

#ifndef AMBROSE_ASYNCCALLBACKPROCESSOR_H
#define AMBROSE_ASYNCCALLBACKPROCESSOR_H

#include "Log.h"
#include "QueryCallback.h"
#include "QueryHolder.h"
#include "Transaction.h"

#include <cstddef>
#include <exception>
#include <iterator>
#include <utility>
#include <vector>

template<typename CallbackType>
class AsyncCallbackProcessor
{
public:
    AsyncCallbackProcessor() = default;

    AsyncCallbackProcessor(AsyncCallbackProcessor const&) = delete;
    AsyncCallbackProcessor& operator=(AsyncCallbackProcessor const&) = delete;

    void AddCallback(CallbackType&& callback)
    {
        _callbacks.emplace_back(std::move(callback));
    }

    void ProcessReadyCallbacks()
    {
        if (_callbacks.empty() || _processing)
            return;
        _processing = true;
        _clearRequested = false;
        std::vector<CallbackType> processing = std::move(_callbacks);
        _callbacks.clear();
        std::vector<CallbackType> pending;
        pending.reserve(processing.size());
        for (CallbackType& callback : processing)
        {
            if (_clearRequested)
                break;
            bool finished = true;
            try
            {
                finished = callback.InvokeIfReady();
            }
            catch (std::exception const& exception)
            {
                LOG_ERROR("sql.sql", "A database callback threw and was dropped: {}", exception.what());
            }
            catch (...)
            {
                LOG_ERROR("sql.sql", "A database callback threw an unknown exception and was dropped");
            }
            if (!finished)
                pending.push_back(std::move(callback));
        }
        _processing = false;
        if (_clearRequested)
        {
            _callbacks.clear();
            _clearRequested = false;
            return;
        }
        pending.insert(pending.end(), std::make_move_iterator(_callbacks.begin()), std::make_move_iterator(_callbacks.end()));
        _callbacks = std::move(pending);
    }

    std::size_t GetPendingCount() const noexcept { return _callbacks.size(); }

    void Clear()
    {
        _callbacks.clear();
        if (_processing)
            _clearRequested = true;
    }

private:
    std::vector<CallbackType> _callbacks;
    bool _processing = false;
    bool _clearRequested = false;
};

using QueryCallbackProcessor = AsyncCallbackProcessor<QueryCallback>;
using TransactionCallbackProcessor = AsyncCallbackProcessor<TransactionCallback>;
using QueryHolderCallbackProcessor = AsyncCallbackProcessor<SQLQueryHolderCallback>;

#endif
