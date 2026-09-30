/*
 * Project Ambrose by Imjustchico
 * A browser for panel tests: it makes the owner from the link the panel logged, signs in with a name and password or trades a sign-in link, from loopback or as the far address a trusted proxy names, and, when the panel asks for a second factor, keeps the challenge cookie and answers it with a code or a recovery code, sends every request over TLS when the panel serves it with its session cookie, this listener's origin and the session's CSRF token, reads each cookie an answer sets by its name, turns two-factor sign-in on from the secret a setup answer carries with the code for a moment the test chooses, and says where any of a list of secrets turns up in the audit rows, their subjects or the captured log.
 */

#ifndef AMBROSE_PANELTESTSESSION_H
#define AMBROSE_PANELTESTSESSION_H

#include "AdminClient.h"
#include "Base32.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "Totp.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace PanelTest
{
    inline constexpr std::string_view Password = "a good long password";

    struct Browser
    {
        std::string Cookie = {};
        std::string Csrf = {};
        std::string Challenge = {};
        int64 UserId = 0;
    };

    struct Enrolled
    {
        std::vector<uint8> Secret = {};
        std::vector<std::string> Codes = {};
        std::string Body = {};
    };

    inline std::vector<std::string> SetCookies(AdminClientResponse const& answer)
    {
        std::vector<std::string> cookies;
        std::string_view head = answer.Head;
        constexpr std::string_view Header = "set-cookie:";
        while (!head.empty())
        {
            std::size_t const end = head.find("\r\n");
            std::string_view const line = head.substr(0, end);
            if (line.size() > Header.size())
            {
                std::string lowered(line.substr(0, Header.size()));
                for (char& c : lowered)
                    c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
                if (lowered == Header)
                {
                    std::string_view value = line.substr(Header.size());
                    while (!value.empty() && value.front() == ' ')
                        value.remove_prefix(1);
                    cookies.emplace_back(value.substr(0, value.find(';')));
                }
            }
            if (end == std::string_view::npos)
                break;
            head.remove_prefix(end + 2);
        }
        return cookies;
    }

    inline std::string CookieEndingWith(AdminClientResponse const& answer, bool challenge)
    {
        for (std::string const& cookie : SetCookies(answer))
        {
            std::size_t const equals = cookie.find('=');
            if (equals == std::string::npos || equals + 1 == cookie.size())
                continue;
            bool const named = std::string_view(cookie).substr(0, equals).ends_with(Panel::ChallengeCookie);
            if (named == challenge)
                return cookie;
        }
        return {};
    }

    inline std::string SessionCookie(AdminClientResponse const& answer)
    {
        return CookieEndingWith(answer, false);
    }

    inline std::string ChallengeCookie(AdminClientResponse const& answer)
    {
        return CookieEndingWith(answer, true);
    }

    inline nlohmann::json Json(AdminClientResponse const& answer)
    {
        return nlohmann::json::parse(answer.Body, nullptr, false);
    }

    inline std::string ErrorOf(AdminClientResponse const& answer)
    {
        nlohmann::json const body = Json(answer);
        return body.is_object() ? body.value("error", std::string()) : std::string();
    }

    inline AdminClientResponse Send(Panel& panel, std::string method, std::string path, nlohmann::json const& body = nullptr, Browser const* browser = nullptr,
        std::string const& forwardedFor = {})
    {
        AdminClient const client("127.0.0.1", panel.GetPort(), "", panel.IsSecure());
        AdminClientRequest request{ std::move(method), std::move(path), body.is_null() ? std::string() : body.dump(), "application/json", "" };
        if (browser)
        {
            std::string cookie = browser->Cookie;
            if (!browser->Challenge.empty())
                cookie += (cookie.empty() ? "" : "; ") + browser->Challenge;
            if (!cookie.empty())
                request.Headers.emplace_back("Cookie", cookie);
        }
        if (!forwardedFor.empty())
            request.Headers.emplace_back("X-Forwarded-For", forwardedFor);
        if (request.Method != "GET")
        {
            request.Headers.emplace_back("Origin", std::string(panel.IsSecure() ? "https" : "http") + "://127.0.0.1:" + std::to_string(panel.GetPort()));
            if (browser && !browser->Csrf.empty())
                request.Headers.emplace_back("X-CSRF-Token", browser->Csrf);
        }
        return client.Send(request, std::chrono::seconds(30));
    }

    inline void Adopt(Browser& browser, AdminClientResponse const& answer)
    {
        nlohmann::json const body = Json(answer);
        browser.Cookie = SessionCookie(answer);
        browser.Csrf = body.is_object() ? body.value("csrf", std::string()) : std::string();
        browser.Challenge.clear();
        if (body.is_object() && body.contains("user") && body["user"].is_object())
            browser.UserId = body["user"].value("id", int64{ 0 });
    }

    inline std::string ClaimToken(LogTestHarness& harness)
    {
        std::string token;
        for (std::string const& line : harness.Store().Texts("Capture"))
        {
            std::size_t const at = line.find("#claim?token=");
            if (at != std::string::npos)
                token = line.substr(at + 13);
        }
        return token;
    }

    inline bool ClaimOwner(Panel& panel, LogTestHarness& harness, Browser& browser, std::string const& username = "owner")
    {
        AdminClientResponse const claimed = Send(panel, "POST", "/api/panel/claim", { { "token", ClaimToken(harness) }, { "username", username }, { "password", Password } });
        if (claimed.Status != 200)
            return false;
        Adopt(browser, claimed);
        return !browser.Cookie.empty() && !browser.Csrf.empty() && browser.UserId != 0;
    }

    inline AdminClientResponse SignIn(Panel& panel, Browser& browser, std::string const& username, std::string_view password = Password)
    {
        AdminClientResponse const answer = Send(panel, "POST", "/api/panel/session", { { "username", username }, { "password", std::string(password) } });
        if (answer.Status != 200)
            return answer;
        nlohmann::json const body = Json(answer);
        if (body.is_object() && body.value("second_factor", false))
            browser.Challenge = ChallengeCookie(answer);
        else
            Adopt(browser, answer);
        return answer;
    }

    inline AdminClientResponse LinkIn(Panel& panel, Browser& browser, std::string const& token, std::string const& forwardedFor = {})
    {
        AdminClientResponse const answer = Send(panel, "POST", std::string(Panel::LinkPath), { { "token", token } }, nullptr, forwardedFor);
        if (answer.Status != 200)
            return answer;
        nlohmann::json const body = Json(answer);
        if (body.is_object() && body.value("second_factor", false))
            browser.Challenge = ChallengeCookie(answer);
        else
            Adopt(browser, answer);
        return answer;
    }

    inline std::string TokenIn(std::string_view link)
    {
        constexpr std::string_view Marker = "token=";
        std::size_t const at = link.find(Marker);
        if (at == std::string_view::npos)
            return {};
        std::string_view const rest = link.substr(at + Marker.size());
        return std::string(rest.substr(0, rest.find('&')));
    }

    inline std::vector<std::string> ScanFor(Panel& panel, LogTestHarness& harness, std::vector<std::string> const& secrets)
    {
        std::vector<std::string> found;
        auto const look = [&](std::string const& text, std::string const& place)
        {
            for (std::string const& secret : secrets)
                if (!secret.empty() && text.find(secret) != std::string::npos)
                    found.push_back(secret + " in " + place);
        };
        std::string error;
        std::optional<PanelStore::Statement> events = panel.Store().Prepare(
            "SELECT name, COALESCE(event_id, ''), COALESCE(actor_id, ''), COALESCE(actor_name, ''), COALESCE(address, ''), COALESCE(user_agent, ''), COALESCE(node, ''),"
            " COALESCE(error, ''), COALESCE(reason, ''), COALESCE(properties, '') FROM audit_event", error);
        if (!events)
        {
            found.push_back("audit_event could not be read: " + error);
            return found;
        }
        while (events->Step(error))
            for (int column = 0; column < 10; ++column)
                look(events->Text(column), "audit_event " + events->Text(0));
        events.reset();
        std::optional<PanelStore::Statement> subjects = panel.Store().Prepare("SELECT kind, COALESCE(subject_id, ''), COALESCE(name, '') FROM audit_subject", error);
        if (!subjects)
        {
            found.push_back("audit_subject could not be read: " + error);
            return found;
        }
        while (subjects->Step(error))
            for (int column = 0; column < 3; ++column)
                look(subjects->Text(column), "audit_subject " + subjects->Text(0));
        subjects.reset();
        for (std::string const& line : harness.Store().Texts("Capture"))
            look(line, "the log");
        return found;
    }

    inline AdminClientResponse SecondFactor(Panel& panel, Browser& browser, nlohmann::json const& body)
    {
        AdminClientResponse const answer = Send(panel, "POST", "/api/panel/session/second-factor", body, &browser);
        if (answer.Status == 200)
            Adopt(browser, answer);
        return answer;
    }

    inline std::vector<uint8> SecretOf(AdminClientResponse const& setup)
    {
        nlohmann::json const body = Json(setup);
        if (!body.is_object() || !body.contains("secret") || !body["secret"].is_string())
            return {};
        return Base32::Decode(body["secret"].get<std::string>()).value_or(std::vector<uint8>());
    }

    inline std::string CodeAt(std::vector<uint8> const& secret, int64 unixSeconds)
    {
        return Totp::Format(Totp::Code(secret, Totp::StepAt(unixSeconds)));
    }

    inline std::string WrongCode(std::vector<uint8> const& secret, int64 unixSeconds)
    {
        std::vector<std::string> taken;
        for (int64 offset = -3; offset <= 3; ++offset)
            taken.push_back(CodeAt(secret, unixSeconds + offset * static_cast<int64>(Totp::StepSeconds)));
        for (uint32 candidate = 0;; ++candidate)
        {
            std::string const code = Totp::Format(candidate);
            if (std::find(taken.begin(), taken.end(), code) == taken.end())
                return code;
        }
    }

    inline std::optional<Enrolled> Enroll(Panel& panel, Browser& browser, int64 unixSeconds, std::string_view password = Password)
    {
        AdminClientResponse const setup = Send(panel, "POST", "/api/panel/me/two-factor/setup", nlohmann::json::object(), &browser);
        if (setup.Status != 200)
            return std::nullopt;
        Enrolled enrolled;
        enrolled.Secret = SecretOf(setup);
        AdminClientResponse const enable = Send(panel, "POST", "/api/panel/me/two-factor/enable",
            { { "password", std::string(password) }, { "code", CodeAt(enrolled.Secret, unixSeconds) } }, &browser);
        if (enable.Status != 200)
            return std::nullopt;
        nlohmann::json const body = Json(enable);
        if (!body.is_object() || !body.contains("recovery_codes") || !body["recovery_codes"].is_array())
            return std::nullopt;
        for (nlohmann::json const& code : body["recovery_codes"])
            if (code.is_string())
                enrolled.Codes.push_back(code.get<std::string>());
        enrolled.Body = enable.Body;
        return enrolled;
    }
}

#endif
