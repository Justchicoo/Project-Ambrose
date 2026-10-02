/*
 * Project Ambrose by Imjustchico
 * The catalog's sentences and their classes, kept sorted by name so a lookup is a binary search, and the rendering: each {placeholder} is filled from the row, the actor by name or kind, the app, the other operator, a setting, a permission, a file, a reload target or a property by its key, a missing value falls back to a plain noun, and every value passes through the same cut and control-character clearing the logs use, so a name chosen to break a page reads as text.
 */

#include "PanelActivityCatalog.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <map>

namespace
{
    constexpr PanelActivityClass Security = PanelActivityClass::Security;
    constexpr PanelActivityClass HighVolume = PanelActivityClass::HighVolume;

    constexpr std::array Sentences{
        PanelActivitySentence{ "app:api.change", "{actor} changed {app} through its admin API", Security },
        PanelActivitySentence{ "app:console.command", "{actor} ran a console command on {app}", HighVolume },
        PanelActivitySentence{ "app:permission.refused", "{actor} was refused {permission} on {app}", Security },
        PanelActivitySentence{ "app:power.kill", "{actor} ended {app} at once", Security },
        PanelActivitySentence{ "app:power.refused", "{actor} asked {app} for a power action it does not know", Security },
        PanelActivitySentence{ "app:power.restart", "{actor} restarted {app}", Security },
        PanelActivitySentence{ "app:power.start", "{actor} started {app}", Security },
        PanelActivitySentence{ "app:power.stop", "{actor} stopped {app}", Security },
        PanelActivitySentence{ "errors:group.cleared", "{actor} cleared the error group {error_group}", Security },
        PanelActivitySentence{ "errors:report.created", "{actor} made an error report", Security },
        PanelActivitySentence{ "file:path.refused", "{actor} was refused the file {file}", HighVolume },
        PanelActivitySentence{ "file:rules.changed", "{actor} changed the protected paths of {root}", Security },
        PanelActivitySentence{ "panel:activity.exported", "{actor} exported the activity log as {format}", Security },
        PanelActivitySentence{ "panel:activity.swept", "The retention sweep removed {removed} activity rows", Security },
        PanelActivitySentence{ "panel:link.issued", "{actor} issued a {kind} link for {user}", Security },
        PanelActivitySentence{ "panel:link.refused", "{actor} was refused a sign-in link", Security },
        PanelActivitySentence{ "panel:link.throttled", "Sign-in links from this address were held back", Security },
        PanelActivitySentence{ "panel:permission.refused", "{actor} was refused {permission}", Security },
        PanelActivitySentence{ "panel:request.throttled", "{actor} was held back for sending too many requests to {route}", HighVolume },
        PanelActivitySentence{ "panel:session.challenged", "{actor} was asked for a second factor", Security },
        PanelActivitySentence{ "panel:session.closed", "{actor} signed out", Security },
        PanelActivitySentence{ "panel:session.opened", "{actor} signed in", Security },
        PanelActivitySentence{ "panel:session.refused", "A sign-in as {user} was refused", Security },
        PanelActivitySentence{ "panel:session.second_factor_refused", "{actor} gave a second factor that was refused", Security },
        PanelActivitySentence{ "panel:session.second_factor_throttled", "Second factors for {user} were held back", Security },
        PanelActivitySentence{ "panel:session.throttled", "Sign-ins as {user} were held back", Security },
        PanelActivitySentence{ "panel:settings.changed", "{actor} changed the panel settings", Security },
        PanelActivitySentence{ "panel:step_up.checked", "{actor} confirmed who they are", Security },
        PanelActivitySentence{ "panel:step_up.required", "{actor} was asked to confirm who they are before using {permission}", Security },
        PanelActivitySentence{ "panel:step_up.used", "{actor} used a fresh check of who they are for {permission}", Security },
        PanelActivitySentence{ "panel:user.created", "{actor} added the operator {user}", Security },
        PanelActivitySentence{ "panel:user.password_set", "{actor} set the password of {user}", Security },
        PanelActivitySentence{ "panel:user.two_factor_reset", "{actor} reset the two-factor sign-in of {user}", Security },
        PanelActivitySentence{ "reload:target.run", "{actor} reloaded {target} on {app}", Security },
        PanelActivitySentence{ "settings:batch.changed", "{actor} changed {setting} on {app}", Security },
        PanelActivitySentence{ "settings:secret.revealed", "{actor} revealed {setting} on {app}", Security },
        PanelActivitySentence{ "settings:setting.changed", "{actor} changed {setting} on {app}", Security },
        PanelActivitySentence{ "settings:setting.reset", "{actor} returned {setting} on {app} to its config value", Security },
        PanelActivitySentence{ "user:recovery_codes.issued", "{actor} made new recovery codes", Security },
        PanelActivitySentence{ "user:two_factor.disabled", "{actor} turned off two-factor sign-in", Security },
        PanelActivitySentence{ "user:two_factor.enabled", "{actor} turned on two-factor sign-in", Security },
    };

