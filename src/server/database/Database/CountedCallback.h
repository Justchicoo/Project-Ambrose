/*
 * Project Ambrose by Imjustchico
 * The pending outcome of an async statement that changes rows, carrying how many it changed so a caller can tell a conditional write that landed from one that matched nothing, and carrying nothing at all when the statement never ran.
 */

#ifndef AMBROSE_COUNTEDCALLBACK_H
#define AMBROSE_COUNTEDCALLBACK_H

#include "Types.h"

#include <functional>
#include <future>
#include <optional>

class CountedCallback
{
public:
    explicit CountedCallback(std::future<std::optional<uint64>>&& result);

    CountedCallback(CountedCallback&&) noexcept = default;
    CountedCallback& operator=(CountedCallback&&) noexcept = default;
    CountedCallback(CountedCallback const&) = delete;
    CountedCallback& operator=(CountedCallback const&) = delete;

    CountedCallback&& AfterComplete(std::function<void(std::optional<uint64>)>&& callback);

    bool IsReady() const;
    bool InvokeIfReady();

private:
    std::future<std::optional<uint64>> _result;
    std::function<void(std::optional<uint64>)> _callback;
    bool _finished = false;
};

#endif
