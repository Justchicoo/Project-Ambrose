/*
 * Project Ambrose by Imjustchico
 * Walks SQL text tracking quoted strings, comments, executable version comments and stored-routine BEGIN...END bodies, and cuts statements at top-level semicolons the way the server counts them; a statement counts as data-only when its leading words make it a row change, a read that writes no file, a transaction step, a table lock or a session-scoped SET, and anything else, an executable comment included, counts as able to change the schema.
 */

#include "SqlScript.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>

namespace
{
    bool IsWordCharacter(char c)
    {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$';
    }

    std::string_view WordAt(std::string_view sql, std::size_t position)
    {
        std::size_t end = position;
        while (end < sql.size() && IsWordCharacter(sql[end]))
            ++end;
        return sql.substr(position, end - position);
    }

    char NextCharacter(std::string_view sql, std::size_t position)
    {
        std::size_t const found = sql.find_first_not_of(" \t\r\n", position);
        return found == std::string_view::npos ? '\0' : sql[found];
    }

    std::string_view NextWord(std::string_view sql, std::size_t position)
    {
        while (position < sql.size() && std::isspace(static_cast<unsigned char>(sql[position])))
            ++position;
        return WordAt(sql, position);
    }

    bool Is(std::string_view word, std::string_view keyword)
    {
        return Ambrose::EqualsIgnoreCase(word, keyword);
    }

    bool DeclaresRoutine(std::string_view statementSoFar)
    {
        std::string const upper = Ambrose::ToUpper(statementSoFar);
        if (upper.rfind("CREATE", 0) != 0)
            return false;
        for (std::string_view const kind : { " PROCEDURE ", " FUNCTION ", " TRIGGER ", " EVENT " })
            if (upper.find(kind) != std::string::npos)
                return true;
        return false;
    }

    bool OpensExecutableComment(std::string_view sql, std::size_t position)
    {
        return position + 2 < sql.size() && (sql[position + 2] == '!' || sql[position + 2] == '+' || (sql[position + 2] == 'M' && position + 3 < sql.size() && sql[position + 3] == '!'));
    }

    bool OpensLineComment(std::string_view sql, std::size_t position)
    {
        char const c = sql[position];
        return (c == '-' && position + 1 < sql.size() && sql[position + 1] == '-' && (position + 2 >= sql.size() || std::isspace(static_cast<unsigned char>(sql[position + 2])))) || c == '#';
    }

    std::vector<std::string> LeadingWords(std::string_view sql, std::size_t count)
    {
        std::vector<std::string> words;
        std::size_t i = 0;
        while (i < sql.size() && words.size() < count)
        {
            char const c = sql[i];
            if (std::isspace(static_cast<unsigned char>(c)))
            {
                ++i;
                continue;
            }
            if (OpensLineComment(sql, i))
            {
                std::size_t const end = sql.find('\n', i);
                i = end == std::string_view::npos ? sql.size() : end + 1;
                continue;
            }
            if (c == '/' && i + 1 < sql.size() && sql[i + 1] == '*' && !OpensExecutableComment(sql, i))
            {
                std::size_t const end = sql.find("*/", i + 2);
                if (end == std::string_view::npos)
                    break;
                i = end + 2;
                continue;
            }
            if (!IsWordCharacter(c))
                break;
            std::string_view const word = WordAt(sql, i);
            words.push_back(Ambrose::ToUpper(word));
            i += word.size();
        }
        return words;
    }

    std::size_t SkipByteOrderMark(std::string_view sql)
    {
        return sql.size() >= 3 && static_cast<unsigned char>(sql[0]) == 0xEF && static_cast<unsigned char>(sql[1]) == 0xBB && static_cast<unsigned char>(sql[2]) == 0xBF ? 3 : 0;
    }
}

std::string_view SqlScript::StripByteOrderMark(std::string_view sql) noexcept
{
    return sql.substr(SkipByteOrderMark(sql));
}

