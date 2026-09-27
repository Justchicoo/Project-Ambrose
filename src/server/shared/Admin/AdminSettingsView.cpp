/*
 * Project Ambrose by Imjustchico
 * Reads a change's body whole before anything is set, so a missing reason and a value out of bounds are both named in one answer, checks a restricted key's right before saying anything about its value, and hands the registry the change, whose own checks decide it; a batch is read and checked entry by entry, every refusal listed with its code, and applied through the registry's all-or-nothing batch. Every value in an answer passes through the registry's mask, and history is never revealed.
 */

#include "AdminSettingsView.h"
#include "AdminRouter.h"
#include "StringUtil.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr std::string_view SettingsPrefix = "/api/settings/";
    constexpr std::string_view HistorySuffix = "/history";

    std::optional<std::string> ValueText(nlohmann::json const& value)
    {
        if (value.is_string())
            return value.get<std::string>();
        if (value.is_boolean())
            return std::string(value.get<bool>() ? "true" : "false");
        if (value.is_number())
            return value.dump();
        return std::nullopt;
    }

    std::optional<std::string> ReasonText(nlohmann::json const& body, std::vector<std::pair<std::string, std::string>>& fields)
    {
        auto const reason = body.find("reason");
        if (reason == body.end() || !reason->is_string() || Ambrose::Trim(reason->get_ref<std::string const&>()).empty())
        {
            fields.emplace_back("reason", "Say why the setting changes, so its history can tell whoever reads it later");
            return std::nullopt;
        }
        std::string_view const text = Ambrose::Trim(reason->get_ref<std::string const&>());
        if (text.size() > Settings::MaxReasonBytes)
        {
            fields.emplace_back("reason", fmt::format("A reason is at most {} bytes", Settings::MaxReasonBytes));
            return std::nullopt;
        }
        return std::string(text);
    }

    nlohmann::json ProblemJson(Settings const& settings, SettingProblem const& problem)
    {
        nlohmann::json error;
        error["key"] = problem.Key;
        error["code"] = Settings::ResultCode(problem.Result);
        error["message"] = problem.Message;
        if (problem.Result == SettingResult::Locked)
            if (std::optional<SettingView> const view = settings.Describe(problem.Key))
            {
                error["layer"] = Settings::LayerCode(view->Layer);
                error["origin"] = view->Origin;
            }
        return error;
    }

    AdminResponse Refusal(std::string message, std::vector<std::pair<std::string, std::string>> fields, nlohmann::json errors)
    {
        AdminResponse response = AdminResponse::Invalid(std::move(message), std::move(fields));
        nlohmann::json body = nlohmann::json::parse(response.Body);
        body["errors"] = std::move(errors);
        response.Body = body.dump();
        return response;
    }

    AdminResponse Locked(Settings const& settings, SettingProblem const& problem)
    {
        nlohmann::json body;
        body["error"] = "setting_locked";
        body["message"] = problem.Message;
        body["key"] = problem.Key;
        body["layer"] = nullptr;
        body["origin"] = nullptr;
        if (std::optional<SettingView> const view = settings.Describe(problem.Key))
        {
            body["layer"] = Settings::LayerCode(view->Layer);
            body["origin"] = view->Origin;
        }
        return AdminResponse::Json(409, body.dump());
    }

    AdminResponse Forbidden(std::string_view key)
    {
        return AdminResponse::Problem(403, "forbidden",
            fmt::format("{} is restricted, because a wrong value stops the app or locks players out, and this account is not allowed to {}", key, AdminSettingsView::RestrictedPermission));
    }

    AdminResponse Answered(SettingResult result, std::string const& message)
    {
        switch (result)
        {
            case SettingResult::UnknownKey:
                return AdminResponse::Problem(404, "setting_unknown", message);
            case SettingResult::NotStarted:
                return AdminResponse::Problem(503, "settings_not_open", message);
            case SettingResult::StoreFailed:
                return AdminResponse::Problem(503, "settings_store_failed", message);
            default:
                break;
        }
        return AdminResponse::Problem(AdminSettingsView::StatusOf(result), std::string(Settings::ResultCode(result)), message);
    }
}

