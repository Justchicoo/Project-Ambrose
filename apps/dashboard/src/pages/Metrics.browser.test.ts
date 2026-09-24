/*
 * Project Ambrose by Imjustchico
 * Tests the metrics page in a real browser against a stubbed admin API: the page asks the server that served it, draws a counter's reading and a histogram's count, total and average, names each series by its labels so two pools are told apart, and redraws when the heartbeat brings new numbers rather than showing the first reading for ever.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { session } from "$lib/api.svelte";
import { live } from "$lib/status.svelte";
import type { Status } from "$lib/schemas";
import Metrics from "./Metrics.svelte";

const status: Status = {
    schema: 1,
    app: "loginserver",
    role: "login",
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

let handled = 7;

function body() {
    return {
        schema: 1,
        metrics: [
            {
                name: "ambrose_messages_handled_total",
                help: "Messages whose handler ran and returned",
                kind: "counter",
                series: [{ labels: { service: "loginserver" }, value: handled }],
            },
            {
                name: "ambrose_database_query_seconds",
                help: "How long a statement took on a connection",
                kind: "histogram",
                series: [
                    {
                        labels: { pool: "login" },
                        count: 4,
                        sum: 0.08,
                        buckets: [
                            { le: 0.005, count: 0 },
                            { le: 0.05, count: 4 },
                        ],
                    },
                    { labels: { pool: "characters" }, count: 0, sum: 0, buckets: [] },
                ],
            },
        ],
    };
}

const asked: string[] = [];
let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

function open() {
    host = document.createElement("div");
    document.body.append(host);
    page = mount(Metrics, { target: host });
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
    handled = 7;
    vi.stubGlobal("fetch", (path: string) => {
        asked.push(path);
        return Promise.resolve(new Response(JSON.stringify(body()), { status: 200, headers: { "Content-Type": "application/json" } }));
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

describe("the metrics page", () => {
    it("reads the metrics of the server that served it", async () => {
        open();
        await drawn("ambrose_messages_handled_total");
        expect(asked.some((path) => path.endsWith("api/metrics"))).toBe(true);
    });

    it("draws a counter's reading and tells two series of one metric apart by their labels", async () => {
        open();
        await drawn("service=loginserver");
        const text = host.textContent ?? "";
        expect(text, "the counter's reading is shown").toContain("7");
        expect(text, "one pool is named").toContain("pool=login");
        expect(text, "and so is the other, so they are not summed into one row").toContain("pool=characters");
    });

    it("turns a histogram into a count, a total and an average rather than raw buckets", async () => {
        open();
        await drawn("ambrose_database_query_seconds");
        const text = host.textContent ?? "";
        expect(text, "80ms over four observations averages 20ms").toContain("20ms");
        expect(text, "a series with nothing observed says so rather than dividing by zero").toContain("no observations yet");
        expect(text, "and the bounds say how the observations were spread").toContain("100% ≤ 50ms");
    });

    it("redraws when the heartbeat brings a new reading", async () => {
        open();
        await drawn("7");
        handled = 41;
        live.now = Date.now() + 1000;
        await drawn("41");
        expect(asked.length, "the page asked again rather than showing the first reading for ever").toBeGreaterThan(1);
    });
});
