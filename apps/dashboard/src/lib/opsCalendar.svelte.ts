/*
 * Project Ambrose by Imjustchico
 * The operations calendar 17.96 reads from: the calendar answer the panel serves for a range, the operator's own time zone with the node's zone named where it differs, week and month grids built from epoch milliseconds, and the conflict reasons naming the entries that overlap in a way that matters, so the page draws every planned thing where it belongs without editing any of them.
 */

import { request } from "./api.svelte";
import { OpsCalendarAnswer, type OpsCalendarConflict, type OpsCalendarEvent } from "./schemas";

export type { OpsCalendarConflict, OpsCalendarEvent };
export type { OpsCalendarAnswer };

export function getCalendar(fromMs: number, toMs: number, signal?: AbortSignal): Promise<OpsCalendarAnswer> {
    return request(
        "GET",
        `/api/ops/calendar?from=${fromMs}&to=${toMs}`,
        OpsCalendarAnswer,
        undefined,
        signal,
    );
}

export function operatorTimeZone(): string {
    return Intl.DateTimeFormat().resolvedOptions().timeZone;
}

function zoneOffsetMs(epochMs: number, timeZone: string): number {
    const parts = new Intl.DateTimeFormat("en-US", {
        timeZone,
        year: "numeric",
        month: "2-digit",
        day: "2-digit",
        hour: "2-digit",
        minute: "2-digit",
        second: "2-digit",
        hour12: false,
    }).formatToParts(epochMs);
    const get = (type: string) => parts.find((p) => p.type === type)?.value ?? "00";
    const hour = get("hour") === "24" ? "00" : get("hour");
    const asUtc = Date.UTC(
        Number(get("year")),
        Number(get("month")) - 1,
        Number(get("day")),
        Number(hour),
        Number(get("minute")),
        Number(get("second")),
    );
    return asUtc - epochMs;
}

export function startOfDayMs(epochMs: number, timeZone: string): number {
    const parts = new Intl.DateTimeFormat("en-US", {
        timeZone,
        year: "numeric",
        month: "2-digit",
        day: "2-digit",
    }).formatToParts(epochMs);
    const get = (type: string) => parts.find((p) => p.type === type)?.value ?? "01";
    const midnightUtc = Date.UTC(Number(get("year")), Number(get("month")) - 1, Number(get("day")));
    return midnightUtc - zoneOffsetMs(midnightUtc, timeZone);
}

export function startOfWeekMs(epochMs: number, timeZone: string): number {
    const day = startOfDayMs(epochMs, timeZone);
    const weekday = new Intl.DateTimeFormat("en-US", { timeZone, weekday: "short" }).format(day);
    const mondayBased = (["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"].indexOf(weekday) + 6) % 7;
    return day - mondayBased * 24 * 60 * 60 * 1000;
}

export function startOfMonthMs(epochMs: number, timeZone: string): number {
    const parts = new Intl.DateTimeFormat("en-US", {
        timeZone,
        year: "numeric",
        month: "2-digit",
    }).formatToParts(epochMs);
    const get = (type: string) => parts.find((p) => p.type === type)?.value ?? "01";
    const midnightUtc = Date.UTC(Number(get("year")), Number(get("month")) - 1, 1);
    return midnightUtc - zoneOffsetMs(midnightUtc, timeZone);
}

export function addDaysMs(epochMs: number, days: number): number {
    return epochMs + days * 24 * 60 * 60 * 1000;
}

const dayFormat = new Intl.DateTimeFormat(undefined, { weekday: "short", month: "numeric", day: "numeric" });
const timeFormat = new Intl.DateTimeFormat(undefined, { hour: "numeric", minute: "2-digit" });

export function formatDay(epochMs: number): string {
    return dayFormat.format(epochMs);
}

export function formatTime(epochMs: number): string {
    return timeFormat.format(epochMs);
}

export function formatRange(event: OpsCalendarEvent): string {
    if (event.end_epoch_ms === 0) return `${formatTime(event.start_epoch_ms)} – ongoing`;
    return `${formatTime(event.start_epoch_ms)} – ${formatTime(event.end_epoch_ms)}`;
}

export function conflictReasonFor(eventId: string, conflicts: OpsCalendarConflict[]): string | null {
    for (const conflict of conflicts) {
        if (conflict.event_a_id === eventId || conflict.event_b_id === eventId) return conflict.reason;
    }
    return null;
}

export function sourceLabel(source: string): string {
    switch (source) {
        case "schedules": return "Schedules";
        case "game_events": return "Game events";
        case "installation_maintenance": return "Installation maintenance";
        case "realm_maintenance": return "Realm maintenance";
        default: return source;
    }
}
