/*
 * Project Ambrose by Imjustchico
 * Waits for shutdown signals on an io_context and calls back for each one until cancelled.
 */

#ifndef AMBROSE_SIGNALHANDLER_H
#define AMBROSE_SIGNALHANDLER_H

#include "IoContext.h"

#include <asio/signal_set.hpp>

#include <functional>
#include <initializer_list>
#include <memory>

namespace Ambrose::Asio
{
    class SignalHandler
    {
    public:
        using Callback = std::function<void(int)>;

        SignalHandler(IoContext& context, Callback callback);
        SignalHandler(IoContext& context, std::initializer_list<int> signals, Callback callback);
        ~SignalHandler();

        SignalHandler(SignalHandler const&) = delete;
        SignalHandler& operator=(SignalHandler const&) = delete;

        void Cancel();

        static std::initializer_list<int> ShutdownSignals();

    private:
        struct Shared;

        static void Arm(std::shared_ptr<Shared> const& shared);

        std::shared_ptr<Shared> _shared;
    };
}

#endif
