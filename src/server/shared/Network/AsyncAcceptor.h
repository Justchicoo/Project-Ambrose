/*
 * Project Ambrose by Imjustchico
 * A listening TCP socket that binds an address and port and accepts connections straight onto the io_context a selector picks for each one.
 */

#ifndef AMBROSE_ASYNCACCEPTOR_H
#define AMBROSE_ASYNCACCEPTOR_H

#include "Types.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <system_error>

class AsyncAcceptor : public std::enable_shared_from_this<AsyncAcceptor>
{
public:
    struct Target
    {
        asio::io_context* Context = nullptr;
        void* Tag = nullptr;
    };

    using Selector = std::function<Target()>;
    using Handler = std::function<void(std::error_code const&, asio::ip::tcp::socket&&, void*)>;

    static constexpr std::chrono::milliseconds ErrorBackoff{ 100 };

    explicit AsyncAcceptor(asio::io_context& context);
    ~AsyncAcceptor();
    AsyncAcceptor(AsyncAcceptor const&) = delete;
    AsyncAcceptor& operator=(AsyncAcceptor const&) = delete;

    bool Bind(std::string const& bindIp, uint16 port, std::string& error);
    void Start(Selector selector, Handler handler);
    void Cancel();
    void Close();
    void CloseAndWait();

    bool IsOpen() const noexcept { return !_closed.load(std::memory_order_relaxed); }
    uint16 GetPort() const noexcept { return _port; }
    std::string GetEndpointText() const;

private:
    void AcceptNext();
    void CloseNow();

    asio::io_context& _context;
    asio::ip::tcp::acceptor _acceptor;
    asio::steady_timer _retryTimer;
    Selector _selector;
    Handler _handler;
    std::atomic<bool> _closed{ false };
    std::string _bindIp;
    uint16 _port = 0;
};

#endif
