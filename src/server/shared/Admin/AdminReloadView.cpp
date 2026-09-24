/*
 * Project Ambrose by Imjustchico
 * Answers for the register: the listing names every target in the order they would run, with the generation each is serving and what the last attempt found, and a run answers with that target's outcome whether it worked or not, because an operator who reloads something and is told only that it failed has to go to the log to learn why, and this is the page they were already looking at.
 */

#include "AdminReloadView.h"
#include "AdminRouter.h"
#include "ReloadMgr.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    nlohmann::json OutcomeJson(std::string const& name)
    {
        nlohmann::json entry;
        entry["target"] = name;
        entry["generation"] = sReloadMgr.GetGeneration(name);
        std::optional<ReloadOutcome> const last = sReloadMgr.GetLastOutcome(name);
        entry["ran"] = last.has_value();
        entry["ok"] = last ? last->Ok : true;
        entry["errors"] = last ? last->Errors : std::vector<std::string>{};
        return entry;
    }

    nlohmann::json OutcomeJson(ReloadOutcome const& outcome)
    {
        nlohmann::json entry;
        entry["target"] = outcome.Target;
        entry["generation"] = outcome.Generation;
        entry["ran"] = true;
        entry["ok"] = outcome.Ok;
        entry["errors"] = outcome.Errors;
        return entry;
    }
}

std::string AdminReloadView::TargetsJson()
{
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    nlohmann::json targets = nlohmann::json::array();
    for (std::string const& name : sReloadMgr.GetOrderedTargets())
        targets.push_back(OutcomeJson(name));
    body["targets"] = std::move(targets);
    return body.dump();
}

void AdminReloadView::Register(AdminRouter& router)
{
    router.AddGuarded("GET", "/api/reload", "reload.read", [](AdminRequest const&)
    {
        return AdminResponse::Json(200, TargetsJson());
    });

    router.AddGuardedPrefix("POST", "/api/reload/", "reload.run", [](AdminRequest const& request)
    {
        std::string_view tail(request.Path);
        tail.remove_prefix(std::string_view("/api/reload/").size());
        if (tail.empty())
            return AdminResponse::Invalid("A reload takes the name of what to reload", { { "target", "Name a target from GET /api/reload, or all" } });

        nlohmann::json body;
        body["schema"] = SchemaVersion;
        if (tail == "all")
        {
            nlohmann::json outcomes = nlohmann::json::array();
            bool everyOne = true;
            for (ReloadOutcome const& outcome : sReloadMgr.ReloadAll())
            {
                everyOne = everyOne && outcome.Ok;
                outcomes.push_back(OutcomeJson(outcome));
            }
            body["ok"] = everyOne;
            body["targets"] = std::move(outcomes);
            return AdminResponse::Json(200, body.dump());
        }

        std::string const name(tail);
        if (!sReloadMgr.IsRegistered(name))
            return AdminResponse::Problem(404, "reload_target_unknown", "Nothing is registered by that name on this app");

        ReloadOutcome const outcome = sReloadMgr.Reload(name);
        body["ok"] = outcome.Ok;
        body["targets"] = nlohmann::json::array({ OutcomeJson(outcome) });
        return AdminResponse::Json(outcome.Ok ? 200 : 409, body.dump());
    });
}
