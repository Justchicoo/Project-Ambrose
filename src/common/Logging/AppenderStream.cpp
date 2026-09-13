/*
 * Project Ambrose by Imjustchico
 * Validates the stream backlog field and forwards each record to the live stream hub.
 */

#include "AppenderStream.h"
#include "LogConfig.h"
#include "LogStreamHub.h"
#include "StringUtil.h"

#include <fmt/format.h>

AppenderStream::AppenderStream(AppenderDefinition const& definition, LogStreamHub& hub, std::size_t backlog)
    : Appender(definition.Name, definition.Type, definition.Level, definition.Flags), _hub(hub), _backlog(backlog)
{
}

AppenderTypeInfo AppenderStream::GetTypeInfo()
{
    AppenderTypeInfo info;
    info.Type = AppenderType::Stream;
    info.Name = "Stream";
    info.Create = [](AppenderDefinition const& definition, AppenderCreateContext const& context, LogConfigResult& result) -> std::shared_ptr<Appender>
    {
        std::optional<std::size_t> const backlog = ParseBacklog(definition, result);
        if (!backlog)
            return nullptr;
        return std::make_shared<AppenderStream>(definition, context.Streams, *backlog);
    };
    return info;
}

std::optional<std::size_t> AppenderStream::ParseBacklog(AppenderDefinition const& definition, LogConfigResult& result)
{
    if (definition.Fields.size() > MaxFields)
    {
        result.Errors.push_back(definition.MakeIssue("too many fields; Stream takes Type,Level,Flags[,Backlog]"));
        return std::nullopt;
    }
    if (definition.Fields.empty() || Ambrose::Trim(definition.Fields[0]).empty())
        return LogStreamHub::DefaultBacklog;
    std::string_view const text = Ambrose::Trim(definition.Fields[0]);
    std::optional<uint32> const backlog = Ambrose::StringTo<uint32>(text);
    if (!backlog || *backlog > LogStreamHub::MaxBacklog)
    {
        result.Errors.push_back(definition.MakeIssue(fmt::format("Backlog '{}' is out of range; use 0-{}", text, LogStreamHub::MaxBacklog)));
        return std::nullopt;
    }
    return *backlog;
}

void AppenderStream::Activate()
{
    _hub.SetBacklogCapacity(_backlog);
}

void AppenderStream::WriteMessage(LogMessage const& message)
{
    _hub.Publish(message);
}
