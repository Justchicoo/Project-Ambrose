/*
 * Project Ambrose by Imjustchico
 * Tests the panel's socket client with no browser, over a stand-in WebSocket: every type the server can send has a handler here and one added without a handler is named; the socket's address is the panel's own events path with ws or wss and nothing after it; one socket opens and its first frame is hello with the session's CSRF token; nothing is sent or queued while it is not ready; ready adopts the permissions, apps and instance and resumes every followed stream after the last sequence it saw, a new instance starting them over, while a record already seen is skipped; a status record reaches the matching app; a dropped range is kept as a gap and moves on only the scope whose session dropped it; an error settles the request it names and is otherwise kept; 4401 ends the session and nothing reconnects after 4400, 4401 or 4403, while 4429 and an abnormal close reconnect; and a ping left unanswered reconnects.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { session } from "./api.svelte";
import {
    ask,
    connectEvents,
    disconnectEvents,
    EventError,
    events,
    eventsUrl,
    follow,
    followedStreams,
    handlers,
    readableScopes,
    reconnectsAfter,
    send,
    unhandledTypes,
} from "./events.svelte";
import { closeCodes, sentTypes, serverTypes } from "./protocol";
import type { AppEntry } from "./schemas";
import { live } from "./status.svelte";

const Address = "ws://127.0.0.1:12080/api/panel/events";
const ReconnectMs = 10;

class FakeSocket extends EventTarget {
    static made: FakeSocket[] = [];
    readonly url: string;
    readyState = 0;
    binaryType = "blob";
    readonly sent: string[] = [];

    constructor(url: string) {
        super();
        this.url = url;
        FakeSocket.made.push(this);
    }

    send(data: string) {
        this.sent.push(data);
    }

    close() {
        this.readyState = 3;
    }

    open() {
        this.readyState = 1;
        this.dispatchEvent(new Event("open"));
    }

    receive(frame: Record<string, unknown>) {
        const whole = { v: 1, id: null, scope: null, seq: null, time: 1789650000123, data: {}, ...frame };
        this.dispatchEvent(new MessageEvent("message", { data: JSON.stringify(whole) }));
    }

    shut(code: number) {
        this.readyState = 3;
        this.dispatchEvent(new CloseEvent("close", { code, reason: "" }));
    }

    frames(): Record<string, unknown>[] {
        return this.sent.map((text) => JSON.parse(text) as Record<string, unknown>);
    }
}

function latest(): FakeSocket {
    const socket = FakeSocket.made[FakeSocket.made.length - 1];
    if (!socket) throw new Error("no socket was opened");
    return socket;
}

async function settle(ms = 0) {
    await vi.advanceTimersByTimeAsync(ms);
    for (let turn = 0; turn < 10; turn += 1) await Promise.resolve();
}

async function connected(pingIntervalMs?: number): Promise<FakeSocket> {
    connectEvents({ WebSocket: FakeSocket, url: () => Address, minReconnectMs: ReconnectMs, maxReconnectMs: ReconnectMs, pingIntervalMs });
    await settle();
    const socket = latest();
    socket.open();
    return socket;
}

async function reopened(): Promise<FakeSocket> {
    const before = FakeSocket.made.length;
    await settle(ReconnectMs);
    expect(FakeSocket.made.length).toBe(before + 1);
    const socket = latest();
    socket.open();
    return socket;
}

function ready(socket: FakeSocket, instance = "run-one", panel: string[] = ["status.read"]) {
    socket.receive({
        type: "ready",
        id: "hello-1",
        data: {
            version: 1,
            server_time: 1789650000123,
            instance,
            permissions: { panel, apps: {} },
            apps: [{ name: "gameserver-1" }],
            realms: [],
        },
    });
}

function status(seq: number, state = "running") {
    return {
        type: "status",
        seq,
        scope: { app: "gameserver-1" },
        data: { app: "gameserver-1", state, since: 1789650000000 + seq, pid: 4000 + seq, exit_code: null, crashes: 1, next_restart: null },
    };
}

function supervisedApp(): AppEntry {
    return {
        name: "gameserver-1",
        role: "gameserver",
        realm: "",
        address: "",
        port: 0,
        revision: "",
        supervision: {
            name: "gameserver-1",
            program: "gameserver",
            config: "gameserver.conf",
            state: "offline",
            watching: true,
            desired: "running",
            pid: null,
            adopted: false,
            started_epoch_ms: null,
            ready_epoch_ms: null,
            start: null,
            admin: { enabled: true, address: "127.0.0.1", port: 12021, problem: null },
            stop: null,
            restart_epoch_ms: 1789650001000,
            crashes: 0,
            failed_starts: 0,
            restarts: 0,
            last_exit: null,
            exits: [],
            message: null,
            disabled: null,
            held: null,
        },
    };
}

beforeEach(() => {
    vi.useFakeTimers();
    FakeSocket.made = [];
    session.state = "signed-in";
    session.csrf = "csrf-token";
    session.via = "session";
    session.panel = true;
    session.ended = false;
    live.apps = [];
});

afterEach(() => {
    disconnectEvents();
    vi.useRealTimers();
});

describe("the contract between the server and the page", () => {
    it("every type the server can send has a dashboard handler", () => {
        expect(unhandledTypes(sentTypes, handlers)).toEqual([]);
        for (const type of sentTypes) expect(serverTypes).toContain(type);
    });

    it("a server type added without a dashboard handler is reported by name", () => {
        expect(unhandledTypes([...sentTypes, "power.result"], handlers)).toEqual(["power.result"]);
        expect(unhandledTypes(["toString"], handlers)).toEqual(["toString"]);
    });
});

describe("opening the socket", () => {
    it("resolves the panel's own events path with ws or wss and nothing after it", () => {
        expect(eventsUrl("http://127.0.0.1:12080/#overview")).toBe("ws://127.0.0.1:12080/api/panel/events");
        expect(eventsUrl("https://panel.example.test/ambrose/?next=1#servers")).toBe("wss://panel.example.test/ambrose/api/panel/events");
    });

    it("opens one socket whose first frame is hello with the session's CSRF token", async () => {
        const socket = await connected();
        connectEvents({ WebSocket: FakeSocket, url: () => Address });
        await settle();
        expect(FakeSocket.made).toHaveLength(1);
        expect(socket.url).toBe(Address);
        expect(socket.url).not.toContain("?");
        expect(events.state).toBe("open");
        const [hello] = socket.frames();
        expect(hello).toMatchObject({ v: 1, type: "hello", scope: null, data: { version: 1, csrf: "csrf-token" } });
        expect(typeof hello?.id).toBe("string");
    });

    it("sends and queues nothing while the socket is not ready", async () => {
        expect(send("ping", {})).toBe(false);
        await expect(ask("ping", {})).rejects.toMatchObject({ code: "not_connected" });
        const socket = await connected();
        expect(send("ping", {})).toBe(false);
        expect(socket.frames().map((frame) => frame.type)).toEqual(["hello"]);
        ready(socket);
        expect(send("ping", {})).toBe(true);
        expect(socket.frames().map((frame) => frame.type)).toEqual(["hello", "ping"]);

        socket.shut(1006);
        expect(send("ping", {})).toBe(false);
        const again = await reopened();
        expect(again.frames().map((frame) => frame.type)).toEqual(["hello"]);
    });
});

describe("ready, records and resume", () => {
    it("adopts the snapshot and resumes followed streams after the last sequence it saw", async () => {
        live.apps = [supervisedApp()];
        const first = await connected();
        follow("status");
        expect(first.frames().map((frame) => frame.type)).toEqual(["hello"]);
        ready(first);
        expect(events.state).toBe("ready");
        expect(events.instance).toBe("run-one");
        expect(events.apps).toEqual(["gameserver-1"]);
        expect(events.permissions.panel).toEqual(["status.read"]);
        expect(first.frames()[1]).toMatchObject({ type: "resume", scope: null, seq: 0, data: { stream: "status" } });

        first.receive(status(1));
        first.receive(status(2));
        first.receive(status(3, "running"));
        first.receive(status(2, "crashed"));
        expect(followedStreams()).toEqual([{ stream: "status", app: null, seq: 3 }]);
        expect(live.apps[0]?.supervision?.state).toBe("running");

        first.shut(1006);
        expect(events.state).toBe("connecting");
        const second = await reopened();
        ready(second);
        expect(second.frames()[0]).toMatchObject({ type: "hello" });
        expect(second.frames()[1]).toMatchObject({ type: "resume", scope: null, seq: 3, data: { stream: "status" } });

        second.shut(closeCodes.limits);
        const third = await reopened();
        ready(third, "run-two");
        expect(third.frames()[1]).toMatchObject({ type: "resume", seq: 0 });
        expect(followedStreams()).toEqual([{ stream: "status", app: null, seq: 0 }]);
    });

    it("applies a status record to the matching app in the live picture", async () => {
        live.apps = [supervisedApp()];
        const socket = await connected();
        ready(socket);
        follow("status", "gameserver-1");
        expect(socket.frames()[1]).toMatchObject({ type: "resume", scope: { app: "gameserver-1" }, seq: 0 });
        socket.receive(status(5));
        const supervision = live.apps[0]?.supervision;
        expect(supervision?.state).toBe("running");
        expect(supervision?.pid).toBe(4005);
        expect(supervision?.crashes).toBe(1);
        expect(supervision?.restart_epoch_ms).toBeNull();
    });

    it("keeps a dropped range as a gap for its stream", async () => {
        const socket = await connected();
        follow("status");
        ready(socket);
        socket.receive({ type: "dropped", data: { stream: "status", count: 18, first: 8, last: 25 } });
        expect(events.gaps).toEqual([{ stream: "status", app: null, first: 8, last: 25, count: 18, at: 1789650000123 }]);
        expect(followedStreams()[0]?.seq).toBe(25);
    });

    it("moves past a dropped range only for the scope whose session dropped it", async () => {
        const socket = await connected();
        follow("status");
        follow("status", "gameserver-1");
        ready(socket);
        socket.receive({ type: "dropped", scope: { app: "gameserver-1" }, data: { stream: "status", count: 3, first: 4, last: 6 } });
        expect(followedStreams()).toEqual([
            { stream: "status", app: null, seq: 0 },
            { stream: "status", app: "gameserver-1", seq: 6 },
        ]);
        expect(events.gaps).toEqual([{ stream: "status", app: "gameserver-1", first: 4, last: 6, count: 3, at: 1789650000123 }]);
    });

    it("reads which scopes may follow a stream from the snapshot", () => {
        expect(readableScopes("status.read", { panel: ["status.read"], apps: {} })).toEqual([null]);
        expect(
            readableScopes("status.read", { panel: [], apps: { "gameserver-1": ["status.read"], loginserver: ["console.read"] } }),
        ).toEqual(["gameserver-1"]);
    });
});

describe("answers and errors", () => {
    it("settles a request by its id and keeps any other error", async () => {
        const socket = await connected();
        ready(socket);

        const failing = ask("stats.now", {});
        const asked = socket.frames().at(-1);
        expect(asked).toMatchObject({ type: "stats.now" });
        socket.receive({
            type: "error",
            id: asked?.id,
            data: { request: asked?.id, code: "failed", message: "The panel could not do that", correlation: "AbCdEfGhIjKlMnOp" },
        });
        await expect(failing).rejects.toBeInstanceOf(EventError);
        await expect(failing).rejects.toMatchObject({ code: "failed", correlation: "AbCdEfGhIjKlMnOp" });

        const answered = ask("ping", {});
        const ping = socket.frames().at(-1);
        socket.receive({ type: "pong", id: ping?.id });
        await expect(answered).resolves.toMatchObject({ type: "pong", id: ping?.id });

        socket.receive({
            type: "error",
            data: { request: null, code: "invalid", message: "resume names its stream", correlation: "QrStUvWxYz012345" },
        });
        expect(events.error).toMatchObject({ code: "invalid", message: "resume names its stream", correlation: "QrStUvWxYz012345" });
    });
});

describe("closing and reconnecting", () => {
    it("never reconnects after a malformed frame, an ended session or lost access", () => {
        expect(reconnectsAfter({ code: closeCodes.malformed })).toBe(false);
        expect(reconnectsAfter({ code: closeCodes.session_ended })).toBe(false);
        expect(reconnectsAfter({ code: closeCodes.access_lost })).toBe(false);
        expect(reconnectsAfter({ code: closeCodes.limits })).toBe(true);
        expect(reconnectsAfter({ code: 1006 })).toBe(true);
    });

    it("ends the session on 4401 and stays closed", async () => {
        const socket = await connected();
        ready(socket);
        const pendingAnswer = ask("ping", {});
        socket.shut(closeCodes.session_ended);
        await expect(pendingAnswer).rejects.toMatchObject({ code: "disconnected" });
        expect(events.state).toBe("closed");
        expect(events.closedWith).toBe(4401);
        expect(session.state).toBe("signed-out");
        expect(session.csrf).toBeNull();
        expect(session.ended).toBe(true);
        await settle(10 * ReconnectMs);
        expect(FakeSocket.made).toHaveLength(1);
    });

    it("reconnects when a ping goes unanswered and not while pongs come back", async () => {
        const socket = await connected(1000);
        ready(socket);
        await settle(1000);
        const ping = socket.frames().at(-1);
        expect(ping).toMatchObject({ type: "ping", scope: null, data: {} });
        socket.receive({ type: "pong", id: ping?.id });
        await settle(1000);
        expect(FakeSocket.made).toHaveLength(1);
        expect(socket.frames().filter((frame) => frame.type === "ping")).toHaveLength(2);

        await settle(1000);
        expect(events.state).toBe("connecting");
        const again = await reopened();
        expect(again.frames()[0]).toMatchObject({ type: "hello" });
    });
});
