/*
 * Project Ambrose by Imjustchico
 * Tests the resources page in a real browser against a stubbed admin API: it reads the history from the server that served it rather than from each app, one row per series with the latest reading and the lowest and highest of the range, a series with a stretch nothing was recorded in is marked as holding fewer readings than the range has points rather than being drawn as though it were complete, a series with nothing at all in the range says so instead of showing a zero, and choosing a longer range asks the server for that range rather than redrawing the one already held.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { session } from "$lib/api.svelte";
import { live } from "$lib/status.svelte";
import type { Status } from "$lib/schemas";
import Graphs from "./Graphs.svelte";

const status: Status = {
    schema: 1,
    app: "supervisor",
    role: "supervisor",
    realm: "",
    revision: "abc1234",
    state: "running",
    uptime: 120,
    memory: null,
    threads: null,
    sessions: null,
    tick: null,
    stats: {},
    problems: [],
};

const asked: string[] = [];

function listing() {
    return {
        schema: 1,
        now_epoch_ms: 1_700_000_000_000,
        subjects: [
            { subject: "gameserver", series: ["cpu_percent", "memory_bytes", "threads"] },
            { subject: "loginserver", series: ["cpu_percent"] },
        ],
    };
}

function range(series: string) {
    const at = [1_700_000_000_000, 1_700_000_060_000, 1_700_000_120_000, 1_700_000_180_000];
    if (series === "cpu_percent") {
        return {
            schema: 1,
            subject: "gameserver",
            series,
            at_epoch_ms: at,
            values: [12.5, null, 40, 25],
            lowest: [12.5, null, 40, 25],
            highest: [12.5, null, 40, 25],
            points: 4,
            present: 3,
        };
    }
    if (series === "memory_bytes") {
        return {
            schema: 1,
            subject: "gameserver",
            series,
            at_epoch_ms: at,
            values: [2 * 1024 * 1024, 3 * 1024 * 1024, 4 * 1024 * 1024, 4 * 1024 * 1024],
            lowest: [],
            highest: [],
            points: 4,
            present: 4,
        };
    }
    return {
        schema: 1,
        subject: "gameserver",
        series,
        at_epoch_ms: at,
        values: [null, null, null, null],
        lowest: [],
        highest: [],
        points: 4,
        present: 0,
    };
}

let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

function open() {
    host = document.createElement("div");
    document.body.append(host);
    page = mount(Graphs, { target: host });
    flushSync();
}

async function drawn(text: string) {
    await vi.waitFor(() => {
        flushSync();
        expect(host.textContent ?? "").toContain(text);
    });
}

beforeEach(() => {
    asked.length = 0;
    vi.stubGlobal("fetch", (path: string) => {
        asked.push(path);
        const body = path.includes("graphs/range") ? range(new URL(path, "http://panel").searchParams.get("series") ?? "") : listing();
        return Promise.resolve(new Response(JSON.stringify(body), { status: 200, headers: { "Content-Type": "application/json" } }));
    });
    session.csrf = "token";
    session.state = "signed-in";
    live.status = status;
    live.now = Date.now();
    live.apps = [];
});

afterEach(() => {
    if (page) unmount(page);
    page = null;
    host?.remove();
    vi.unstubAllGlobals();
});

describe("the resources page", () => {
    it("reads the history from the server that served it rather than from each app", async () => {
        open();
        await drawn("gameserver");
        expect(asked.some((path) => path === "api/graphs")).toBe(true);
        expect(asked.some((path) => path.includes("api/apps/"))).toBe(false);
    });

    it("names every series of the chosen server with its latest reading", async () => {
        open();
        await drawn("Processor");
        const text = host.textContent ?? "";
        expect(text, "the last reading that was present, not the last slot").toContain("25%");
        expect(text, "memory is written in units a reader uses").toContain("4 MiB");
        expect(text, "every series of the chosen server is named, in words rather than in the wire name").toContain("Threads");
        expect(text, "and not the wire name").not.toContain("memory_bytes");
    });

    it("shows the lowest and highest of the range beside the latest", async () => {
        open();
        await drawn("Processor");
        const text = host.textContent ?? "";
        expect(text, "the lowest processor reading in the range").toContain("12.5%");
        expect(text, "and the highest").toContain("40%");
    });

    it("marks a series that holds fewer readings than the range has points", async () => {
        open();
        await drawn("Processor");
        expect(host.textContent ?? "", "three readings across four points is a gap and must be visible as one").toContain("3 of 4");
    });

    it("says a series with nothing in the range is empty rather than showing a zero", async () => {
        open();
        await drawn("Threads");
        expect(host.textContent ?? "").toContain("nothing in this range");
        expect(host.textContent ?? "").toContain("no reading");
    });

    it("asks the server for a longer range rather than redrawing the one it holds", async () => {
        open();
        await drawn("Processor");
        const before = asked.filter((path) => path.includes("graphs/range")).length;
        expect(before).toBeGreaterThan(0);

        const buttons = Array.from(host.querySelectorAll("button"));
        const month = buttons.find((one) => (one.textContent ?? "").trim() === "30d");
        expect(month, "the range chooser offers thirty days").toBeDefined();
        month?.click();
        flushSync();

        await vi.waitFor(() => {
            flushSync();
            const sent = asked.filter((path) => path.includes("graphs/range"));
            expect(sent.length).toBeGreaterThan(before);
            const last = sent[sent.length - 1];
            const parameters = new URL(last, "http://panel").searchParams;
            const span = Number(parameters.get("to")) - Number(parameters.get("from"));
            expect(span, "thirty days rather than the hour it started on").toBe(30 * 24 * 60 * 60 * 1000);
        });
    });
});
