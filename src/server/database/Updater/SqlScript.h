/*
 * Project Ambrose by Imjustchico
 * Splits SQL update files into statements the way the server reads them, skipping quotes and comments, so the updater can count statements, show the one that failed, and refuse DELIMITER.
 */

#ifndef AMBROSE_SQLSCRIPT_H
#define AMBROSE_SQLSCRIPT_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace SqlScript
{
    struct Statement
    {
        std::string_view Text;
        std::size_t Line = 0;
    };

    std::string_view StripByteOrderMark(std::string_view sql) noexcept;
    bool Split(std::string_view sql, std::vector<Statement>& statements, std::string& error);
    std::string Excerpt(std::string_view statement, std::size_t maxLength = 200);
}

#endif
