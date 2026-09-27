/*
 * Project Ambrose by Imjustchico
 * Tests the overview in a real browser: it reports live issues, freshness, tick subsystem budgets and unavailable timing, and downloads an on-demand world tick trace through the panel API.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { FreshnessMs, live } from "$lib/status.svelte";
import type { Capabilities, Status } from "$lib/schemas";
import Overview from "./Overview.svelte";

const status: Status = {
    schema: 1,
    app: "gameserver",
    role: "game",
    realm: "",
    revision: "abc1234",
    state: "running",
    uptime: 3725,
    memory: { resident_bytes: 150_000_000 },
    threads: 14,
    sessions: 3,
    tick: { average_ms: 3.4, max_ms: 11.8, samples: 60, window_seconds: 60 },
    stats: {},
    problems: [{ code: "type_dump_missing", message: "No type dump is in use", subject: "client" }],
};

function capabilities(codes: string[]): Capabilities {
    return {
        schema: 1,
        reload_targets: [],
        schedule_actions: [],
        announcement_channels: [],
        problem_codes: codes.map((code) => ({ code, description: code })),
    };
}

let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

beforeEach(() => {
    const now = Date.now();
    live.apps = [{ name: "gameserver", role: "game", realm: "", address: "127.0.0.1", port: 12001, revision: "abc1234" }];
    live.status = status;
    live.capabilities = capabilities(["type_dump_missing"]);
    live.receivedAt = now;
    live.now = now;
    live.connection = "live";
    vi.stubGlobal("fetch", (path: string, init?: RequestInit) =>
        Promise.resolve(
            new Response(
                JSON.stringify(
                    path.includes("tick-profile/trace")
                        ? { schema: 1, requested_seconds: 1, truncated: false, events: 2, trace: '{"traceEvents":[]}' }
                        : path.endsWith("tick-profile")
                          ? init?.method === "POST"
                              ? { schema: 1, active: true, complete: false, truncated: false, requested_seconds: 1, events: 0 }
                              : { schema: 1, active: false, complete: true, truncated: false, requested_seconds: 1, events: 2 }
                          : {
                                schema: 1,
                                metrics: [
                                    {
                                        name: "ambrose_world_tick_subsystem_nanoseconds",
                                        kind: "gauge",
                                        series: [
                                            { labels: { component: "network_drain" }, value: 15000000 },
                                            { labels: { component: "movement" }, value: 0 },
                                            { labels: { component: "database_waits" }, value: 0 },
                                            { labels: { component: "combat" }, value: 0 },
                                        ],
                                    },
                                    {
                                        name: "ambrose_world_tick_subsystem_budget_nanoseconds",
                                        kind: "gauge",
                                        series: [{ labels: { component: "network_drain" }, value: 5000000 }],
                                    },
                                    {
                                        name: "ambrose_world_tick_subsystem_available",
                                        kind: "gauge",
                                        series: [
                                            { labels: { component: "network_drain" }, value: 1 },
                                            {
                                                labels: {
                                                    component: "movement",
                                                    reason: "Queued movement is not separately timed outside handler drain",
                                                },
                                                value: 0,
                                            },
                                            {
                                                labels: {
                                                    component: "database_waits",
                                                    reason: "Database work runs on asynchronous workers outside the world tick",
                                                },
                                                value: 0,
                                            },
                                            {
                                                labels: { component: "combat", reason: "Combat is not integrated into the world tick yet" },
                                                value: 0,
                                            },
                                        ],
                                    },
                                    {
                                        name: "ambrose_world_tick_subsystem_over_budget",
                                        kind: "gauge",
                                        series: [{ labels: { component: "network_drain" }, value: 1 }],
                                    },
                                ],
                            },
                ),
                {
                    status: path.endsWith("tick-profile") && init?.method === "POST" ? 202 : 200,
                    headers: { "Content-Type": "application/json" },
                },
            ),
        ),
    );
    host = document.createElement("div");
    document.body.append(host);
    page = mount(Overview, { target: host });
    flushSync();
});

afterEach(() => {
    if (page) unmount(page);
    host.remove();
    vi.unstubAllGlobals();
});

describe("the overview", () => {
    it("shows a problem the build reports with the button that opens the page that fixes it", () => {
        expect(host.textContent).toContain("No type dump is in use");
        const fix = [...host.querySelectorAll("a")].find((link) => link.textContent?.includes("Open client data"));
        expect(fix?.getAttribute("href")).toBe("#client");
        expect(host.textContent).toContain("Needs attention");
        expect(host.textContent).toContain("127.0.0.1:12001");
    });

    it("keeps the message but leaves out the button when the build does not report the problem", () => {
        live.capabilities = capabilities([]);
        flushSync();
        expect(host.textContent).toContain("No type dump is in use");
        expect([...host.querySelectorAll("a")].some((link) => link.textContent?.includes("Open client data"))).toBe(false);
    });

    it("says how old a stale sample is instead of passing it as current", () => {
        live.now = live.receivedAt + FreshnessMs + 7000;
        flushSync();
        expect(host.textContent).toContain("Last sample 10 s ago");
        expect(host.textContent).not.toContain("Updated");
    });

    it("reads an app that stopped answering as not answering", () => {
        live.connection = "reconnecting";
        flushSync();
        expect(host.textContent).toContain("Not answering");
        expect(host.textContent).not.toContain("Needs attention");
    });

    it("names the slow tick subsystem and shows an unlanded subsystem as unavailable", async () => {
        await vi.waitFor(() => {
            flushSync();
            expect(host.textContent).toContain("Network queue drain");
        });
        expect(host.textContent).toContain("15.000 ms");
        expect(host.textContent).toContain("Over budget");
        expect(host.textContent).toContain("Database waits");
        expect(host.textContent).toContain("Outside tick");
        expect(host.textContent).toContain("Movement");
        expect(host.textContent).toContain("Not separately measured");
        expect(host.textContent).toContain("Combat");
        expect(host.textContent).toContain("Not landed");
    });

    it("captures and downloads a Chrome trace for a bounded duration", async () => {
        await vi.waitFor(() => {
            flushSync();
            expect(host.textContent).toContain("Capture trace");
        });
        const capture = [...host.querySelectorAll("button")].find((button) => button.textContent?.includes("Capture trace"));
        capture?.click();
        await vi.waitFor(() => {
            flushSync();
            expect(host.textContent).toContain("Downloaded 2 Chrome trace events");
        });
    });
});