int AdminSettingsView::StatusOf(SettingResult result) noexcept
{
    switch (result)
    {
        case SettingResult::Ok:
        case SettingResult::Unchanged:
            return 200;
        case SettingResult::UnknownKey:
            return 404;
        case SettingResult::Locked:
            return 409;
        case SettingResult::WrongType:
        case SettingResult::OutOfBounds:
        case SettingResult::Invalid:
        case SettingResult::Duplicate:
            return 422;
        case SettingResult::NotStarted:
        case SettingResult::StoreFailed:
            return 503;
    }
    return 422;
}

SettingAuthor AdminSettingsView::AuthorOf(AdminRequest const& request)
{
    SettingAuthor author;
    bool const relayed = request.Principal == "token" && !request.ActorName.empty();
    if (relayed)
    {
        author.Who = request.ActorName;
        author.Source = "panel";
        return author;
    }
    author.Who = request.Principal.empty() ? std::string("the admin API") : request.Principal;
    author.Source = "admin api";
    return author;
}

AdminResponse AdminSettingsView::Put(AdminRouter const& router, Settings& settings, AdminRequest const& request, std::string_view key)
{
    std::optional<SettingView> const view = settings.Describe(key);
    if (!view)
        return AdminResponse::Problem(404, "setting_unknown", fmt::format("No setting is named {}", Ambrose::ForLog(key, 128)));
    if (view->Declaration.Edit == SettingEditClass::Restricted && !router.Permits(request, RestrictedPermission))
        return Forbidden(key);

    nlohmann::json const body = request.Body.empty() ? nlohmann::json() : nlohmann::json::parse(request.Body, nullptr, false);
    if (!body.is_object())
        return AdminResponse::Invalid("Changing a setting takes a JSON object with a value and a reason",
            { { "value", "Give the new value" }, { "reason", "Say why the setting changes" } });
    std::vector<std::pair<std::string, std::string>> fields;
    for (auto const& [name, unused] : body.items())
        if (name != "value" && name != "reason")
            fields.emplace_back(name, "A change takes only value and reason");
    std::optional<std::string> value;
    if (auto const given = body.find("value"); given == body.end())
        fields.emplace_back("value", "Give the new value");
    else if (!(value = ValueText(*given)))
        fields.emplace_back("value", "Give the value as text, a number, or true or false");
    std::optional<std::string> const reason = ReasonText(body, fields);

    nlohmann::json errors = nlohmann::json::array();
    if (value)
    {
        SettingEntry const entry{ std::string(key), *value };
        std::vector<SettingProblem> const problems = settings.Validate(std::span<SettingEntry const>(&entry, 1));
        for (SettingProblem const& problem : problems)
        {
            if (problem.Result == SettingResult::Locked)
                return Locked(settings, problem);
            if (problem.Result == SettingResult::NotStarted || problem.Result == SettingResult::UnknownKey)
                return Answered(problem.Result, problem.Message);
        }
        for (SettingProblem const& problem : problems)
        {
            if (fields.empty() || fields.back().first != "value")
                fields.emplace_back("value", problem.Message);
            errors.push_back(ProblemJson(settings, problem));
        }
    }
    if (!fields.empty())
        return Refusal(fmt::format("{} was not changed", key), std::move(fields), std::move(errors));

    SettingOutcome const outcome = settings.Set(key, *value, AuthorOf(request), *reason);
    if (outcome.Result == SettingResult::Locked)
        return Locked(settings, { std::string(key), outcome.Result, outcome.Message });
    if (outcome.Result == SettingResult::WrongType || outcome.Result == SettingResult::OutOfBounds || outcome.Result == SettingResult::Invalid)
        return Refusal(fmt::format("{} was not changed", key), { { "value", outcome.Message } },
            nlohmann::json::array({ ProblemJson(settings, { std::string(key), outcome.Result, outcome.Message }) }));
    if (!outcome.Ok() && outcome.Result != SettingResult::Unchanged)
        return Answered(outcome.Result, outcome.Message);

    std::optional<SettingView> const after = settings.Describe(key);
    nlohmann::json answer;
    answer["schema"] = SchemaVersion;
    answer["key"] = std::string(key);
    answer["changed"] = outcome.Result == SettingResult::Ok;
    answer["value"] = after ? settings.Shown(key, after->Value) : std::string();
    answer["layer"] = after ? Settings::LayerCode(after->Layer) : std::string_view("live");
    answer["apply"] = after ? std::string(Settings::ApplyName(after->Declaration.Apply)) : std::string();
    answer["restart_reason"] = after && after->Declaration.Apply == SettingApply::Restart ? nlohmann::json(after->Declaration.RestartReason) : nlohmann::json(nullptr);
    answer["message"] = outcome.Message;
    return AdminResponse::Json(200, answer.dump());
}

