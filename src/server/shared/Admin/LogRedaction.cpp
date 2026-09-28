/*
 * Project Ambrose by Imjustchico
 * Asks the settings table which settings are secrets, masks a connection string's password segment, each key of a key list while keeping its id, and a token's whole value, leaves an empty value empty because it hides nothing, scrubs any text that quotes a secret setting's value after its key, reading a key list through its commas, and masks a configuration file one Key = value line at a time, keeping its byte order mark, comments, spacing, quotes and line endings and naming each key it hid.
 */

#include "LogRedaction.h"
#include "SettingDeclarations.h"
#include "StringUtil.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace
{
    bool EndsWith(std::string_view text, std::string_view suffix) noexcept
    {
        return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
    }

    std::string MaskConnectionString(std::string_view value)
    {
        std::string masked;
        std::size_t field = 0;
        std::size_t start = 0;
        while (true)
        {
            std::size_t const end = value.find(';', start);
            std::string_view const part = value.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
            if (field == 3)
                masked.append(LogRedaction::Mask);
            else
                masked.append(part);
            if (end == std::string_view::npos)
                break;
            masked.push_back(';');
            start = end + 1;
            ++field;
        }
        return masked;
    }

    bool IsKeyList(std::string_view lowered) noexcept
    {
        return lowered == "account.verifierkeys";
    }

    std::string MaskKeyList(std::string_view value)
    {
        std::string masked;
        std::size_t start = 0;
        while (true)
        {
            std::size_t const end = value.find(',', start);
            std::string_view const part = value.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
            std::size_t const colon = part.find(':');
            std::string_view const id = colon == std::string_view::npos ? std::string_view() : Ambrose::Trim(part.substr(0, colon));
            if (!id.empty() && std::all_of(id.begin(), id.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }))
                masked.append(id).push_back(':');
            masked.append(LogRedaction::Mask);
            if (end == std::string_view::npos)
                break;
            masked.push_back(',');
            start = end + 1;
        }
        return masked;
    }

    bool IsKeyCharacter(char character) noexcept
    {
        return std::isalnum(static_cast<unsigned char>(character)) || character == '.' || character == '_';
    }

    std::string RedactConfLine(std::string_view line, std::vector<std::string>* redactedKeys)
    {
        std::size_t bodyEnd = line.size();
        if (bodyEnd > 0 && line[bodyEnd - 1] == '\n')
            --bodyEnd;
        if (bodyEnd > 0 && line[bodyEnd - 1] == '\r')
            --bodyEnd;
        std::string_view const body = line.substr(0, bodyEnd);
        std::string_view const ending = line.substr(bodyEnd);
        std::size_t const start = body.find_first_not_of(" \t");
        if (start == std::string_view::npos || body[start] == '#')
            return std::string(line);
        std::size_t const equals = body.find('=', start);
        if (equals == std::string_view::npos)
            return std::string(line);
        std::string_view const key = Ambrose::Trim(body.substr(start, equals - start));
        if (!LogRedaction::IsSecretSetting(key))
            return std::string(line);
        std::size_t valueStart = equals + 1;
        while (valueStart < body.size() && (body[valueStart] == ' ' || body[valueStart] == '\t'))
            ++valueStart;
        std::size_t valueEnd = body.size();
        while (valueEnd > valueStart && (body[valueEnd - 1] == ' ' || body[valueEnd - 1] == '\t'))
            --valueEnd;
        std::string_view value = body.substr(valueStart, valueEnd - valueStart);
        std::string_view quote;
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        {
            quote = "\"";
            value = value.substr(1, value.size() - 2);
        }
        std::string const masked = LogRedaction::RedactSettingValue(key, value);
        if (masked == value)
            return std::string(line);
        if (redactedKeys && std::find(redactedKeys->begin(), redactedKeys->end(), key) == redactedKeys->end())
            redactedKeys->emplace_back(key);
        std::string out(body.substr(0, valueStart));
        out.append(quote);
        out.append(masked);
        out.append(quote);
        out.append(body.substr(valueEnd));
        out.append(ending);
        return out;
    }
}

