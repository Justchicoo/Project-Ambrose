/*
 * Project Ambrose by Imjustchico
 * Sends the panel's test mail over SMTP through the outbound HTTP library, reaching only the address it is given and reporting the SMTP server's own reply when delivery fails.
 */

#include "PanelMail.h"

#include <curl/curl.h>

#include <cstring>
#include <mutex>
#include <vector>

namespace
{
    void EnsureCurl()
    {
        static std::once_flag flag;
        std::call_once(flag, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    }

    struct UploadState
    {
        std::string const* Body = nullptr;
        std::size_t Sent = 0;
    };

    std::size_t ReadCallback(char* buffer, std::size_t size, std::size_t count, void* userp)
    {
        UploadState* const state = static_cast<UploadState*>(userp);
        std::size_t const left = state->Body->size() - state->Sent;
        std::size_t const take = (size * count < left) ? size * count : left;
        if (take > 0)
        {
            std::memcpy(buffer, state->Body->data() + state->Sent, take);
            state->Sent += take;
        }
        return take;
    }

    int DebugCallback(CURL*, curl_infotype type, char* data, std::size_t size, void* userp)
    {
        if (type == CURLINFO_HEADER_IN && size > 2)
        {
            std::string* const lastReply = static_cast<std::string*>(userp);
            std::string line(data, size);
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
                line.pop_back();
            if (!line.empty() && line.front() == '<')
                line.erase(0, 1);
            while (!line.empty() && line.front() == ' ')
                line.erase(0, 1);
            if (!line.empty() && (line.front() == '4' || line.front() == '5'))
                *lastReply = std::move(line);
        }
        return 0;
    }

    bool BreaksAHeader(std::string_view value)
    {
        return value.find_first_of("\r\n<>\"") != std::string_view::npos;
    }

    std::string MailboxOf(std::string_view name, std::string_view address)
    {
        if (name.empty())
            return "<" + std::string(address) + ">";
        return "\"" + std::string(name) + "\" <" + std::string(address) + ">";
    }
}

PanelMailResult PanelMail::SendTestMail(PanelMailSettings const& settings, std::string_view toAddress)
{
    PanelMailResult result;
    if (settings.SmtpHost.empty() || toAddress.empty() || settings.FromAddress.empty())
    {
        result.Error = "the SMTP host, the sender and the recipient are all required";
        return result;
    }
    if (BreaksAHeader(toAddress) || BreaksAHeader(settings.FromAddress) || BreaksAHeader(settings.FromName) || BreaksAHeader(settings.SmtpHost))
    {
        result.Error = "the SMTP host, the sender, the sender's name and the recipient may not hold a line break, an angle bracket or a quote";
        return result;
    }
    EnsureCurl();

    std::string const scheme = settings.TlsMode == "implicit" ? "smtps" : "smtp";
    std::string const url = scheme + "://" + settings.SmtpHost + ":" + std::to_string(settings.SmtpPort);

    std::string const body =
        "From: " + MailboxOf(settings.FromName, settings.FromAddress) + "\r\n"
        "To: <" + std::string(toAddress) + ">\r\n"
        "Subject: Ambrose panel mail test\r\n"
        "\r\n"
        "This is a test message from the Ambrose panel's mail settings.\r\n"
        "If it reached you, the mail settings work.\r\n";

    CURL* const curl = curl_easy_init();
    if (!curl)
    {
        result.Error = "the mail sender could not start";
        return result;
    }

    char errorBuffer[CURL_ERROR_SIZE] = {};
    std::string lastReply;
    UploadState upload{ &body, 0 };
    struct curl_slist* recipients = nullptr;
    recipients = curl_slist_append(recipients, ("<" + std::string(toAddress) + ">").c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, ("<" + settings.FromAddress + ">").c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    if (!settings.Username.empty())
    {
        curl_easy_setopt(curl, CURLOPT_USERNAME, settings.Username.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, settings.Password.c_str());
    }
    if (settings.TlsMode == "starttls")
        curl_easy_setopt(curl, CURLOPT_USE_SSL, static_cast<long>(CURLUSESSL_ALL));
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, ReadCallback);
    curl_easy_setopt(curl, CURLOPT_READDATA, &upload);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, DebugCallback);
    curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &lastReply);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 30000L);

    CURLcode const code = curl_easy_perform(curl);
    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);

    if (code == CURLE_OK)
    {
        result.Sent = true;
        return result;
    }
    if (!lastReply.empty())
        result.Error = "the mail server refused the message: " + lastReply;
    else if (errorBuffer[0] != '\0')
        result.Error = std::string("the mail server could not be reached: ") + errorBuffer;
    else
        result.Error = std::string("the mail server could not be reached: ") + curl_easy_strerror(code);
    return result;
}