AdminResponse AdminSettingsView::Batch(AdminRouter const& router, Settings& settings, AdminRequest const& request)
{
    nlohmann::json const body = request.Body.empty() ? nlohmann::json() : nlohmann::json::parse(request.Body, nullptr, false);
    if (!body.is_object())
        return AdminResponse::Invalid("A batch takes a JSON object with entries and a reason",
            { { "entries", "List the settings to change, each with its key and value" }, { "reason", "Say why the settings change" } });
    std::vector<std::pair<std::string, std::string>> fields;
    for (auto const& [name, unused] : body.items())
        if (name != "entries" && name != "reason")
            fields.emplace_back(name, "A batch takes only entries and reason");
    std::optional<std::string> const reason = ReasonText(body, fields);

    std::size_t const most = std::max<std::size_t>(settings.GetDeclarations().size(), 1);
    std::vector<SettingEntry> entries;
    nlohmann::json errors = nlohmann::json::array();
    auto const given = body.find("entries");
    if (given == body.end() || !given->is_array() || given->empty())
        fields.emplace_back("entries", "List the settings to change, each with its key and value");
    else if (given->size() > most)
        fields.emplace_back("entries", fmt::format("A batch changes at most the {} settings this app declares", most));
    else
    {
        for (nlohmann::json const& item : *given)
        {
            auto const key = item.is_object() ? item.find("key") : item.end();
            auto const value = item.is_object() ? item.find("value") : item.end();
            std::optional<std::string> const text = item.is_object() && value != item.end() ? ValueText(*value) : std::nullopt;
            bool const extra = item.is_object() && item.size() != 2;
            if (!item.is_object() || key == item.end() || !key->is_string() || !text || extra)
            {
                std::string const named = item.is_object() && key != item.end() && key->is_string() ? key->get<std::string>() : fmt::format("entries[{}]", entries.size() + errors.size());
                fields.emplace_back(named, "Each entry takes only a key and a value given as text, a number, or true or false");
                errors.push_back({ { "key", named }, { "code", "invalid" }, { "message", fields.back().second } });
                continue;
            }
            entries.emplace_back(key->get<std::string>(), *text);
        }
    }

    for (SettingEntry const& entry : entries)
        if (std::optional<SettingView> const view = settings.Describe(entry.first); view && view->Declaration.Edit == SettingEditClass::Restricted && !router.Permits(request, RestrictedPermission))
        {
            std::string const message = fmt::format("{} is restricted, and this account is not allowed to {}", entry.first, RestrictedPermission);
            fields.emplace_back(entry.first, message);
            errors.push_back({ { "key", entry.first }, { "code", "restricted" }, { "message", message } });
        }
    for (SettingProblem const& problem : settings.Validate(entries))
    {
        if (problem.Result == SettingResult::NotStarted)
            return Answered(problem.Result, problem.Message);
        fields.emplace_back(problem.Key, problem.Message);
        errors.push_back(ProblemJson(settings, problem));
    }
    if (!fields.empty())
        return Refusal(fmt::format("None of the {} settings changed", entries.size()), std::move(fields), std::move(errors));

    SettingBatchOutcome const outcome = settings.SetMany(entries, AuthorOf(request), *reason);
    if (!outcome.Problems.empty())
    {
        for (SettingProblem const& problem : outcome.Problems)
        {
            fields.emplace_back(problem.Key, problem.Message);
            errors.push_back(ProblemJson(settings, problem));
        }
        return Refusal(outcome.Message, std::move(fields), std::move(errors));
    }
    if (!outcome.Ok())
        return Answered(outcome.Result, outcome.Message);

    nlohmann::json changed = nlohmann::json::array();
    for (SettingChange const& change : outcome.Changes)
        changed.push_back({ { "key", change.Key }, { "old", settings.Shown(change.Key, change.OldValue) }, { "new", settings.Shown(change.Key, change.NewValue) } });
    nlohmann::json answer;
    answer["schema"] = SchemaVersion;
    answer["changed"] = std::move(changed);
    answer["unchanged"] = outcome.Unchanged;
    answer["message"] = outcome.Message;
    return AdminResponse::Json(200, answer.dump());
}

