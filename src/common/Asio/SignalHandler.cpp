/*
 * Project Ambrose by Imjustchico
 * Registers SIGINT, SIGTERM, and SIGBREAK where available, rearms the asio signal wait after each delivery, and releases the signals only on destruction.
 */

#include "SignalHandler.h"

#include <csignal>
#include <mutex>
#include <system_error>

struct Ambrose::Asio::SignalHandler::Shared
{
    Shared(IoContext& context, Callback function) : Signals(context.GetImpl()), Function(std::move(function))
    {
    }

    asio::signal_set Signals;
    Callback Function;
    std::recursive_mutex CallbackMutex;
    std::mutex Mutex;
    bool Cancelled = false;
};

Ambrose::Asio::SignalHandler::SignalHandler(IoContext& context, Callback callback) : SignalHandler(context, ShutdownSignals(), std::move(callback))
{
}

Ambrose::Asio::SignalHandler::SignalHandler(IoContext& context, std::initializer_list<int> signals, Callback callback)
    : _shared(std::make_shared<Shared>(context, std::move(callback)))
{
    for (int const signal : signals)
    {
        std::error_code error;
        _shared->Signals.add(signal, error);
    }
    std::lock_guard lock(_shared->Mutex);
    Arm(_shared);
}

Ambrose::Asio::SignalHandler::~SignalHandler()
{
    Cancel();
    std::lock_guard callbackLock(_shared->CallbackMutex);
    std::lock_guard lock(_shared->Mutex);
    std::error_code error;
    _shared->Signals.clear(error);
}

void Ambrose::Asio::SignalHandler::Cancel()
{
    std::lock_guard callbackLock(_shared->CallbackMutex);
    std::lock_guard lock(_shared->Mutex);
    if (_shared->Cancelled)
        return;
    _shared->Cancelled = true;
    std::error_code error;
    _shared->Signals.cancel(error);
}

std::initializer_list<int> Ambrose::Asio::SignalHandler::ShutdownSignals()
{
#ifdef SIGBREAK
    static constexpr std::initializer_list<int> Signals{ SIGINT, SIGTERM, SIGBREAK };
#else
    static constexpr std::initializer_list<int> Signals{ SIGINT, SIGTERM };
#endif
    return Signals;
}

void Ambrose::Asio::SignalHandler::Arm(std::shared_ptr<Shared> const& shared)
{
    std::weak_ptr<Shared> const weak = shared;
    shared->Signals.async_wait([weak](std::error_code const& error, int signal)
    {
        if (error)
            return;
        std::shared_ptr<Shared> const current = weak.lock();
        if (!current)
            return;
        std::lock_guard callbackLock(current->CallbackMutex);
        Callback function;
        {
            std::lock_guard lock(current->Mutex);
            if (current->Cancelled)
                return;
            function = current->Function;
        }
        if (function)
            function(signal);
        std::lock_guard lock(current->Mutex);
        if (!current->Cancelled)
            Arm(current);
    });
}
