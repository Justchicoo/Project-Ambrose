/*
 * Project Ambrose by Imjustchico
 * Holds pending async database callbacks for one owner, accepts them safely across threads, and runs ready ones when polled, isolating a callback that throws and honoring overlapping polls after the active batch.
 */

#ifndef AMBROSE_ASYNCCALLBACKPROCESSOR_H
#define AMBROSE_ASYNCCALLBACKPROCESSOR_H

#include "CountedCallback.h"
#include "Log.h"
#include "QueryCallback.h"
#include "QueryHolder.h"
#include "Transaction.h"

#include <cstddef>
#include <exception>
#include <iterator>
#include <mutex>
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
        std::lock_guard const lock(_mutex);
        _callbacks.emplace_back(std::move(callback));
    }

    void ProcessReadyCallbacks()
    {
        std::vector<CallbackType> processing;
        {
            std::lock_guard const lock(_mutex);
            if (_processing)
            {
                _processAgain = true;
                return;
            }
            if (_callbacks.empty())
                return;
            _processing = true;
            _clearRequested = false;
            processing = std::move(_callbacks);
            _callbacks.clear();
        }
        while (true)
        {
            std::vector<CallbackType> pending;
            pending.reserve(processing.size());
            for (CallbackType& callback : processing)
            {
                {
                    std::lock_guard const lock(_mutex);
                    if (_clearRequested)
                        break;
                }
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
            {
                std::lock_guard const lock(_mutex);
                if (_clearRequested)
                {
                    _callbacks.clear();
                    _clearRequested = false;
                    _processAgain = false;
                    _processing = false;
                    return;
                }
                pending.insert(pending.end(), std::make_move_iterator(_callbacks.begin()), std::make_move_iterator(_callbacks.end()));
                _callbacks = std::move(pending);
                if (!std::exchange(_processAgain, false))
                {
                    _processing = false;
                    return;
                }
                processing = std::move(_callbacks);
                _callbacks.clear();
            }
        }
    }

    std::size_t GetPendingCount() const noexcept
    {
        std::lock_guard const lock(_mutex);
        return _callbacks.size();
    }

    void Clear()
    {
        std::lock_guard const lock(_mutex);
        _callbacks.clear();
        if (_processing)
            _clearRequested = true;
    }

private:
    mutable std::mutex _mutex;
    std::vector<CallbackType> _callbacks;
    bool _processing = false;
    bool _clearRequested = false;
    bool _processAgain = false;
};

using QueryCallbackProcessor = AsyncCallbackProcessor<QueryCallback>;
using CountedCallbackProcessor = AsyncCallbackProcessor<CountedCallback>;
using TransactionCallbackProcessor = AsyncCallbackProcessor<TransactionCallback>;
using QueryHolderCallbackProcessor = AsyncCallbackProcessor<SQLQueryHolderCallback>;

#endif
