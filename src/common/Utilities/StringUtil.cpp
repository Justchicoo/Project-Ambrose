/*
 * Project Ambrose by Imjustchico
 * Implements tokenizing, trimming, and ASCII case helpers without locale dependence.
 */

#include "StringUtil.h"

#include <algorithm>

namespace
{
    constexpr bool IsSpace(char c)
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    }

    constexpr char AsciiLower(char c)
    {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }

    constexpr char AsciiUpper(char c)
    {
        return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    }
}

std::vector<std::string_view> Ambrose::Tokenize(std::string_view str, char separator, bool keepEmpty)
{
    std::vector<std::string_view> tokens;
    std::size_t start = 0;
    while (true)
    {
        std::size_t const position = str.find(separator, start);
        std::string_view const token = str.substr(start, position == std::string_view::npos ? std::string_view::npos : position - start);
        if (keepEmpty || !token.empty())
            tokens.push_back(token);
        if (position == std::string_view::npos)
            break;
        start = position + 1;
    }
    return tokens;
}

std::string_view Ambrose::TrimLeft(std::string_view str)
{
    while (!str.empty() && IsSpace(str.front()))
        str.remove_prefix(1);
    return str;
}

std::string_view Ambrose::TrimRight(std::string_view str)
{
    while (!str.empty() && IsSpace(str.back()))
        str.remove_suffix(1);
    return str;
}

std::string_view Ambrose::Trim(std::string_view str)
{
    return TrimRight(TrimLeft(str));
}

std::string Ambrose::ToLower(std::string_view str)
{
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(), AsciiLower);
    return result;
}

std::string Ambrose::ToUpper(std::string_view str)
{
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(), AsciiUpper);
    return result;
}

bool Ambrose::EqualsIgnoreCase(std::string_view left, std::string_view right)
{
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), [](char a, char b) { return AsciiLower(a) == AsciiLower(b); });
}
