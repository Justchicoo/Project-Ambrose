/*
 * Project Ambrose by Imjustchico
 * The panel's one socket client, on partysocket's reconnecting socket to the panel's own /api/panel/events with nothing in its address: it opens only while a panel session is signed in, says hello with the session's CSRF token before anything else, never queues a message while it is not open, and reconnects after anything but a malformed frame, an ended session or lost access. Every frame is read against the generated protocol and handed to the handler the table holds for its type, so a type the server can send with no handler here is a compile error and a failing contract test. Ready brings the permission snapshot and this run's instance, which throws away the sequences of an earlier run, and then resumes every followed stream after the last sequence it saw, while records it has already seen are skipped. A pong answers the client's own ping every twenty seconds and its absence reconnects, an error settles the request it names or is kept for the page, a dropped frame is kept as a gap for its stream, a status record updates the matching app in the live picture, and 4401 ends the session.
 */

import ReconnectingWebSocket, { type CloseEvent } from "partysocket/ws";
import { untrack } from "svelte";
import * as v from "valibot";
import { session } from "./api.svelte";
import {
    closeCodes,
    protocolVersion,
    sentTypes,
    serverData,
    ServerFrame,
    streamOfType,
    type ClientData,
    type ClientType,
    type ReadyData,
    type Scope,
    type SentType,
    type ServerData,
    type Stream,
} from "./protocol";
import { live } from "./status.svelte";

export const PingIntervalMs = 20_000;
export const MinReconnectMs = 1000;
export const MaxReconnectMs = 10_000;
export const MaxGaps = 50;

export type EventsState = "closed" | "connecting" | "open" | "ready";

export type Permissions = ReadyData["permissions"];

export type Gap = {
    stream: string;
    app: string | null;
    first: number;
    last: number;
    count: number;
    at: number;
};

export type Followed = {
    stream: Stream;
    app: string | null;
    seq: number;
};

export type EventsOptions = {
    WebSocket?: unknown;
    url?: () => string;
    pingIntervalMs?: number;
    minReconnectMs?: number;
    maxReconnectMs?: number;
};

export class EventError extends Error {
    readonly code: string;
    readonly request: string | null;
    readonly correlation: string;

    constructor(code: string, message: string, request: string | null, correlation: string) {
        super(message);
        this.name = "EventError";
        this.code = code;
        this.request = request;
        this.correlation = correlation;
    }
}

export const events = $state({
    state: "closed" as EventsState,
    instance: null as string | null,
    serverTime: 0,
    permissions: { panel: [], apps: {} } as Permissions,
    apps: [] as string[],
    gaps: [] as Gap[],
    error: null as EventError | null,
    closedWith: 0,
});

type Pending = {
    id: string;
    resolve: (frame: ServerFrame) => void;
    reject: (failure: EventError) => void;
};

const finalCodes = new Set<number>([closeCodes.malformed, closeCodes.session_ended, closeCodes.access_lost]);
const followed: Followed[] = [];
let pending: Pending[] = [];
let socket: ReconnectingWebSocket | null = null;
let heartbeat: ReturnType<typeof setInterval> | undefined;
let awaitingPong: string | null = null;
let counter = 0;

