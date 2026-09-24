/*
 * Project Ambrose by Imjustchico
 * What a server counts, answered twice from the one register: GET /metrics in the Prometheus text exposition format for a scraper, which asks at that fixed path and reads nothing else, and GET /api/metrics as JSON for the panel, which reads every page the same typed way. Both carry the live values the status register publishes as well, under the same prefix, so an operator graphs everything the process knows from the one place rather than only what was counted with an atomic; a published name that a metric already holds is left out, since one name cannot be two metrics. Both sit behind the same guard as the rest of the admin API, because what a server counts says how many players it holds and how hard it is working, which is not public, and neither carries a label a caller supplied, so nothing a client sends can reach the name of a metric.
 */

#ifndef AMBROSE_ADMINMETRICSVIEW_H
#define AMBROSE_ADMINMETRICSVIEW_H

#include <string>

class AdminRouter;

class AdminMetricsView
{
public:
    static constexpr int SchemaVersion = 1;

    AdminMetricsView() = delete;

    static std::string MetricsJson();

    static void Register(AdminRouter& router);
};

#endif
