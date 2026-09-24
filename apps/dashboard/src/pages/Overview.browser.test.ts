/*
 * Project Ambrose by Imjustchico
 * Tests the overview in a real browser from a live picture the test sets: a problem the build reports shows with the button that opens the page that fixes it, one the build does not report keeps its message but loses its button, a sample past its freshness budget says how old it is instead of passing as current, and an app that stopped answering reads as not answering.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it } from "vitest";
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
    host = document.createElement("div");
    document.body.append(host);
    page = mount(Overview, { target: host });
    flushSync();
});

afterEach(() => {
    if (page) unmount(page);
    host.remove();
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
});