bool LogRedaction::IsSecretSetting(std::string_view key)
{
    return SettingDeclarations::IsSecret(key);
}

std::string LogRedaction::RedactSettingValue(std::string_view key, std::string_view value)
{
    if (!IsSecretSetting(key))
        return std::string(value);
    return MaskSecretValue(key, value);
}

std::string LogRedaction::MaskSecretValue(std::string_view key, std::string_view value)
{
    if (Ambrose::Trim(value).empty())
        return std::string(value);
    std::string const lowered = Ambrose::ToLower(std::string(key));
    if (EndsWith(lowered, "databaseinfo"))
        return MaskConnectionString(value);
    if (IsKeyList(lowered))
        return MaskKeyList(value);
    return std::string(Mask);
}

std::string LogRedaction::DescribeSettingChange(std::string_view key, std::string_view value, std::string_view source)
{
    std::string text = "Setting ";
    text.append(key);
    text.append(" changed to ");
    text.append(RedactSettingValue(key, value));
    if (!source.empty())
    {
        text.append(" from ");
        text.append(source);
    }
    return text;
}

std::string LogRedaction::Redact(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    std::size_t position = 0;
    while (position < text.size())
    {
        std::size_t keyStart = position;
        while (keyStart < text.size() && !IsKeyCharacter(text[keyStart]))
            ++keyStart;
        std::size_t keyEnd = keyStart;
        while (keyEnd < text.size() && IsKeyCharacter(text[keyEnd]))
            ++keyEnd;
        out.append(text.substr(position, keyEnd - position));
        position = keyEnd;
        if (keyStart == keyEnd || !IsSecretSetting(text.substr(keyStart, keyEnd - keyStart)))
            continue;
        std::size_t cursor = keyEnd;
        while (cursor < text.size() && (text[cursor] == ' ' || text[cursor] == '\t'))
            ++cursor;
        if (cursor >= text.size() || (text[cursor] != '=' && text[cursor] != ':'))
            continue;
        ++cursor;
        while (cursor < text.size() && (text[cursor] == ' ' || text[cursor] == '\t'))
            ++cursor;
        bool const quoted = cursor < text.size() && text[cursor] == '"';
        if (quoted)
            ++cursor;
        std::size_t valueEnd = cursor;
        if (quoted)
        {
            while (valueEnd < text.size() && text[valueEnd] != '"')
                ++valueEnd;
        }
        else
        {
            bool const list = IsKeyList(Ambrose::ToLower(std::string(text.substr(keyStart, keyEnd - keyStart))));
            while (valueEnd < text.size() && text[valueEnd] != ' ' && (list || text[valueEnd] != ',') && text[valueEnd] != '\n' && text[valueEnd] != '\r')
                ++valueEnd;
        }
        out.append(text.substr(keyEnd, cursor - keyEnd));
        out.append(RedactSettingValue(text.substr(keyStart, keyEnd - keyStart), text.substr(cursor, valueEnd - cursor)));
        position = valueEnd;
    }
    return out;
}

std::string LogRedaction::RedactConf(std::string_view text, std::vector<std::string>* redactedKeys)
{
    constexpr std::string_view ByteOrderMark = "\xEF\xBB\xBF";
    std::string out;
    out.reserve(text.size());
    if (text.starts_with(ByteOrderMark))
    {
        out.append(ByteOrderMark);
        text.remove_prefix(ByteOrderMark.size());
    }
    std::size_t position = 0;
    while (position < text.size())
    {
        std::size_t const end = text.find('\n', position);
        std::size_t const stop = end == std::string_view::npos ? text.size() : end + 1;
        out.append(RedactConfLine(text.substr(position, stop - position), redactedKeys));
        position = stop;
    }
    return out;
}
