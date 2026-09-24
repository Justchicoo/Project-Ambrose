/*
 * Project Ambrose by Imjustchico
 * The live picture of the app that served the panel: its entry in the app list and its capabilities, read when watching starts and, when the supervisor served the panel, the whole app list read again every second because it carries every app's state, its status read every second into the latest sample and the time it arrived, fifteen-minute typed-array rings of tick times and sessions for the charts, and the connection in the words doc/DESIGN.md sets, retrying with a growing delay and a countdown when answers stop, and stopping when the session ends. Watching starts untracked, so a caller starting it from inside an effect does not take whatever the first read touches as a dependency and then cancel its own request when that state changes.
 */

import { untrack } from "svelte";
import { ApiError, request } from "./api.svelte";
import {
    AppList,
    Capabilities,
    Status,
    type AppEntry,
    type Capabilities as CapabilitiesShape,
    type Status as StatusShape,
} from "./schemas";

export const IntervalMs = 1000;
export const FreshnessMs = 3 * IntervalMs;
export const WindowSeconds = 15 * 60;
export const RetryDelaysMs = [1000, 2000, 4000, 8000, 10000] as const;

export type Connection = "live" | "reconnecting" | "disconnected";

export class Ring {
    readonly capacity: number;
    private readonly times: Float64Array;
    private readonly values: Float64Array;
    private start = 0;
    private size = 0;

    constructor(capacity: number) {
        this.capacity = capacity;
        this.times = new Float64Array(capacity);
        this.values = new Float64Array(capacity);
    }

    push(time: number, value: number | null) {
        const at = (this.start + this.size) % this.capacity;
        this.times[at] = time;
        this.values[at] = value ?? Number.NaN;
        if (this.size < this.capacity) this.size += 1;
        else this.start = (this.start + 1) % this.capacity;
    }

    read(): { times: number[]; values: (number | null)[] } {
        const times: number[] = [];
        const values: (number | null)[] = [];
        for (let index = 0; index < this.size; index += 1) {
            const at = (this.start + index) % this.capacity;
            times.push(this.times[at]);
            values.push(Number.isNaN(this.values[at]) ? null : this.values[at]);
        }
        return { times, values };
    }

    get length() {
        return this.size;
    }

    clear() {
        this.start = 0;
        this.size = 0;
    }
}

export function retryDelay(attempt: number): number {
    return RetryDelaysMs[Math.min(Math.max(attempt, 1), RetryDelaysMs.length) - 1];
}

export function isStale(receivedAt: number, now: number): boolean {
    return receivedAt === 0 || now - receivedAt > FreshnessMs;
}

export const live = $state({
    apps: [] as AppEntry[],
    capabilities: null as CapabilitiesShape | null,
    status: null as StatusShape | null,
    receivedAt: 0,
    connection: "live" as Connection,
    attempt: 0,
    retryAt: 0,
    error: null as ApiError | null,
    now: Date.now(),
    drawn: 0,
});

export const history = {
    tickAverage: new Ring(WindowSeconds),
    tickMax: new Ring(WindowSeconds),
    sessions: new Ring(WindowSeconds),
};

let timer: ReturnType<typeof setTimeout> | undefined;
let clock: ReturnType<typeof setInterval> | undefined;
let inFlight: AbortController | null = null;
let watching = false;

function schedule(delay: number) {
    clearTimeout(timer);
    timer = setTimeout(() => void poll(), delay);
}

function describe(failure: unknown): ApiError {
    if (failure instanceof DOMException && failure.name === "AbortError")
        return new ApiError(0, "timed_out", `The server did not answer within ${(3 * IntervalMs) / 1000} seconds`, "");
    const said = failure instanceof Error ? `${failure.name}: ${failure.message}` : String(failure);
    return new ApiError(0, "poll_failed", `Reading the server's state failed with ${said}`, "");
}

async function poll() {
    if (!watching || inFlight) return;
    const controller = new AbortController();
    inFlight = controller;
    const deadline = setTimeout(() => controller.abort(), 3 * IntervalMs);
    try {
        if (live.apps.length === 0 || live.apps.some((entry) => entry.supervision != null))
            live.apps = await request("GET", "api/apps", AppList, undefined, controller.signal);
        if (live.capabilities === null)
            live.capabilities = await request("GET", "api/capabilities", Capabilities, undefined, controller.signal);
        const status = await request("GET", "api/status", Status, undefined, controller.signal);
        const arrived = Date.now();
        live.status = status;
        live.receivedAt = arrived;
        live.connection = "live";
        live.attempt = 0;
        live.error = null;
        history.tickAverage.push(arrived / 1000, status.tick?.average_ms ?? null);
        history.tickMax.push(arrived / 1000, status.tick?.max_ms ?? null);
        history.sessions.push(arrived / 1000, status.sessions);
        live.drawn += 1;
        schedule(IntervalMs);
    } catch (failure) {
        if (!watching) return;
        const error = failure instanceof ApiError ? failure : describe(failure);
        live.error = error;
        if (error.status === 401) {
            live.connection = "disconnected";
            stop();
            return;
        }
        live.attempt += 1;
        live.connection = "reconnecting";
        const delay = retryDelay(live.attempt);
        live.retryAt = Date.now() + delay;
        schedule(delay);
    } finally {
        clearTimeout(deadline);
        if (inFlight === controller) inFlight = null;
    }
}

export function watch() {
    if (watching) return;
    watching = true;
    live.connection = "live";
    clock = setInterval(() => (live.now = Date.now()), 1000);
    untrack(() => void poll());
}

export function stop() {
    watching = false;
    clearTimeout(timer);
    clearInterval(clock);
    inFlight?.abort();
    inFlight = null;
}

export function retryNow() {
    if (!watching) return;
    clearTimeout(timer);
    void poll();
}
