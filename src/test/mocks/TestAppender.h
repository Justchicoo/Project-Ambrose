/*
 * Project Ambrose by Imjustchico
 * Configurable test appender type that records into a shared TestAppenderStore.
 */

#ifndef AMBROSE_TESTAPPENDER_H
#define AMBROSE_TESTAPPENDER_H

#include "AppenderRegistry.h"
#include "TestAppenderStore.h"

#include <memory>

class TestAppender : public Appender
{
public:
    static constexpr AppenderType DefaultType = static_cast<AppenderType>(200);

    TestAppender(AppenderDefinition const& definition, std::shared_ptr<TestAppenderStore> store, bool acceptsNested);

    static AppenderTypeInfo GetTypeInfo(std::shared_ptr<TestAppenderStore> store, AppenderType type = DefaultType, std::vector<std::string> excludedCategories = {});

    bool AcceptsNestedMessages() const noexcept override;
    void Flush() override;

protected:
    void WriteMessage(LogMessage const& message) override;

private:
    std::shared_ptr<TestAppenderStore> _store;
    bool _acceptsNested;
};

#endif
