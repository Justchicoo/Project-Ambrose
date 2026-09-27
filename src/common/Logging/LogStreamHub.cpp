/*
 * Project Ambrose by Imjustchico
 * Holds the one compiled copy of the stream layer's hub over live log records.
 */

#include "LogStreamHub.h"

template class StreamHub<LogMessage, LogStreamFilter>;