    constexpr bool Sorted()
    {
        for (std::size_t index = 1; index < Sentences.size(); ++index)
            if (!(Sentences[index - 1].Name < Sentences[index].Name))
                return false;
        return true;
    }

    static_assert(Sorted(), "the catalog is kept sorted by name with no name twice");

    std::string Clean(std::string_view value)
    {
        std::string_view const kept = Ambrose::TruncateUtf8(value, PanelActivityCatalog::MaxValueBytes);
        std::string cleaned = Ambrose::ForLog(kept, kept.size());
        if (kept.size() < value.size())
            cleaned += "...";
        return cleaned;
    }

    std::string RawActor(AuditEvent const& event)
    {
        if (!event.ActorName.empty())
            return event.ActorName;
        switch (event.Actor)
        {
            case AuditActor::Token: return "A script with the panel token";
            case AuditActor::Schedule: return "A schedule";
            case AuditActor::System: return "The panel";
            case AuditActor::User: break;
        }
        return event.ActorId.empty() ? std::string("Somebody") : fmt::format("Operator {}", event.ActorId);
    }

    std::string PropertyText(nlohmann::json const& properties, std::string_view key)
    {
        if (!properties.is_object())
            return {};
        auto const found = properties.find(std::string(key));
        if (found == properties.end())
            return {};
        if (found->is_string())
            return found->get<std::string>();
        if (found->is_number_integer())
            return std::to_string(found->get<int64>());
        if (found->is_array())
        {
            std::string joined;
            for (nlohmann::json const& item : *found)
                if (item.is_string())
                    joined += (joined.empty() ? "" : ", ") + item.get<std::string>();
            return joined;
        }
        return {};
    }

    std::string SubjectText(AuditEvent const& event, std::string_view kind)
    {
        std::string joined;
        for (AuditSubject const& subject : event.Subjects)
            if (subject.Kind == kind)
                joined += (joined.empty() ? "" : ", ") + (subject.Name.empty() ? subject.Id : subject.Name);
        return joined;
    }

    std::string OtherOperator(AuditEvent const& event)
    {
        AuditSubject const* first = nullptr;
        for (AuditSubject const& subject : event.Subjects)
        {
            if (subject.Kind != "panel_user")
                continue;
            if (!first)
                first = &subject;
            bool const isActor = (!subject.Id.empty() && subject.Id == event.ActorId) || (!subject.Name.empty() && subject.Name == event.ActorName);
            if (!isActor)
                return subject.Name.empty() ? subject.Id : subject.Name;
        }
        if (!first)
            return {};
        return first->Name.empty() ? first->Id : first->Name;
    }

