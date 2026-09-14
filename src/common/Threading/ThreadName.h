/*
 * Project Ambrose by Imjustchico
 * Sets and reads the current thread's name so debuggers, profilers, and crash dumps show what each thread does.
 */

#ifndef AMBROSE_THREADNAME_H
#define AMBROSE_THREADNAME_H

#include <cstddef>
#include <string>
#include <string_view>

namespace Ambrose::Threading
{
    inline constexpr std::size_t MaxPortableThreadNameLength = 15;

    bool SetCurrentThreadName(std::string_view name);
    std::string GetCurrentThreadName();
}

#endif
