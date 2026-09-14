/*
 * Project Ambrose by Imjustchico
 * Runs a callable when a scope ends, so tests stop queues and io_contexts before waiting on futures even after a failed assert.
 */

#ifndef AMBROSE_SCOPEEXIT_H
#define AMBROSE_SCOPEEXIT_H

#include <utility>

template<typename Function>
class ScopeExit
{
public:
    explicit ScopeExit(Function function) : _function(std::move(function))
    {
    }

    ~ScopeExit()
    {
        _function();
    }

    ScopeExit(ScopeExit const&) = delete;
    ScopeExit& operator=(ScopeExit const&) = delete;

private:
    Function _function;
};

#endif
