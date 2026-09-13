/*
 * Project Ambrose by Imjustchico
 * Builds a Log on a fake console, registers the capture appender type, and applies inline config text.
 */

#include "LogTestHarness.h"

#include <gtest/gtest.h>

LogTestHarness::LogTestHarness(bool terminal, bool virtualTerminal) : _store(std::make_shared<TestAppenderStore>())
{
    auto device = std::make_unique<FakeConsoleDevice>(terminal, virtualTerminal);
    _device = device.get();
    _writer = std::make_unique<ConsoleWriter>(std::move(device));
    _log = std::make_unique<Log>(*_writer);
    LogConfigResult const registered = _log->RegisterAppenderType(TestAppender::GetTypeInfo(_store));
    EXPECT_TRUE(registered.Succeeded()) << LogTestConfig::Describe(registered);
}

LogTestHarness::~LogTestHarness()
{
    _store->OpenGate();
    _log.reset();
}

Log& LogTestHarness::GetLog()
{
    return *_log;
}

TestAppenderStore& LogTestHarness::Store()
{
    return *_store;
}

std::shared_ptr<TestAppenderStore> const& LogTestHarness::SharedStore() const
{
    return _store;
}

FakeConsoleDevice& LogTestHarness::Device()
{
    return *_device;
}

ConsoleWriter& LogTestHarness::Writer()
{
    return *_writer;
}

LogConfigResult LogTestHarness::Apply(std::string_view body)
{
    LogConfigResult parsed;
    LogSettings settings = LogTestConfig::Settings(body, parsed);
    if (!parsed.Succeeded())
        return parsed;
    LogConfigResult applied = _log->Apply(std::move(settings));
    applied.Warnings.insert(applied.Warnings.begin(), parsed.Warnings.begin(), parsed.Warnings.end());
    return applied;
}

void LogTestHarness::ApplyOrFail(std::string_view body)
{
    LogConfigResult const result = Apply(body);
    ASSERT_TRUE(result.Succeeded()) << LogTestConfig::Describe(result);
}
