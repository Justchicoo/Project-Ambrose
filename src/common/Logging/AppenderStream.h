/*
 * Project Ambrose by Imjustchico
 * Stream appender type 3 that publishes raw structured records to the LogStreamHub.
 */

#ifndef AMBROSE_APPENDERSTREAM_H
#define AMBROSE_APPENDERSTREAM_H

#include "AppenderRegistry.h"

class AppenderStream : public Appender
{
public:
    static constexpr std::size_t MaxFields = 1;

    AppenderStream(AppenderDefinition const& definition, LogStreamHub& hub, std::size_t backlog);

    static AppenderTypeInfo GetTypeInfo();
    static std::optional<std::size_t> ParseBacklog(AppenderDefinition const& definition, LogConfigResult& result);

    void Activate() override;

protected:
    void WriteMessage(LogMessage const& message) override;

private:
    LogStreamHub& _hub;
    std::size_t _backlog;
};

#endif
