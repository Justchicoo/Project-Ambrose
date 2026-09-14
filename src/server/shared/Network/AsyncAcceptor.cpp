/*
 * Project Ambrose by Imjustchico
 * Binds and listens with clear errors, keeps SO_REUSEADDR off on Windows, loops async accepts with a backoff after errors, and closes synchronously when asked.
 */

#include "AsyncAcceptor.h"
#include "IpAddress.h"
#include "Log.h"

#include <asio/post.hpp>

#include <fmt/format.h>

#include <future>

AsyncAcceptor::AsyncAcceptor(asio::io_context& context) : _context(context), _acceptor(context), _retryTimer(context)
{
}

AsyncAcceptor::~AsyncAcceptor()
{
    std::error_code ignored;
    _acceptor.close(ignored);
}

bool AsyncAcceptor::Bind(std::string const& bindIp, uint16 port, std::string& error)
{
    std::optional<asio::ip::address> const address = Ambrose::Asio::MakeAddress(bindIp);
    if (!address)
    {
        error = fmt::format("BindIP '{}' is not an IP address", bindIp);
        return false;
    }
    asio::ip::tcp::endpoint const endpoint(*address, port);
    std::error_code code;
    _acceptor.open(endpoint.protocol(), code);
    if (!code)
    {
#ifndef _WIN32
        _acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true), code);
#endif
    }
    if (!code)
        _acceptor.bind(endpoint, code);
    if (!code)
        _acceptor.listen(asio::socket_base::max_listen_connections, code);
    if (code)
    {
        error = fmt::format("cannot listen on {}:{}: {}", bindIp, port, code.message());
        std::error_code ignored;
        _acceptor.close(ignored);
        return false;
    }
    _bindIp = bindIp;
    _port = _acceptor.local_endpoint(code).port();
    return true;
}

void AsyncAcceptor::Start(Selector selector, Handler handler)
{
    _selector = std::move(selector);
    _handler = std::move(handler);
    asio::post(_context, [self = shared_from_this()] { self->AcceptNext(); });
}

void AsyncAcceptor::Cancel()
{
    asio::post(_context, [self = shared_from_this()]
    {
        std::error_code ignored;
        self->_acceptor.cancel(ignored);
    });
}

void AsyncAcceptor::Close()
{
    if (_closed.exchange(true, std::memory_order_relaxed))
        return;
    asio::post(_context, [self = shared_from_this()] { self->CloseNow(); });
}

void AsyncAcceptor::CloseAndWait()
{
    if (_closed.exchange(true, std::memory_order_relaxed))
        return;
    if (_context.get_executor().running_in_this_thread())
    {
        CloseNow();
        return;
    }
    auto done = std::make_shared<std::promise<void>>();
    std::future<void> finished = done->get_future();
    asio::post(_context, [self = shared_from_this(), done]
    {
        self->CloseNow();
        done->set_value();
    });
    finished.wait();
}

std::string AsyncAcceptor::GetEndpointText() const
{
    return fmt::format("{}:{}", _bindIp, _port);
}

void AsyncAcceptor::CloseNow()
{
    std::error_code ignored;
    _retryTimer.cancel();
    _acceptor.close(ignored);
}

void AsyncAcceptor::AcceptNext()
{
    if (!IsOpen() || !_acceptor.is_open())
        return;
    Target const target = _selector();
    if (!target.Context)
        return;
    _acceptor.async_accept(*target.Context, [self = shared_from_this(), tag = target.Tag](std::error_code const& error, asio::ip::tcp::socket socket)
    {
        self->_handler(self->IsOpen() ? error : std::error_code(asio::error::operation_aborted), std::move(socket), tag);
        if (!self->IsOpen())
            return;
        if (!error || error == asio::error::operation_aborted)
        {
            self->AcceptNext();
            return;
        }
        LOG_WARN("network", "Accept on {} failed: {}; retrying in {} ms", self->GetEndpointText(), error.message(), ErrorBackoff.count());
        self->_retryTimer.expires_after(ErrorBackoff);
        self->_retryTimer.async_wait([self](std::error_code const& timerError)
        {
            if (!timerError)
                self->AcceptNext();
        });
    });
}
