/*
 * Project Ambrose by Imjustchico
 * String helpers: tokenizing, trimming, ASCII case handling, checked number parsing, and formatting.
 */

#ifndef AMBROSE_STRINGUTIL_H
#define AMBROSE_STRINGUTIL_H

#include <fmt/format.h>

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace Ambrose
{
    std::vector<std::string_view> Tokenize(std::string_view str, char separator, bool keepEmpty);

    std::string_view TrimLeft(std::string_view str);
    std::string_view TrimRight(std::string_view str);
    std::string_view Trim(std::string_view str);

    std::string ToLower(std::string_view str);
    std::string ToUpper(std::string_view str);
    bool EqualsIgnoreCase(std::string_view left, std::string_view right);

    template<typename T>
        requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
    std::optional<T> StringTo(std::string_view str, int base = 10)
    {
        T value{};
        char const* const begin = str.data();
        char const* const end = str.data() + str.size();
        auto const [pointer, error] = std::from_chars(begin, end, value, base);
        if (str.empty() || error != std::errc() || pointer != end)
            return std::nullopt;
        return value;
    }

    template<typename T>
        requires std::is_floating_point_v<T>
    std::optional<T> StringTo(std::string_view str)
    {
        T value{};
        char const* const begin = str.data();
        char const* const end = str.data() + str.size();
        auto const [pointer, error] = std::from_chars(begin, end, value);
        if (str.empty() || error != std::errc() || pointer != end)
            return std::nullopt;
        return value;
    }

    template<typename T>
        requires std::is_same_v<T, bool>
    std::optional<T> StringTo(std::string_view str)
    {
        if (str == "1" || EqualsIgnoreCase(str, "true") || EqualsIgnoreCase(str, "yes"))
            return true;
        if (str == "0" || EqualsIgnoreCase(str, "false") || EqualsIgnoreCase(str, "no"))
            return false;
        return std::nullopt;
    }

    template<typename... Args>
    std::string StringFormat(fmt::format_string<Args...> format, Args&&... args)
    {
        return fmt::format(format, std::forward<Args>(args)...);
    }
}

#endif
