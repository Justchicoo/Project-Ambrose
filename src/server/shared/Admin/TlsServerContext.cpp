/*
 * Project Ambrose by Imjustchico
 * Parses the checked PEM text into an OpenSSL certificate chain and key, publishes that pair as one value every handshake reads through a certificate callback, and replaces it in a single store, so a swap is either the old pair or the new one and never half of each; parsing happens before the store, so a pair that will not load reports the reason and changes nothing.
 */

#include "TlsServerContext.h"
#include "TlsCertificate.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <fmt/format.h>

#include <atomic>
#include <vector>

namespace
{
    struct StackDeleter
    {
        void operator()(STACK_OF(X509) * chain) const { sk_X509_pop_free(chain, X509_free); }
    };

    struct LeafDeleter
    {
        void operator()(X509* certificate) const { X509_free(certificate); }
    };

    struct KeyDeleter
    {
        void operator()(EVP_PKEY* key) const { EVP_PKEY_free(key); }
    };

    struct BioDeleter
    {
        void operator()(BIO* bio) const { BIO_free(bio); }
    };

    struct Pair
    {
        std::unique_ptr<X509, LeafDeleter> Leaf;
        std::unique_ptr<STACK_OF(X509), StackDeleter> Chain;
        std::unique_ptr<EVP_PKEY, KeyDeleter> Key;
        std::string Fingerprint;
    };

    std::string TakeOpenSslError()
    {
        std::string text;
        while (unsigned long const code = ERR_get_error())
        {
            char buffer[256] = {};
            ERR_error_string_n(code, buffer, sizeof(buffer));
            if (!text.empty())
                text += "; ";
            text += buffer;
        }
        return text.empty() ? std::string("OpenSSL gave no reason") : text;
    }

    std::shared_ptr<Pair const> Parse(TlsCertificate const& certificate, std::string& error)
    {
        ERR_clear_error();
        auto pair = std::make_shared<Pair>();
        std::unique_ptr<BIO, BioDeleter> certificateBio(BIO_new_mem_buf(certificate.GetCertificatePem().data(), static_cast<int>(certificate.GetCertificatePem().size())));
        std::unique_ptr<BIO, BioDeleter> keyBio(BIO_new_mem_buf(certificate.GetKeyPem().data(), static_cast<int>(certificate.GetKeyPem().size())));
        if (!certificateBio || !keyBio)
        {
            error = "the certificate could not be held in memory";
            return nullptr;
        }

        pair->Leaf.reset(PEM_read_bio_X509(certificateBio.get(), nullptr, nullptr, nullptr));
        if (!pair->Leaf)
        {
            error = fmt::format("the certificate could not be parsed: {}", TakeOpenSslError());
            return nullptr;
        }
        pair->Chain.reset(sk_X509_new_null());
        if (!pair->Chain)
        {
            error = "the certificate chain could not be held in memory";
            return nullptr;
        }
        while (X509* issuer = PEM_read_bio_X509(certificateBio.get(), nullptr, nullptr, nullptr))
        {
            if (sk_X509_push(pair->Chain.get(), issuer) == 0)
            {
                X509_free(issuer);
                error = "the certificate chain could not be held in memory";
                return nullptr;
            }
        }
        ERR_clear_error();

        pair->Key.reset(PEM_read_bio_PrivateKey(keyBio.get(), nullptr, nullptr, nullptr));
        if (!pair->Key)
        {
            error = fmt::format("the key could not be parsed: {}", TakeOpenSslError());
            return nullptr;
        }
        if (X509_check_private_key(pair->Leaf.get(), pair->Key.get()) != 1)
        {
            error = fmt::format("the key does not belong to the certificate: {}", TakeOpenSslError());
            return nullptr;
        }
        ERR_clear_error();
        pair->Fingerprint = certificate.GetInfo().Fingerprint;
        return pair;
    }
}

struct TlsServerContext::Impl
{
    SSL_CTX* Context = nullptr;
    std::atomic<std::shared_ptr<Pair const>> Current;
};

namespace
{
    int SelectCertificate(SSL* connection, void* argument)
    {
        auto* const impl = static_cast<TlsServerContext::Impl*>(argument);
        std::shared_ptr<Pair const> const pair = impl->Current.load();
        if (!pair)
            return 0;
        if (SSL_use_certificate(connection, pair->Leaf.get()) != 1 || SSL_use_PrivateKey(connection, pair->Key.get()) != 1)
            return 0;
        return SSL_set1_chain(connection, pair->Chain.get()) == 1 ? 1 : 0;
    }
}

TlsServerContext::TlsServerContext() : _impl(std::make_unique<Impl>())
{
}

TlsServerContext::~TlsServerContext()
{
    Detach();
}

bool TlsServerContext::Attach(void* sslContext, TlsCertificate const& certificate, std::string& error)
{
    error.clear();
    std::shared_ptr<Pair const> pair = Parse(certificate, error);
    if (!pair)
        return false;
    Detach();
    _impl->Context = static_cast<SSL_CTX*>(sslContext);
    SSL_CTX_up_ref(_impl->Context);
    _impl->Current.store(std::move(pair));
    SSL_CTX_set_cert_cb(_impl->Context, &SelectCertificate, _impl.get());
    return true;
}

bool TlsServerContext::Swap(TlsCertificate const& certificate, std::string& error)
{
    error.clear();
    if (_impl->Context == nullptr)
    {
        error = "no listener is serving TLS";
        return false;
    }
    std::shared_ptr<Pair const> pair = Parse(certificate, error);
    if (!pair)
        return false;
    _impl->Current.store(std::move(pair));
    return true;
}

void TlsServerContext::Detach()
{
    if (_impl->Context != nullptr)
    {
        SSL_CTX_set_cert_cb(_impl->Context, nullptr, nullptr);
        SSL_CTX_free(_impl->Context);
        _impl->Context = nullptr;
    }
    _impl->Current.store(nullptr);
}

bool TlsServerContext::IsAttached() const
{
    return _impl->Context != nullptr;
}

std::string TlsServerContext::GetFingerprint() const
{
    std::shared_ptr<Pair const> const pair = _impl->Current.load();
    return pair ? pair->Fingerprint : std::string();
}
