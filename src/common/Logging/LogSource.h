/*
 * Project Ambrose by Imjustchico
 * Where in the code a log record was written: the repository-relative file, the line and the function, taken at compile time so a record costs no extra work, with the build machine's own folder trimmed off the front of __FILE__ by a comparison the compiler performs and never emits. The macro is parenthesized because its commas would otherwise be read as argument separators wherever a logging call sits inside another macro, such as a test's EXPECT_NO_THROW.
 */

#ifndef AMBROSE_LOGSOURCE_H
#define AMBROSE_LOGSOURCE_H

#include "Types.h"

#include <string>
#include <string_view>

#ifndef AMBROSE_SOURCE_ROOT
#define AMBROSE_SOURCE_ROOT ""
#endif

struct LogSource
{
    std::string_view File;
    std::string_view Function;
    uint32 Line = 0;

    constexpr bool Known() const noexcept { return !File.empty() && Line != 0; }
};

namespace LogSourcePath
{
constexpr bool SameChar(char left, char right) noexcept
{
    char const a = left == '\\' ? '/' : (left >= 'A' && left <= 'Z' ? static_cast<char>(left - 'A' + 'a') : left);
    char const b = right == '\\' ? '/' : (right >= 'A' && right <= 'Z' ? static_cast<char>(right - 'A' + 'a') : right);
    return a == b;
}

constexpr std::string_view WithoutRoot(std::string_view file) noexcept
{
    constexpr std::string_view root{ AMBROSE_SOURCE_ROOT };
    if (root.empty() || file.size() <= root.size())
        return file;
    for (std::size_t index = 0; index < root.size(); ++index)
        if (!SameChar(file[index], root[index]))
            return file;
    file.remove_prefix(root.size());
    return file;
}

std::string Portable(std::string_view file);
}

#define AMBROSE_LOG_SOURCE \
    (LogSource{ LogSourcePath::WithoutRoot(__FILE__), std::string_view(__func__), static_cast<uint32>(__LINE__) })

#endif
