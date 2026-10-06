/*
 * Project Ambrose by Imjustchico
 * Runs one GET through libcurl: for a pinned panel it hands OpenSSL a verify callback that accepts the chain only when the leaf certificate's SHA-256, written the colon-separated way the supervisor prints it, equals the pin, and it turns host name checking off since the pin is the identity; the body is capped, and a failed handshake on a pin is reported as a refused pin with the fingerprint that was served.
 */

#include "CurlTransport.h"

#include <curl/curl.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <fmt/format.h>

#include <mutex>

namespace
{
    struct PinCheck
    {
        std::string Expected;
        std::string Served;
        bool Refused = false;
    };

    std::string Fingerprint(X509* certificate)
    {
        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int length = 0;
        if (certificate == nullptr || X509_digest(certificate, EVP_sha256(), digest, &length) != 1)
            return std::string();
        std::string text;
        for (unsigned int index = 0; index < length; ++index)
            text += fmt::format("{}{:02X}", index == 0 ? "" : ":", digest[index]);
        return text;
    }

    int VerifyPin(int, X509_STORE_CTX* store)
    {
        if (X509_STORE_CTX_get_error_depth(store) != 0)
            return 1;
        auto* const ssl = static_cast<SSL*>(X509_STORE_CTX_get_ex_data(store, SSL_get_ex_data_X509_STORE_CTX_idx()));
        auto* const check = ssl == nullptr ? nullptr : static_cast<PinCheck*>(SSL_CTX_get_app_data(SSL_get_SSL_CTX(ssl)));
        if (check == nullptr)
            return 0;
        check->Served = Fingerprint(X509_STORE_CTX_get_current_cert(store));
        check->Refused = check->Served.empty() || check->Served != check->Expected;
        if (check->Refused)
            return 0;
        X509_STORE_CTX_set_error(store, X509_V_OK);
        return 1;
    }

    CURLcode PrepareContext(CURL*, void* context, void* data)
    {
        auto* const ssl = static_cast<SSL_CTX*>(context);
        SSL_CTX_set_app_data(ssl, data);
        SSL_CTX_set_verify(ssl, SSL_VERIFY_PEER, VerifyPin);
        return CURLE_OK;
    }

    std::size_t Collect(char* data, std::size_t size, std::size_t count, void* into)
    {
        auto* const body = static_cast<std::string*>(into);
        std::size_t const bytes = size * count;
        if (body->size() + bytes > CurlTransport::MaxBodyBytes)
            return 0;
        body->append(data, bytes);
        return bytes;
    }

    void Initialise()
    {
        static std::once_flag once;
        std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    }
}

PanelReply CurlTransport::Get(PanelAddress const& address, std::string_view path)
{
    Initialise();
    PanelReply reply;
    CURL* const curl = curl_easy_init();
    if (curl == nullptr)
    {
        reply.Error = "libcurl could not start a request";
        return reply;
    }
    std::string const url = address.Origin.Describe() + std::string(path);
    std::string body;
    PinCheck check{ address.Pin, {}, false };
    char failure[CURL_ERROR_SIZE] = {};
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(Timeout.count()));
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Collect);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, failure);
    if (address.Trust == PanelTrust::Pinned)
    {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_CTX_FUNCTION, PrepareContext);
        curl_easy_setopt(curl, CURLOPT_SSL_CTX_DATA, &check);
    }
    else
    {
#ifdef _WIN32
        curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, static_cast<long>(CURLSSLOPT_NATIVE_CA));
#endif
    }

    CURLcode const result = curl_easy_perform(curl);
    if (check.Refused)
    {
        reply.PinRefused = true;
        reply.Served = check.Served;
        reply.Error = "the certificate is not the pinned one";
    }
    else if (result != CURLE_OK)
    {
        reply.Error = failure[0] != '\0' ? std::string(failure) : std::string(curl_easy_strerror(result));
    }
    else
    {
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        reply.Connected = true;
        reply.Status = static_cast<int>(status);
        reply.Body = std::move(body);
    }
    curl_easy_cleanup(curl);
    return reply;
}