AdminResponse AdminSettingsView::History(Settings const& settings, std::string_view key)
{
    std::optional<SettingView> const view = settings.Describe(key);
    if (!view)
        return AdminResponse::Problem(404, "setting_unknown", fmt::format("No setting is named {}", Ambrose::ForLog(key, 128)));
    std::vector<SettingAuditEntry> entries;
    std::string error;
    if (!settings.History(key, entries, error))
        return AdminResponse::Problem(503, "settings_not_open", error);
    nlohmann::json rows = nlohmann::json::array();
    for (SettingAuditEntry const& entry : entries)
        rows.push_back({ { "id", entry.Id }, { "old", settings.Shown(key, entry.OldValue) }, { "new", settings.Shown(key, entry.NewValue) }, { "who", entry.Who },
            { "account_id", entry.AccountId }, { "source", entry.Source }, { "reason", entry.Reason }, { "epoch_seconds", entry.EpochSeconds } });
    nlohmann::json answer;
    answer["schema"] = SchemaVersion;
    answer["key"] = std::string(key);
    answer["visibility"] = Settings::VisibilityName(view->Declaration.Visibility);
    answer["entries"] = std::move(rows);
    return AdminResponse::Json(200, answer.dump());
}

void AdminSettingsView::Register(AdminRouter& router, Settings& settings)
{
    router.AddGuardedPrefix("PUT", std::string(SettingsPrefix), "settings.edit", [&router, &settings](AdminRequest const& request)
    {
        std::string_view key(request.Path);
        key.remove_prefix(SettingsPrefix.size());
        if (key.empty() || key.find('/') != std::string_view::npos)
            return AdminResponse::Problem(404, "not_found", "Change a setting at PUT /api/settings/<key>");
        return Put(router, settings, request, key);
    });
    router.AddGuardedPrefix("GET", std::string(SettingsPrefix), "settings.read", [&settings](AdminRequest const& request)
    {
        std::string_view key(request.Path);
        key.remove_prefix(SettingsPrefix.size());
        if (!key.ends_with(HistorySuffix) || key.size() == HistorySuffix.size())
            return AdminResponse::Problem(404, "not_found", "Read a setting's history at GET /api/settings/<key>/history");
        key.remove_suffix(HistorySuffix.size());
        return History(settings, key);
    });
    router.AddCosting("POST", "/api/settings/batch", "settings.edit", BatchCost, [&router, &settings](AdminRequest const& request)
    {
        return Batch(router, settings, request);
    });
}
