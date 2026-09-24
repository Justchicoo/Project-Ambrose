/*
 * Project Ambrose by Imjustchico
 * Appender base class with level, flags, excluded categories, nested-message policy and exception-safe writes.
 */

#ifndef AMBROSE_APPENDER_H
#define AMBROSE_APPENDER_H

#include "ConfigMgr.h"
#include "LogCommon.h"
#include "LogMessage.h"

#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

struct AppenderDefinition
{
    std::string Name;
    AppenderType Type = AppenderType::None;
    LogLevel Level = LogLevel::Disabled;
    AppenderFlags Flags = AppenderFlags::None;
    std::vector<std::string> Fields;
    std::filesystem::path File;
    std::size_t Line = 0;

    std::string ReuseKey() const;
    ConfigIssue MakeIssue(std::string const& message) const;
};

class Appender
{
public:
    Appender(std::string name, AppenderType type, LogLevel level, AppenderFlags flags);
    virtual ~Appender();

    Appender(Appender const&) = delete;
    Appender& operator=(Appender const&) = delete;

    std::string const& GetName() const noexcept;
    AppenderType GetType() const noexcept;
    LogLevel GetLevel() const noexcept;
    void SetLevel(LogLevel level) noexcept;
    AppenderFlags GetFlags() const noexcept;
    bool HasFlag(AppenderFlags flag) const noexcept;
    uint64 GetFailureCount() const noexcept;
    void SetExcludedCategories(std::vector<std::string> prefixes);
    std::vector<std::string> const& GetExcludedCategories() const noexcept;

    bool Accepts(LogMessage const& message) const noexcept;
    void Write(LogMessage const& message) noexcept;
    virtual void Flush();
    virtual bool AcceptsNestedMessages() const noexcept;
    virtual Appender const* GetSink() const noexcept;
    virtual void Activate();

protected:
    virtual bool AcceptsMessage(LogMessage const& message) const noexcept;
    virtual void WriteMessage(LogMessage const& message) = 0;

private:
    std::string _name;
    AppenderType _type;
    std::atomic<LogLevel> _level;
    AppenderFlags _flags;
    std::vector<std::string> _excluded;
    std::atomic<uint64> _failures{ 0 };
};

#endif