bool SqlScript::Split(std::string_view input, std::vector<Statement>& statements, std::string& error)
{
    statements.clear();
    std::string_view const sql = StripByteOrderMark(input);
    std::size_t line = 1;
    std::size_t codeStart = 0;
    std::size_t statementLine = 1;
    bool sawCode = false;
    bool sawSeparator = false;
    int blockDepth = 0;
    bool statementStart = true;
    std::size_t i = 0;

    auto beginCode = [&]
    {
        if (sawCode)
            return;
        sawCode = true;
        statementLine = line;
        codeStart = i;
    };
    auto flush = [&](std::size_t end, bool atSeparator)
    {
        if (sawCode)
            statements.push_back(Statement{ Ambrose::Trim(sql.substr(codeStart, end - codeStart)), statementLine });
        else if (atSeparator && sawSeparator)
            statements.push_back(Statement{ std::string_view(), line });
        sawCode = false;
        blockDepth = 0;
        statementStart = true;
    };

    while (i < sql.size())
    {
        char const c = sql[i];
        if (c == '\n')
        {
            ++line;
            ++i;
            continue;
        }
        if (c == '\'' || c == '"' || c == '`')
        {
            beginCode();
            statementStart = false;
            char const quote = c;
            std::size_t const openedLine = line;
            ++i;
            bool closed = false;
            while (i < sql.size())
            {
                if (sql[i] == '\n')
                    ++line;
                if (sql[i] == '\\' && quote != '`' && i + 1 < sql.size())
                {
                    if (sql[i + 1] == '\n')
                        ++line;
                    i += 2;
                    continue;
                }
                if (sql[i] == quote)
                {
                    if (i + 1 < sql.size() && sql[i + 1] == quote)
                    {
                        i += 2;
                        continue;
                    }
                    ++i;
                    closed = true;
                    break;
                }
                ++i;
            }
            if (!closed)
            {
                error = fmt::format("line {}: a {} quote is never closed", openedLine, quote == '`' ? "backtick" : quote == '"' ? "double" : "single");
                return false;
            }
            continue;
        }
        if (OpensLineComment(sql, i))
        {
            while (i < sql.size() && sql[i] != '\n')
                ++i;
            continue;
        }
        if (c == '/' && i + 1 < sql.size() && sql[i + 1] == '*')
        {
            bool const executable = OpensExecutableComment(sql, i);
            std::size_t const end = sql.find("*/", i + 2);
            if (end == std::string_view::npos)
            {
                error = fmt::format("line {}: a /* comment is never closed", line);
                return false;
            }
            if (executable)
            {
                beginCode();
                statementStart = false;
            }
            line += static_cast<std::size_t>(std::count(sql.begin() + static_cast<std::ptrdiff_t>(i), sql.begin() + static_cast<std::ptrdiff_t>(end), '\n'));
            i = end + 2;
            continue;
        }
        if (c == ';')
        {
            if (blockDepth > 0)
            {
                statementStart = true;
                ++i;
                continue;
            }
            flush(i, true);
            sawSeparator = true;
            ++i;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c)))
        {
            ++i;
            continue;
        }
        if (IsWordCharacter(c))
        {
            bool const wasStart = statementStart;
            if (!sawCode)
            {
                beginCode();
                if (Is(WordAt(sql, i), "DELIMITER"))
                {
                    error = fmt::format("line {}: DELIMITER is not allowed; update files run through the connector, which needs no delimiter changes", line);
                    return false;
                }
            }
            std::string_view const word = WordAt(sql, i);
            statementStart = false;
            bool const inRoutine = blockDepth > 0 || DeclaresRoutine(sql.substr(codeStart, i - codeStart));
            if (inRoutine)
            {
                if (Is(word, "BEGIN") || Is(word, "CASE"))
                {
                    ++blockDepth;
                    statementStart = Is(word, "BEGIN");
                }
                else if ((Is(word, "IF") || Is(word, "LOOP") || Is(word, "WHILE") || Is(word, "REPEAT")) && wasStart)
                {
                    if (!(Is(word, "IF") && NextCharacter(sql, i + word.size()) == '('))
                        ++blockDepth;
                    statementStart = Is(word, "LOOP") || Is(word, "REPEAT");
                }
                else if (Is(word, "END") && blockDepth > 0)
                {
                    --blockDepth;
                    std::string_view const closing = NextWord(sql, i + word.size());
                    if (Is(closing, "IF") || Is(closing, "CASE") || Is(closing, "LOOP") || Is(closing, "WHILE") || Is(closing, "REPEAT"))
                    {
                        std::size_t const closingAt = sql.find_first_not_of(" \t\r\n", i + word.size());
                        line += static_cast<std::size_t>(std::count(sql.begin() + static_cast<std::ptrdiff_t>(i), sql.begin() + static_cast<std::ptrdiff_t>(closingAt), '\n'));
                        i = closingAt + closing.size();
                        continue;
                    }
                }
                else if (Is(word, "THEN") || Is(word, "ELSE") || Is(word, "DO"))
                    statementStart = true;
            }
            i += word.size();
            if (i < sql.size() && sql[i] == ':' && wasStart)
                statementStart = true;
            continue;
        }
        beginCode();
        statementStart = false;
        ++i;
    }
    flush(sql.size(), false);
    return true;
}

std::string SqlScript::Excerpt(std::string_view statement, std::size_t maxLength)
{
    std::string text;
    text.reserve(std::min(statement.size(), maxLength));
    bool space = false;
    for (char const c : statement)
    {
        if (std::isspace(static_cast<unsigned char>(c)))
        {
            space = !text.empty();
            continue;
        }
        if (space)
            text.push_back(' ');
        space = false;
        text.push_back(c);
        if (text.size() >= maxLength)
        {
            text += "...";
            break;
        }
    }
    return text;
}

std::string SqlScript::LeadingKeyword(std::string_view statement)
{
    std::vector<std::string> words = LeadingWords(statement, 1);
    return words.empty() ? std::string() : std::move(words.front());
}

bool SqlScript::IsDataOnly(std::string_view statement)
{
    std::vector<std::string> const words = LeadingWords(statement, 2);
    if (words.empty())
        return Ambrose::Trim(statement).empty();
    std::string const& first = words[0];
    std::string_view const second = words.size() > 1 ? std::string_view(words[1]) : std::string_view();
    if (first == "INSERT" || first == "REPLACE" || first == "UPDATE" || first == "DELETE" || first == "COMMIT" || first == "ROLLBACK" || first == "SAVEPOINT")
        return true;
    std::string const upper = Ambrose::ToUpper(statement);
    if (first == "SELECT" || first == "WITH")
        return upper.find("OUTFILE") == std::string::npos && upper.find("DUMPFILE") == std::string::npos;
    if (first == "START")
        return second == "TRANSACTION";
    if (first == "BEGIN")
        return second.empty() || second == "WORK";
    if (first == "RELEASE")
        return second == "SAVEPOINT";
    if (first == "LOCK" || first == "UNLOCK")
        return second == "TABLES" || second == "TABLE";
    if (first == "SET")
    {
        for (std::string_view const scope : { "GLOBAL", "PERSIST", "PERSIST_ONLY", "PASSWORD", "ROLE", "DEFAULT", "STATEMENT" })
            if (second == scope)
                return false;
        return upper.find("@@GLOBAL") == std::string::npos && upper.find("@@PERSIST") == std::string::npos;
    }
    return false;
}
