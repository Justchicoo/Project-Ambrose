/*
 * Project Ambrose by Imjustchico
 * The sentences themselves, one per event name the panel emits, with {actor}, {subject}, {address}, {reason} and {error} filled from the row and every value escaped so a hostile name cannot break the sentence or the page.
 */

#include "PanelActivityCatalog.h"
#include "PanelAudit.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>

namespace
{
    using PanelActivityCatalog::Sentence;

    std::vector<Sentence> const kSentences = {
        { "app:api.change", "{actor} changed the API surface of {subject}" },
        { "app:console.command", "{actor} ran a console command on {subject}" },
        { "app:permission.refused", "{actor} was refused permission on {subject}: {reason}" },
        { "app:power.refused", "{actor} was refused a power action on {subject}: {reason}" },
        { "app:power.restart", "{actor} restarted {subject}" },
        { "app:power.stop", "{actor} stopped {subject}" },
        { "errors:group.cleared", "{actor} cleared the error group {subject}" },
        { "errors:report.created", "{actor} created the error report {subject}" },
        { "file:batch.changed", "{actor} changed {subject} in a batch" },
        { "file:path.refused", "{actor} was refused access to the path {subject}: {reason}" },
        { "file:rules.changed", "{actor} changed the file rules for {subject}" },
        { "panel:link.issued", "{actor} issued the link {subject}" },
        { "panel:link.refused", "{actor} was refused the link {subject}: {reason}" },
        { "panel:link.throttled", "{actor} was throttled issuing the link {subject}" },
        { "panel:note.added", "{actor} added the note {subject}" },
        { "panel:permission.refused", "{actor} was refused the panel permission {subject}: {reason}" },
        { "panel:request.throttled", "{actor} was throttled at {subject}" },
        { "panel:session.challenged", "{actor} faced a session challenge at {address}" },
        { "panel:session.closed", "{actor} closed the session {subject}" },
        { "panel:session.opened", "{actor} opened a session from {address}" },
        { "panel:session.refused", "{actor} was refused a session from {address}: {reason}" },
        { "panel:session.second_factor_refused", "{actor} was refused at the second factor from {address}: {reason}" },
        { "panel:session.second_factor_throttled", "{actor} was throttled at the second factor from {address}" },
        { "panel:session.throttled", "{actor} was throttled signing in from {address}" },
        { "panel:settings.changed", "{actor} changed the panel setting {subject}" },
        { "panel:settings.mail_tested", "{actor} tested the panel mail settings" },
        { "panel:step_up.checked", "{actor} passed a step-up check for {subject}" },
        { "panel:step_up.required", "{actor} was asked to step up for {subject}" },
        { "panel:step_up.used", "{actor} completed step-up authentication from {address}" },
        { "panel:user.created", "{actor} created the panel user {subject}" },
        { "panel:user.password_set", "{actor} set the password for {subject}" },
        { "panel:user.two_factor_reset", "{actor} reset two-factor for {subject}" },
        { "reload:target.run", "{actor} ran the reload target {subject}" },
        { "settings:batch.changed", "{actor} changed settings in a batch on {subject}" },
        { "settings:secret.revealed", "{actor} revealed the secret {subject}" },
        { "settings:setting.changed", "{actor} changed the setting {subject}" },
        { "settings:setting.reset", "{actor} reset the setting {subject}" },
        { "user:recovery_codes.issued", "{actor} issued recovery codes for {subject}" },
        { "user:two_factor.disabled", "{actor} disabled two-factor for {subject}" },
        { "user:two_factor.enabled", "{actor} enabled two-factor for {subject}" },
        { "activity:sweep.ran", "The hourly sweep removed {subject} expired rows" },
        { "activity:export.created", "{actor} exported the activity log as {subject}" },
    };

    std::string Pick(std::vector<std::string> const& values, std::string_view fallback)
    {
        for (std::string const& value : values)
            if (!value.empty())
                return value;
        return std::string(fallback);
    }
}

namespace PanelActivityCatalog
{
    std::vector<Sentence> const& All()
    {
        return kSentences;
    }

    bool Has(std::string_view name) noexcept
    {
        return std::any_of(kSentences.begin(), kSentences.end(), [name](Sentence const& s) { return s.Name == name; });
    }

    std::string Escape(std::string_view value)
    {
        std::string out;
        out.reserve(value.size());
        for (char c : value)
        {
            switch (c)
            {
                case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break;
                case '&': out += "&amp;"; break;
                case '"': out += "&quot;"; break;
                default: out += c; break;
            }
        }
        return out;
    }

    std::string Render(std::string_view name, AuditEvent const& event)
    {
        auto found = std::find_if(kSentences.begin(), kSentences.end(), [name](Sentence const& s) { return s.Name == name; });
        if (found == kSentences.end())
            return fmt::format("Unknown event {}", Escape(name));

        std::string actor = Pick({ event.ActorName, event.ActorId }, "someone");
        std::string subject;
        if (!event.Subjects.empty())
            subject = Pick({ event.Subjects.front().Name, event.Subjects.front().Id }, event.Subjects.front().Kind);
        std::string address = event.Address.empty() ? "an unknown address" : event.Address;
        std::string reason = event.Reason.empty() ? "no reason given" : event.Reason;
        std::string error = event.Error.empty() ? "no error given" : event.Error;

        std::string text(found->Text);
        auto replace = [&text](std::string_view from, std::string const& to)
        {
            std::string::size_type at = 0;
            while ((at = text.find(from, at)) != std::string::npos)
            {
                text.replace(at, from.size(), to);
                at += to.size();
            }
        };
        replace("{actor}", Escape(actor));
        replace("{subject}", Escape(subject));
        replace("{address}", Escape(address));
        replace("{reason}", Escape(reason));
        replace("{error}", Escape(error));
        return text;
    }
}
