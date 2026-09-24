/*
 * Project Ambrose by Imjustchico
 * Calls a queued operation's completion handler once, from whichever thread settles its result or destroys it unrun, and logs a handler that throws.
 */

#include "SQLOperation.h"
#include "Log.h"

#include <exception>

SQLOperation::~SQLOperation()
{
    NotifyCompleted();
}

void SQLOperation::NotifyCompleted() noexcept
{
    if (_notified || !_completion)
        return;
    _notified = true;
    try
    {
        _completion();
    }
    catch (std::exception const& exception)
    {
        LOG_ERROR("sql.sql", "A database completion handler threw: {}", exception.what());
    }
    catch (...)
    {
        LOG_ERROR("sql.sql", "A database completion handler threw an unknown exception");
    }
}