export function eventsUrl(page: string): string {
    const bare = page.replace(/[?#].*$/s, "");
    return `${bare.slice(0, bare.lastIndexOf("/") + 1).replace(/^http/, "ws")}api/panel/events`;
}

export function reconnectsAfter(event: Pick<CloseEvent, "code">): boolean {
    return !finalCodes.has(event.code);
}

export function unhandledTypes(sent: readonly string[], table: object): string[] {
    const held = table as Record<string, unknown>;
    return sent.filter((type) => !Object.hasOwn(table, type) || typeof held[type] !== "function");
}

export function readableScopes(permission: string, snapshot: Permissions): (string | null)[] {
    if (snapshot.panel.includes(permission)) return [null];
    return Object.entries(snapshot.apps)
        .filter(([, keys]) => keys.includes(permission))
        .map(([app]) => app);
}

function nextId(prefix: string): string {
    counter += 1;
    return `${prefix}-${counter}`;
}

function isOpen(): boolean {
    return socket !== null && socket.readyState === ReconnectingWebSocket.OPEN;
}

function transmit(type: ClientType, data: object, scope: Scope, seq: number | undefined, id: string): boolean {
    if (!socket || !isOpen()) return false;
    const frame: Record<string, unknown> = { v: protocolVersion, type, id, scope, data };
    if (seq !== undefined) frame.seq = seq;
    socket.send(JSON.stringify(frame));
    return true;
}

export function send<T extends ClientType>(type: T, data: ClientData<T>, scope: Scope = null): boolean {
    return events.state === "ready" && transmit(type, data, scope, undefined, nextId(type));
}

export function ask<T extends ClientType>(type: T, data: ClientData<T>, scope: Scope = null): Promise<ServerFrame> {
    const id = nextId(type);
    return new Promise((resolve, reject) => {
        if (events.state !== "ready" || !transmit(type, data, scope, undefined, id)) {
            reject(new EventError("not_connected", "The panel's live connection is not open; try again once it reconnects", id, ""));
            return;
        }
        pending.push({ id, resolve, reject });
    });
}

function take(id: string): Pending | undefined {
    const index = pending.findIndex((entry) => entry.id === id);
    return index < 0 ? undefined : pending.splice(index, 1)[0];
}

function failPending(code: string, message: string) {
    const waiting = pending;
    pending = [];
    for (const entry of waiting) entry.reject(new EventError(code, message, entry.id, ""));
}

function resume(entry: Followed) {
    transmit("resume", { stream: entry.stream }, entry.app === null ? null : { app: entry.app }, entry.seq, nextId("resume"));
}

export function follow(stream: Stream, app: string | null = null) {
    if (followed.some((entry) => entry.stream === stream && entry.app === app)) return;
    const entry: Followed = { stream, app, seq: 0 };
    followed.push(entry);
    if (events.state === "ready") resume(entry);
}

export function followedStreams(): Followed[] {
    return followed.map((entry) => ({ ...entry }));
}

function track(frame: ServerFrame): boolean {
    if (frame.seq === null) return true;
    const stream = (streamOfType as Record<string, string | undefined>)[frame.type];
    if (stream === undefined) return true;
    const app = frame.scope?.app ?? null;
    let matched = false;
    let fresh = false;
    for (const entry of followed) {
        if (entry.stream !== stream || (entry.app !== null && entry.app !== app)) continue;
        matched = true;
        if (frame.seq > entry.seq) {
            entry.seq = frame.seq;
            fresh = true;
        }
    }
    return !matched || fresh;
}

type Handler<T extends SentType> = (frame: ServerFrame, data: ServerData<T>) => void;

export type Handlers = { [T in SentType]: Handler<T> };

export const handlers: Handlers = {
    ready: (_frame, data) => {
        if (events.instance !== data.instance) {
            for (const entry of followed) entry.seq = 0;
            events.gaps = [];
        }
        events.instance = data.instance;
        events.serverTime = data.server_time;
        events.permissions = { panel: [...data.permissions.panel], apps: { ...data.permissions.apps } };
        events.apps = data.apps.map((app) => app.name);
        events.state = "ready";
        for (const entry of followed) resume(entry);
    },
    status: (_frame, data) => {
        const entry = live.apps.find((app) => app.name === data.app && app.supervision != null);
        if (!entry?.supervision) return;
        entry.supervision.state = data.state;
        entry.supervision.pid = data.pid;
        entry.supervision.crashes = data.crashes;
        entry.supervision.restart_epoch_ms = data.next_restart;
    },
    dropped: (frame, data) => {
        const app = frame.scope?.app ?? null;
        for (const entry of followed) if (entry.stream === data.stream && entry.app === app) entry.seq = Math.max(entry.seq, data.last);
        events.gaps = [
            ...events.gaps,
            { stream: data.stream, app, first: data.first, last: data.last, count: data.count, at: frame.time },
        ].slice(-MaxGaps);
    },
    error: (frame, data) => {
        const failure = new EventError(data.code, data.message, data.request, data.correlation);
        const id = data.request ?? frame.id;
        const waiting = id === null ? undefined : take(id);
        if (waiting) {
            waiting.reject(failure);
            return;
        }
        events.error = failure;
    },
    pong: (frame) => {
        if (frame.id !== null && frame.id === awaitingPong) awaitingPong = null;
    },
};

function isSent(type: string): type is SentType {
    return (sentTypes as readonly string[]).includes(type);
}

function deliver(frame: ServerFrame) {
    if (!isSent(frame.type)) return;
    const checked = v.safeParse(serverData[frame.type], frame.data);
    if (!checked.success) return;
    if (!track(frame)) return;
    (handlers[frame.type] as (frame: ServerFrame, data: unknown) => void)(frame, checked.output);
    if (frame.type === "error" || frame.id === null) return;
    take(frame.id)?.resolve(frame);
}

function received(event: MessageEvent) {
    if (typeof event.data !== "string") return;
    let parsed: unknown;
    try {
        parsed = JSON.parse(event.data);
    } catch {
        return;
    }
    const frame = v.safeParse(ServerFrame, parsed);
    if (frame.success) deliver(frame.output);
}

function opened() {
    events.state = "open";
    awaitingPong = null;
    const csrf = session.csrf;
    if (!csrf) {
        disconnectEvents();
        return;
    }
    transmit("hello", { version: protocolVersion, csrf }, null, undefined, nextId("hello"));
}

function closed(event: CloseEvent) {
    events.closedWith = event.code;
    events.state = socket !== null && socket.shouldReconnect ? "connecting" : "closed";
    awaitingPong = null;
    failPending("disconnected", "The panel's live connection closed before the answer came");
    if (event.code === closeCodes.session_ended) {
        session.state = "signed-out";
        session.csrf = null;
        session.ended = true;
    }
}

function beat() {
    if (!socket || events.state !== "ready") return;
    if (awaitingPong !== null) {
        awaitingPong = null;
        events.state = "connecting";
        socket.reconnect();
        return;
    }
    const id = nextId("ping");
    if (transmit("ping", {}, null, undefined, id)) awaitingPong = id;
}

export function connectEvents(options: EventsOptions = {}) {
    untrack(() => {
        if (socket) return;
        const url = options.url ?? (() => eventsUrl(window.location.href));
        const opening = new ReconnectingWebSocket(url, undefined, {
            WebSocket: options.WebSocket,
            startClosed: true,
            maxEnqueuedMessages: 0,
            minReconnectionDelay: options.minReconnectMs ?? MinReconnectMs,
            maxReconnectionDelay: options.maxReconnectMs ?? MaxReconnectMs,
            shouldReconnectOnClose: reconnectsAfter,
        });
        opening.onopen = opened;
        opening.onmessage = received;
        opening.onclose = closed;
        socket = opening;
        events.state = "connecting";
        events.closedWith = 0;
        events.error = null;
        heartbeat = setInterval(beat, options.pingIntervalMs ?? PingIntervalMs);
        opening.reconnect();
    });
}

export function disconnectEvents() {
    clearInterval(heartbeat);
    heartbeat = undefined;
    const closing = socket;
    socket = null;
    closing?.close(1000, "signed out");
    followed.length = 0;
    failPending("disconnected", "The panel's live connection was closed");
    awaitingPong = null;
    events.state = "closed";
    events.instance = null;
    events.permissions = { panel: [], apps: {} };
    events.apps = [];
    events.gaps = [];
}
