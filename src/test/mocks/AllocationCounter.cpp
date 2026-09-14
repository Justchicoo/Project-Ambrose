/*
 * Project Ambrose by Imjustchico
 * Records allocations per thread through sanitizer hooks in sanitizer builds and by replacing operator new otherwise.
 */

#include "AllocationCounter.h"

#include <cstdlib>
#include <new>

namespace
{
    struct AllocationState
    {
        bool Active;
        std::size_t Count;
        std::size_t Largest;
    };

    thread_local AllocationState State{ false, 0, 0 };

    void Record(std::size_t size)
    {
        if (!State.Active)
            return;
        ++State.Count;
        if (size > State.Largest)
            State.Largest = size;
    }
}

AllocationScope::AllocationScope()
{
    State = AllocationState{ true, 0, 0 };
}

AllocationScope::~AllocationScope()
{
    State.Active = false;
}

std::size_t AllocationScope::GetCount() const
{
    return State.Count;
}

std::size_t AllocationScope::GetLargest() const
{
    return State.Largest;
}

#if (defined(AMBROSE_SANITIZE_ADDRESS) || defined(AMBROSE_SANITIZE_THREAD)) && !defined(_MSC_VER)

extern "C" int __sanitizer_install_malloc_and_free_hooks(void (*mallocHook)(void const*, std::size_t), void (*freeHook)(void const*));

namespace
{
    void MallocHook(void const*, std::size_t size)
    {
        Record(size);
    }

    void FreeHook(void const*)
    {
    }

    int const HooksInstalled = __sanitizer_install_malloc_and_free_hooks(MallocHook, FreeHook);
}

bool AllocationScope::IsSupported()
{
    return HooksInstalled != 0;
}

#elif defined(AMBROSE_SANITIZE_ADDRESS)

bool AllocationScope::IsSupported()
{
    return false;
}

#else

bool AllocationScope::IsSupported()
{
    return true;
}

namespace
{
    void* Allocate(std::size_t size)
    {
        Record(size);
        if (void* pointer = std::malloc(size == 0 ? 1 : size))
            return pointer;
        throw std::bad_alloc();
    }
}

void* operator new(std::size_t size)
{
    return Allocate(size);
}

void* operator new[](std::size_t size)
{
    return Allocate(size);
}

void operator delete(void* pointer) noexcept
{
    std::free(pointer);
}

void operator delete[](void* pointer) noexcept
{
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept
{
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept
{
    std::free(pointer);
}

#endif