    std::string Value(AuditEvent const& event, nlohmann::json const& properties, std::string_view key)
    {
        if (key == "actor")
            return RawActor(event);
        if (key == "app")
        {
            std::string app = SubjectText(event, "app");
            if (app.empty())
                app = PropertyText(properties, "app");
            return app.empty() ? event.Node : app;
        }
        if (key == "user")
            return OtherOperator(event);
        if (key == "setting")
        {
            std::string const keys = SubjectText(event, "setting");
            return keys.empty() ? PropertyText(properties, "keys") : keys;
        }
        if (key == "permission")
        {
            std::string const permission = SubjectText(event, "permission");
            return permission.empty() ? PropertyText(properties, "permission") : permission;
        }
        if (key == "target")
        {
            std::string const target = SubjectText(event, "reload_target");
            return target.empty() ? PropertyText(properties, "target") : target;
        }
        if (key == "root")
            return SubjectText(event, "file_root");
        if (key == "file")
            return SubjectText(event, "file");
        if (key == "route")
            return SubjectText(event, "route");
        if (key == "error_group")
            return SubjectText(event, "error_group");
        if (key == "kind")
        {
            std::string const kind = PropertyText(properties, "kind");
            return kind.empty() ? PropertyText(properties, "link_kind") : kind;
        }
        return PropertyText(properties, key);
    }

    std::string_view Fallback(std::string_view key)
    {
        static std::map<std::string_view, std::string_view> const nouns{
            { "actor", "Somebody" }, { "app", "an app" }, { "user", "an operator" }, { "setting", "settings" }, { "permission", "a permission" },
            { "target", "a target" }, { "root", "a file root" }, { "file", "a path" }, { "route", "a route" }, { "error_group", "a group" },
            { "kind", "sign-in" }, { "removed", "some" }, { "format", "a file" }
        };
        auto const found = nouns.find(key);
        return found == nouns.end() ? std::string_view("something") : found->second;
    }
}

std::span<PanelActivitySentence const> PanelActivityCatalog::All()
{
    return Sentences;
}

PanelActivitySentence const* PanelActivityCatalog::Find(std::string_view name)
{
    auto const found = std::lower_bound(Sentences.begin(), Sentences.end(), name,
        [](PanelActivitySentence const& sentence, std::string_view wanted) { return sentence.Name < wanted; });
    return found != Sentences.end() && found->Name == name ? &*found : nullptr;
}

PanelActivityClass PanelActivityCatalog::ClassOf(std::string_view name)
{
    PanelActivitySentence const* const sentence = Find(name);
    return sentence ? sentence->Class : PanelActivityClass::Security;
}

std::string_view PanelActivityCatalog::ClassName(PanelActivityClass value) noexcept
{
    return value == PanelActivityClass::HighVolume ? "high_volume" : "security";
}

std::vector<std::string> PanelActivityCatalog::NamesOf(PanelActivityClass value)
{
    std::vector<std::string> names;
    for (PanelActivitySentence const& sentence : Sentences)
        if (sentence.Class == value)
            names.emplace_back(sentence.Name);
    return names;
}

std::string PanelActivityCatalog::ActorText(AuditEvent const& event)
{
    return Clean(RawActor(event));
}

std::string PanelActivityCatalog::Render(AuditEvent const& event)
{
    PanelActivitySentence const* const sentence = Find(event.Name);
    std::string_view const text = sentence ? sentence->Text : std::string_view("{actor} did {event}");
    nlohmann::json const properties = nlohmann::json::parse(event.Properties.empty() ? std::string("{}") : event.Properties, nullptr, false);
    std::string rendered;
    rendered.reserve(text.size() + 32);
    std::size_t at = 0;
    while (at < text.size())
    {
        std::size_t const open = text.find('{', at);
        if (open == std::string_view::npos)
        {
            rendered.append(text.substr(at));
            break;
        }
        std::size_t const close = text.find('}', open);
        if (close == std::string_view::npos)
        {
            rendered.append(text.substr(at));
            break;
        }
        rendered.append(text.substr(at, open - at));
        std::string_view const key = text.substr(open + 1, close - open - 1);
        std::string value = key == "event" ? event.Name : Value(event, properties, key);
        rendered += value.empty() ? std::string(Fallback(key)) : Clean(value);
        at = close + 1;
    }
    return rendered;
}
