/*
 * Project Ambrose by Imjustchico
 * What the panel asks about one app: running a command on it, the supervisor's own routes for power, captured output and the file roots with their listings, reads and protected patterns, and the app's own routes for its status, settings with their changes, resets, batches and their previews, history and reveals, the events it announces, and databases, sent through the supervisor's relay when the supervisor served this panel and straight to the app when the app served it itself, so every page reads the same way whichever is in front of it. A page about something only some apps keep, such as the realm list the login server holds, asks each app once and offers only those that answer, because an app answering that it has no such page is not an app with an empty one; an app run without an admin API has no pages at all and is never offered, while one that is only stopped still is, so its page can say so.
 */

import { ApiError, request } from "./api.svelte";
import { filesQuery } from "./files";
import { live } from "./status.svelte";
import {
    CommandAnswer,
    CommandHistoryAnswer,
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
    SettingBatchAnswer,
    SettingChangeAnswer,
    SettingHistoryAnswer,
    EventsAnswer,
    PanelSettingsAnswer,
    FileContent,
    FileListing,
    FileRootsAnswer,
    FileRulesAnswer,
    TickProfileAnswer,
    TickProfileTraceAnswer,
    ErrorsAnswer,
    ErrorClearAnswer,
    ErrorReportAnswer,
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

export function candidates(): string[] {
    if (!supervisorServes()) return [servedBy()];
    return [
        servedBy(),
        ...supervised()
            .filter((app) => app.supervision?.admin.enabled !== false)
            .map((app) => app.name),
    ];
}

export async function servingApps(path: string, signal?: AbortSignal): Promise<string[]> {
    const found = await Promise.all(
        candidates().map(async (name) => {
            try {
                await request("GET", pathFor(name, path), null, undefined, signal);
                return name;
            } catch (problem) {
                if (problem instanceof DOMException && problem.name === "AbortError") throw problem;
                if (problem instanceof ApiError && (problem.status === 404 || problem.code === "app_admin_off")) return null;
                return name;
            }
        }),
    );
    return found.filter((name): name is string => name !== null);
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

export function commandHistory(app: string, signal?: AbortSignal) {
    return request("GET", `api/panel/apps/${encodeURIComponent(app)}/command-history`, CommandHistoryAnswer, undefined, signal);
}

export function settingsOf(app: string, signal?: AbortSignal, reveal?: string) {
    return request(
        "GET",
        pathFor(app, reveal ? `settings?reveal=${encodeURIComponent(reveal)}` : "settings"),
        SettingsAnswer,
        undefined,
        signal,
    );
}

export type SettingValue = string | number | boolean;

export function changeSetting(app: string, key: string, value: SettingValue, reason: string) {
    return request("PUT", pathFor(app, `settings/${encodeURIComponent(key)}`), SettingChangeAnswer, { value, reason });
}

export function changeSettings(app: string, entries: { key: string; value: SettingValue }[], reason: string) {
    return request("POST", pathFor(app, "settings/batch"), SettingBatchAnswer, { entries, reason });
}

export function resetSetting(app: string, key: string, reason: string) {
    return request("DELETE", pathFor(app, `settings/${encodeURIComponent(key)}`), SettingChangeAnswer, { reason });
}

export function previewSettings(app: string, entries: { key: string; value: SettingValue }[]) {
    return request("POST", pathFor(app, "settings/batch"), SettingBatchAnswer, { entries, dry_run: true });
}

export function eventsAfter(app: string, after: number, signal?: AbortSignal) {
    return request("GET", pathFor(app, `events/after/${after}`), EventsAnswer, undefined, signal);
}

export function settingHistory(app: string, key: string, signal?: AbortSignal) {
    return request("GET", pathFor(app, `settings/${encodeURIComponent(key)}/history`), SettingHistoryAnswer, undefined, signal);
}

export function panelSettings(signal?: AbortSignal) {
    return request("GET", "api/panel/settings", PanelSettingsAnswer, undefined, signal);
}

export function updatePanelSettings(values: Record<string, string>) {
    return request("PATCH", "api/panel/settings", PanelSettingsAnswer, { values });
}

export function errors(signal?: AbortSignal) {
    return request("GET", "api/panel/errors", ErrorsAnswer, undefined, signal);
}

export function clearErrorGroup(id: number) {
    return request("POST", "api/panel/errors/clear", ErrorClearAnswer, { id });
}

export function previewErrorReport(groups: number[], includeRendered: boolean, signal?: AbortSignal) {
    return request("POST", "api/panel/errors/report/preview", ErrorReportAnswer, { groups, include_rendered: includeRendered }, signal);
}

export function createErrorReport(groups: number[], includeRendered: boolean) {
    return request("POST", "api/panel/errors/report", ErrorReportAnswer, { groups, include_rendered: includeRendered });
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

export function startTickProfile(app: string, seconds: number) {
    return request("POST", pathFor(app, "tick-profile"), TickProfileAnswer, { seconds });
}

export function tickProfileOf(app: string, signal?: AbortSignal) {
    return request("GET", pathFor(app, "tick-profile"), TickProfileAnswer, undefined, signal);
}

export function tickProfileTraceOf(app: string) {
    return request("GET", pathFor(app, "tick-profile/trace"), TickProfileTraceAnswer);
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

export type FileListQuery = { path: string; q?: string; sort?: string; order?: string; offset?: number; limit?: number };

export function fileRoots(signal?: AbortSignal) {
    return request("GET", "api/files", FileRootsAnswer, undefined, signal);
}

export function listFiles(root: string, query: FileListQuery, signal?: AbortSignal) {
    const asked = filesQuery({
        path: query.path,
        q: query.q,
        sort: query.sort,
        order: query.order,
        offset: query.offset,
        limit: query.limit,
    });
    return request("GET", `api/files/${encodeURIComponent(root)}/list${asked === "" ? "" : `?${asked}`}`, FileListing, undefined, signal);
}

export function readFile(root: string, path: string, options: { offset?: number; reveal?: boolean } = {}, signal?: AbortSignal) {
    const asked = filesQuery({ path, offset: options.offset, reveal: options.reveal });
    return request("GET", `api/files/${encodeURIComponent(root)}/content?${asked}`, FileContent, undefined, signal);
}

export function fileRules(root: string, signal?: AbortSignal) {
    return request("GET", `api/files/${encodeURIComponent(root)}/rules`, FileRulesAnswer, undefined, signal);
}

export function setFileRules(root: string, patterns: string[], reason: string) {
    return request(
        "PUT",
        `api/files/${encodeURIComponent(root)}/rules`,
        FileRulesAnswer,
        reason === "" ? { patterns } : { patterns, reason },
    );
}
