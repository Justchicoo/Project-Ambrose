/*
 * Project Ambrose by Imjustchico
 * Verifies a sign-in captcha answer against its provider through the outbound HTTP library, failing closed when the provider cannot be reached or is not one the panel knows.
 */

#include "PanelCaptcha.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstring>
#include <mutex>

namespace
{
    void EnsureCurl()
    {
        static std::once_flag flag;
        std::call_once(flag, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    }

    std::size_t WriteCallback(char* data, std::size_t size, std::size_t count, void* userp)
    {
        std::string* const body = static_cast<std::string*>(userp);
        body->append(data, size * count);
        return size * count;
    }

    FILE* DevNull()
    {
#ifdef _WIN32
        static FILE* nullOut = std::fopen("NUL", "w");
#else
        static FILE* nullOut = std::fopen("/dev/null", "w");
#endif
        return nullOut;
    }

    std::string UrlEncode(CURL* curl, std::string_view value)
    {
        char* const encoded = curl_easy_escape(curl, value.data(), static_cast<int>(value.size()));
        std::string result = encoded != nullptr ? encoded : std::string(value);
        curl_free(encoded);
        return result;
    }
}

std::string PanelCaptcha::VerifyUrlFor(std::string_view provider)
{
    if (provider == "recaptcha")
        return "https://www.google.com/recaptcha/api/siteverify";
    if (provider == "hcaptcha")
        return "https://api.hcaptcha.com/siteverify";
    if (provider == "turnstile")
        return "https://challenges.cloudflare.com/turnstile/v0/siteverify";
    return {};
}

PanelCaptchaResult PanelCaptcha::Verify(std::string_view provider, std::string_view secret, std::string_view token, std::string_view remoteIp, std::string_view verifyUrl)
{
    PanelCaptchaResult result;
    if (provider.empty() || provider == "off" || secret.empty() || token.empty() || verifyUrl.empty())
    {
        result.Result = PanelCaptchaResult::Outcome::Misconfigured;
        result.Detail = "the captcha provider, its secret, the answer and the verification address are all required";
        return result;
    }
    EnsureCurl();

    CURL* const curl = curl_easy_init();
    if (!curl)
    {
        result.Result = PanelCaptchaResult::Outcome::Unreachable;
        result.Detail = "the captcha check could not start";
        return result;
    }

    std::string const fields = "secret=" + UrlEncode(curl, secret) + "&response=" + UrlEncode(curl, token) + "&remoteip=" + UrlEncode(curl, remoteIp);
    std::string answer;
    char errorBuffer[CURL_ERROR_SIZE] = {};

    curl_easy_setopt(curl, CURLOPT_URL, std::string(verifyUrl).c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, fields.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &answer);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    curl_easy_setopt(curl, CURLOPT_STDERR, DevNull());
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);

    CURLcode const code = curl_easy_perform(curl);
    long httpCode = 0;
    if (code == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK || httpCode != 200)
    {
        result.Result = PanelCaptchaResult::Outcome::Unreachable;
        if (code != CURLE_OK && errorBuffer[0] != '\0')
            result.Detail = std::string("the captcha provider could not be reached: ") + errorBuffer;
        else if (code != CURLE_OK)
            result.Detail = std::string("the captcha provider could not be reached: ") + curl_easy_strerror(code);
        else
            result.Detail = "the captcha provider answered HTTP " + std::to_string(httpCode) + " instead of verifying the answer";
        return result;
    }

    nlohmann::json const reply = nlohmann::json::parse(answer, nullptr, false);
    if (!reply.is_object() || !reply.contains("success") || !reply["success"].is_boolean())
    {
        result.Result = PanelCaptchaResult::Outcome::Unreachable;
        result.Detail = "the captcha provider answered in a form the panel does not understand";
        return result;
    }
    if (reply["success"].get<bool>())
    {
        result.Result = PanelCaptchaResult::Outcome::Verified;
        return result;
    }
    result.Result = PanelCaptchaResult::Outcome::Invalid;
    if (reply.contains("error-codes") && reply["error-codes"].is_array() && !reply["error-codes"].empty() && reply["error-codes"][0].is_string())
        result.Detail = "the captcha provider refused the answer: " + reply["error-codes"][0].get<std::string>();
    else
        result.Detail = "the captcha provider refused the answer";
    return result;
}
