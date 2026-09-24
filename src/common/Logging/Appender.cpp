/*
 * Project Ambrose by Imjustchico
 * Shared appender filtering by level, excluded category, and nesting, with writes that never throw.
 */

#include "Appender.h"

#include <fmt/format.h>

#include <utility>

std::string AppenderDefinition::ReuseKey() const
{
    std::string key = fmt::format("{}\x1F{}", static_cast<uint32>(Type), static_cast<uint32>(Flags));
    for (std::string const& field : Fields)
    {
        key.push_back('\x1F');
        key.append(field);
    }
    return key;
}

ConfigIssue AppenderDefinition::MakeIssue(std::string const& message) const
{
    return ConfigIssue{ File, Line, fmt::format("Appender.{}: {}", Name, message) };
}

Appender::Appender(std::string name, AppenderType type, LogLevel level, AppenderFlags flags)
    : _name(std::move(name)), _type(type), _level(level), _flags(flags)
{
}

Appender::~Appender() = default;

std::string const& Appender::GetName() const noexcept
{
    return _name;
}

AppenderType Appender::GetType() const noexcept
{
    return _type;
}

LogLevel Appender::GetLevel() const noexcept
{
    return _level.load(std::memory_order_relaxed);
}

void Appender::SetLevel(LogLevel level) noexcept
{
    _level.store(level, std::memory_order_relaxed);
}

AppenderFlags Appender::GetFlags() const noexcept
{
    return _flags;
}

bool Appender::HasFlag(AppenderFlags flag) const noexcept
{
    return HasAppenderFlag(_flags, flag);
}

uint64 Appender::GetFailureCount() const noexcept
{
    return _failures.load(std::memory_order_relaxed);
}

void Appender::SetExcludedCategories(std::vector<std::string> prefixes)
{
    _excluded = std::move(prefixes);
}

std::vector<std::string> const& Appender::GetExcludedCategories() const noexcept
{
    return _excluded;
}

bool Appender::Accepts(LogMessage const& message) const noexcept
{
    if (!IsLevelEnabled(GetLevel(), message.Level))
        return false;
    for (std::string const& prefix : _excluded)
        if (Ambrose::Logging::IsCategoryWithin(message.Category, prefix))
            return false;
    if (message.Nested && !AcceptsNestedMessages())
        return false;
    return AcceptsMessage(message);
}

void Appender::Write(LogMessage const& message) noexcept
{
    try
    {
        WriteMessage(message);
    }
    catch (...)
    {
        _failures.fetch_add(1, std::memory_order_relaxed);
    }
}

void Appender::Flush()
{
}

bool Appender::AcceptsNestedMessages() const noexcept
{
    return true;
}

Appender const* Appender::GetSink() const noexcept
{
    return this;
}

void Appender::Activate()
{
}

bool Appender::AcceptsMessage(LogMessage const&) const noexcept
{
    return true;
}
