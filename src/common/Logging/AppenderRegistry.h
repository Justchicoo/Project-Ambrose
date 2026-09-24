/*
 * Project Ambrose by Imjustchico
 * Appender definitions, the creation context, and the type id to factory registry used by outer layers.
 */

#ifndef AMBROSE_APPENDERREGISTRY_H
#define AMBROSE_APPENDERREGISTRY_H

#include "Appender.h"
#include "LogMessage.h"

#include <chrono>
#include <concepts>
#include <functional>
#include <map>
#include <memory>
#include <vector>

class ConsoleWriter;
class LogFileRegistry;
class LogStreamHub;
struct LogConfigResult;

struct AppenderCreateContext
{
    std::filesystem::path LogsDir;
    std::chrono::system_clock::time_point LoadTime;
    bool Utc = false;
    ConsoleWriter& Console;
    LogFileRegistry& Files;
    LogStreamHub& Streams;
    LogLayout ConsoleLayout;
    bool RepeatCategory = true;
};

using AppenderFactory = std::function<std::shared_ptr<Appender>(AppenderDefinition const&, AppenderCreateContext const&, LogConfigResult&)>;

struct AppenderTypeInfo
{
    AppenderType Type = AppenderType::None;
    std::string Name;
    AppenderFactory Create;
    std::vector<std::string> ExcludedCategories;
};

template<typename T>
concept AppenderImplementation = std::derived_from<T, Appender> && requires
{
    { T::GetTypeInfo() } -> std::convertible_to<AppenderTypeInfo>;
};

class AppenderRegistry
{
public:
    bool Register(AppenderTypeInfo info);
    bool Unregister(AppenderType type);
    AppenderTypeInfo const* Find(AppenderType type) const;
    std::vector<AppenderType> GetTypes() const;

private:
    std::map<AppenderType, AppenderTypeInfo> _types;
};

#endif
