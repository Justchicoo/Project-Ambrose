/*
 * Project Ambrose by Imjustchico
 * The certificate a running listener serves, held apart from the listener so it can be replaced while connections are open: it attaches to the listener's OpenSSL context once and answers every new handshake from the pair it holds, and a reload swaps that pair in one step, so a new certificate serves the next connection and a pair that will not load leaves the old one serving.
 */

#ifndef AMBROSE_TLSSERVERCONTEXT_H
#define AMBROSE_TLSSERVERCONTEXT_H

#include <memory>
#include <string>

class TlsCertificate;

class TlsServerContext
{
public:
    TlsServerContext();
    ~TlsServerContext();

    TlsServerContext(TlsServerContext const&) = delete;
    TlsServerContext& operator=(TlsServerContext const&) = delete;

    bool Attach(void* sslContext, TlsCertificate const& certificate, std::string& error);
    bool Swap(TlsCertificate const& certificate, std::string& error);
    void Detach();

    bool IsAttached() const;
    std::string GetFingerprint() const;

    struct Impl;

private:
    std::unique_ptr<Impl> _impl;
};

#endif
