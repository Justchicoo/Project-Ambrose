/*
 * Project Ambrose by Imjustchico
 * Creates capture appenders from Type,Level,Flags[,nonested] and records every accepted write.
 */

#include "TestAppender.h"
#include "LogConfig.h"
#include "StringUtil.h"

#include <stdexcept>

TestAppender::TestAppender(AppenderDefinition const& definition, std::shared_ptr<TestAppenderStore> store, bool acceptsNested)
    : Appender(definition.Name, definition.Type, definition.Level, definition.Flags), _store(std::move(store)), _acceptsNested(acceptsNested)
{
}

AppenderTypeInfo TestAppender::GetTypeInfo(std::shared_ptr<TestAppenderStore> store, AppenderType type, std::vector<std::string> excludedCategories)
{
    AppenderTypeInfo info;
    info.Type = type;
    info.Name = "Test";
    info.ExcludedCategories = std::move(excludedCategories);
    info.Create = [store](AppenderDefinition const& definition, AppenderCreateContext const&, LogConfigResult& result) -> std::shared_ptr<Appender>
    {
        bool acceptsNested = true;
        if (!definition.Fields.empty())
        {
            if (definition.Fields.size() > 1 || !Ambrose::EqualsIgnoreCase(Ambrose::Trim(definition.Fields[0]), "nonested"))
            {
                result.Errors.push_back(definition.MakeIssue("test appenders take Type,Level,Flags[,nonested]"));
                return nullptr;
            }
            acceptsNested = false;
        }
        return std::make_shared<TestAppender>(definition, store, acceptsNested);
    };
    return info;
}

bool TestAppender::AcceptsNestedMessages() const noexcept
{
    return _acceptsNested;
}

void TestAppender::Flush()
{
    _store->NoteFlush(GetName());
}

void TestAppender::WriteMessage(LogMessage const& message)
{
    _store->WaitAtGate();
    if (_store->ShouldThrow())
        throw std::runtime_error("test appender failure");
    _store->Record(GetName(), message);
    _store->RunOnWrite(GetName(), message);
}
