/*
 * Project Ambrose by Imjustchico
 * The live log's hub: the stream layer's backlog ring and subscriber registry over log records, a class of its own so the logger can name it without seeing the layer.
 */

#ifndef AMBROSE_LOGSTREAMHUB_H
#define AMBROSE_LOGSTREAMHUB_H

#include "LogSubscription.h"
#include "StreamHub.h"

extern template class StreamHub<LogMessage, LogStreamFilter>;

class LogStreamHub : public StreamHub<LogMessage, LogStreamFilter>
{
public:
    LogStreamHub() = default;
};

#endif
