/*
 * Project Ambrose by Imjustchico
 * The installation maintenance state 17.64's panel banner and control read and write: the record the panel keeps of whether maintenance is on, who turned it on, why, when it started and the optional window the public status page (17.70) will publish, entered and left through the panel's audited endpoints, so the banner, the audit rows and the future public page all read the same record.
 */

import { request } from "./api.svelte";
import { MaintenanceAnswer, type MaintenanceState } from "./schemas";

export function getMaintenance(signal?: AbortSignal): Promise<MaintenanceState> {
    return request("GET", "/api/panel/maintenance", MaintenanceAnswer, undefined, signal);
}

export function enterMaintenance(
    reason: string,
    windowStart: number | null,
    windowEnd: number | null,
    signal?: AbortSignal,
): Promise<MaintenanceState> {
    return request(
        "POST",
        "/api/panel/maintenance/enter",
        MaintenanceAnswer,
        { reason, window_start_epoch_ms: windowStart, window_end_epoch_ms: windowEnd },
        signal,
    );
}

export function exitMaintenance(signal?: AbortSignal): Promise<MaintenanceState> {
    return request("POST", "/api/panel/maintenance/exit", MaintenanceAnswer, {}, signal);
}

const instantFormat = new Intl.DateTimeFormat(undefined, {
    dateStyle: "medium",
    timeStyle: "short",
});

export function formatInstant(epochMs: number): string {
    return instantFormat.format(epochMs);
}

export function describeWindow(state: MaintenanceState): string | null {
    if (state.window_start_epoch_ms == null || state.window_end_epoch_ms == null) return null;
    return `${formatInstant(state.window_start_epoch_ms)} – ${formatInstant(state.window_end_epoch_ms)}`;
}
