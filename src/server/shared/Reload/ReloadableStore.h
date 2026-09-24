/*
 * Project Ambrose by Imjustchico
 * A store that is replaced rather than edited: readers take a snapshot and hold it for as long as they need it, a reload builds the next one off to the side and puts it in place in a single step, and a reader that was already holding one keeps the whole generation it started with rather than seeing half of each. A generation number says how many times the store has been replaced, and a store that has never been filled still answers with an empty one, so nothing has to ask whether it is ready.
 */

#ifndef AMBROSE_RELOADABLESTORE_H
#define AMBROSE_RELOADABLESTORE_H

#include "Types.h"

#include <atomic>
#include <memory>
#include <utility>

template<typename T>
class ReloadableStore
{
public:
    using Snapshot = std::shared_ptr<T const>;

    ReloadableStore() : _snapshot(std::make_shared<T const>())
    {
    }

    explicit ReloadableStore(T initial) : _snapshot(std::make_shared<T const>(std::move(initial)))
    {
    }

    ReloadableStore(ReloadableStore const&) = delete;
    ReloadableStore& operator=(ReloadableStore const&) = delete;

    Snapshot Get() const
    {
        return _snapshot.load(std::memory_order_acquire);
    }

    uint64 GetGeneration() const noexcept
    {
        return _generation.load(std::memory_order_acquire);
    }

    void Replace(T next)
    {
        Replace(std::make_shared<T const>(std::move(next)));
    }

    void Replace(Snapshot next)
    {
        if (!next)
            return;
        _snapshot.store(std::move(next), std::memory_order_release);
        _generation.fetch_add(1, std::memory_order_acq_rel);
    }

    long RetiredHolders() const
    {
        Snapshot const held = Get();
        return held.use_count() - 2;
    }

private:
    std::atomic<Snapshot> _snapshot;
    std::atomic<uint64> _generation{ 0 };
};

#endif
