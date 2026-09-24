/*
 * Project Ambrose by Imjustchico
 * What the panel asks about one app: running a command on it, the supervisor's own routes for power and captured output, and the app's own routes for its status, settings and databases, sent through the supervisor's relay when the supervisor served this panel and straight to the app when the app served it itself, so every page reads the same way whichever is in front of it.
 */

import { request } from "./api.svelte";
import { live } from "./status.svelte";
import {
    CommandAnswer,
    LogAnswer,
    DatabaseAnswer,
    DatabaseApplyAnswer,
    DatabaseUpdatesAnswer,
    OutputAnswer,
    ActivityAnswer,
    ClientAnswer,
    GraphRangeAnswer,
    GraphsAnswer,
    MetricsAnswer,
    PlayersAnswer,
    RealmsAnswer,
    PowerAnswer,
    ReloadAnswer,
    ReloadRunAnswer,
    SettingsAnswer,
    type AppEntry,
} from "./schemas";

export type PowerAction = "start" | "stop" | "restart" | "kill";
export type OutputRun = "current" | "previous";

export function servedBy(): string {
    return live.status?.app ?? "";
}

export function supervisorServes(): boolean {
    return live.apps.some((app) => app.supervision != null);
}

export function supervised(): AppEntry[] {
    return live.apps.filter((app) => app.supervision != null);
}

export function appNamed(name: string): AppEntry | undefined {
    return live.apps.find((app) => app.name === name);
}

export function pathFor(app: string, path: string): string {
    return app === servedBy() ? `api/${path}` : `api/apps/${app}/api/${path}`;
}

export function power(app: string, action: PowerAction, seconds = 0) {
    return request("POST", `api/apps/${app}/power`, PowerAnswer, { action, seconds });
}

export function output(app: string, run: OutputRun, signal?: AbortSignal) {
    return request("GET", `api/apps/${app}/output/${run}`, OutputAnswer, undefined, signal);
}

export function logsAfter(app: string, after: number, signal?: AbortSignal) {
    return request("GET", pathFor(app, `logs/after/${after}`), LogAnswer, undefined, signal);
}

export function runCommand(app: string, command: string, confirm = false) {
    return request("POST", pathFor(app, "command"), CommandAnswer, confirm ? { command, confirm } : { command });
}

export function settingsOf(app: string, signal?: AbortSignal) {
    return request("GET", pathFor(app, "settings"), SettingsAnswer, undefined, signal);
}

export function clientDataOf(app: string, signal?: AbortSignal) {
    return request("GET", pathFor(app, "client"), ClientAnswer, undefined, signal);
}

export function realmsOf(app: string, signal?: AbortSignal) {
    return request("GET", pathFor(app, "realms"), RealmsAnswer, undefined, signal);
}

export function activityOf(app: string, signal?: AbortSignal) {
    return request("GET", pathFor(app, "activity"), ActivityAnswer, undefined, signal);
}

export function playersOf(app: string, signal?: AbortSignal) {
    return request("GET", pathFor(app, "players"), PlayersAnswer, undefined, signal);
}

export function graphs(signal?: AbortSignal) {
    return request("GET", "api/graphs", GraphsAnswer, undefined, signal);
}

export function graphRange(subject: string, series: string, fromEpochMs: number, toEpochMs: number, points: number, signal?: AbortSignal) {
    const query = [
        `subject=${encodeURIComponent(subject)}`,
        `series=${encodeURIComponent(series)}`,
        `from=${Math.round(fromEpochMs)}`,
        `to=${Math.round(toEpochMs)}`,
        `points=${Math.round(points)}`,
    ].join("&");
    return request("GET", `api/graphs/range?${query}`, GraphRangeAnswer, undefined, signal);
}

export function metricsOf(app: string, signal?: AbortSignal) {
    return request("GET", pathFor(app, "metrics"), MetricsAnswer, undefined, signal);
}

export function reloadTargetsOf(app: string, signal?: AbortSignal) {
    return request("GET", pathFor(app, "reload"), ReloadAnswer, undefined, signal);
}

export function runReload(app: string, target: string) {
    return request("POST", pathFor(app, `reload/${encodeURIComponent(target)}`), ReloadRunAnswer);
}

export function databasesOf(app: string, signal?: AbortSignal) {
    return request("GET", pathFor(app, "database"), DatabaseAnswer, undefined, signal);
}

export function updatesOf(app: string, signal?: AbortSignal) {
    return request("GET", pathFor(app, "database/updates"), DatabaseUpdatesAnswer, undefined, signal);
}

export function applyData(app: string, database: string) {
    return request("POST", pathFor(app, "database/apply"), DatabaseApplyAnswer, { database });
}
