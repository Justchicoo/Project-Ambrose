/*
 * Project Ambrose by Imjustchico
 * Logging calls that must compile in the control case and must fail to compile in every other case.
 */

#include "Log.h"

#include <string>
#include <string_view>

#if AMBROSE_LOG_FORMAT_CASE == 0
namespace
{
    constexpr std::string_view CheckCategory = "server.check";
}

#line 1000
void LogFormatControl() { std::string const format = "{}"; std::string const category = "server.check"; std::string_view const view = "text"; Log& local = sLog; LOG_INFO("server.check", "{} {}", 1, "two"); LOG_INFO(CheckCategory, "{}", view); LOG_INFO("server.check", fmt::runtime(format), 1); LOG_DYNAMIC(LogLevel::Info, category, "{}", 1); LOG_ERROR("server.check", "{:d}", 5); LOG_FATAL("server.check", "plain"); AMBROSE_LOG(local, LogLevel::Warn, "server.check", "{}", 2); }
#elif AMBROSE_LOG_FORMAT_CASE == 1
#line 1001
void LogFormatTooFewArguments() { LOG_INFO("server.check", "{} {}", 1); }
#elif AMBROSE_LOG_FORMAT_CASE == 2
#line 1002
void LogFormatWrongSpecifierType() { LOG_INFO("server.check", "{:d}", std::string_view("text")); }
#elif AMBROSE_LOG_FORMAT_CASE == 3
#line 1003
void LogFormatMalformed() { LOG_INFO("server.check", "{", 1); }
#elif AMBROSE_LOG_FORMAT_CASE == 4
#line 1004
void LogFormatRuntimeString() { std::string const format = "{}"; LOG_INFO("server.check", format, 1); }
#elif AMBROSE_LOG_FORMAT_CASE == 5
#line 1005
void LogFormatRuntimeCategory() { std::string const category = "server"; LOG_INFO(category, "{}", 1); }
#endif
