/*
 * Project Ambrose by Imjustchico
 * A private Log instance with a fake console and a capture store, for logging tests that avoid the sLog singleton.
 */

#ifndef AMBROSE_LOGTESTHARNESS_H
#define AMBROSE_LOGTESTHARNESS_H

#include "ConsoleWriter.h"
#include "FakeConsoleDevice.h"
#include "Log.h"
#include "LogTestConfig.h"
#include "TestAppender.h"

#include <memory>
#include <string_view>

class LogTestHarness
{
public:
    explicit LogTestHarness(bool terminal = false, bool virtualTerminal = false);
    ~LogTestHarness();

    LogTestHarness(LogTestHarness const&) = delete;
    LogTestHarness& operator=(LogTestHarness const&) = delete;

    Log& GetLog();
    TestAppenderStore& Store();
    std::shared_ptr<TestAppenderStore> const& SharedStore() const;
    FakeConsoleDevice& Device();
    ConsoleWriter& Writer();
    LogConfigResult Apply(std::string_view body);
    void ApplyOrFail(std::string_view body);

private:
    std::shared_ptr<TestAppenderStore> _store;
    FakeConsoleDevice* _device;
    std::unique_ptr<ConsoleWriter> _writer;
    std::unique_ptr<Log> _log;
};

#endif
