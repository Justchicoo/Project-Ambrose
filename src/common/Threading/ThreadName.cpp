/*
 * Project Ambrose by Imjustchico
 * Thread naming through SetThreadDescription on Windows and pthread names on Linux, truncated to 15 bytes.
 */

#include "ThreadName.h"
#include "Utf.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace
{
    std::string_view Truncate(std::string_view name)
    {
        if (name.size() <= Ambrose::Threading::MaxPortableThreadNameLength)
            return name;
        std::size_t length = Ambrose::Threading::MaxPortableThreadNameLength;
        while (length > 0 && (static_cast<unsigned char>(name[length]) & 0xC0) == 0x80)
            --length;
        return name.substr(0, length);
    }
}

bool Ambrose::Threading::SetCurrentThreadName(std::string_view name)
{
    std::string const truncated(Truncate(name));
#ifdef _WIN32
    std::optional<std::u16string> const wide = Utf::Utf8ToUtf16(truncated, Utf::InvalidPolicy::ReplaceWithU_FFFD);
    if (!wide)
        return false;
    return SUCCEEDED(::SetThreadDescription(::GetCurrentThread(), reinterpret_cast<wchar_t const*>(wide->c_str())));
#else
    return ::pthread_setname_np(::pthread_self(), truncated.c_str()) == 0;
#endif
}

std::string Ambrose::Threading::GetCurrentThreadName()
{
#ifdef _WIN32
    wchar_t* description = nullptr;
    if (FAILED(::GetThreadDescription(::GetCurrentThread(), &description)) || description == nullptr)
        return {};
    std::u16string const wide(reinterpret_cast<char16_t const*>(description));
    ::LocalFree(description);
    return Utf::Utf16ToUtf8(wide, Utf::InvalidPolicy::ReplaceWithU_FFFD).value_or(std::string());
#else
    char buffer[64] = {};
    if (::pthread_getname_np(::pthread_self(), buffer, sizeof(buffer)) != 0)
        return {};
    return buffer;
#endif
}
