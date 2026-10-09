/*
 * Project Ambrose by Imjustchico
 * Tests the activity page in a real browser against a stubbed panel API: for the supervisor it reads the audit chain verification, says how many audit events wait for the off-machine collector, and names the row a broken chain breaks at and the last row that fails with it.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { session } from "$lib/api.svelte";
import { live } from "$lib/status.svelte";
import type { AppEntry, Status, Supervision } from "$lib/schemas";
import Activity from "./Activity.svelte";

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

function supervision(name: string): Supervision {
    return {
        name,
        program: `/bin/${name}`,
        config: `/etc/${name}.conf`,
        state: "running",
        watching: true,
        desired: "running",
        pid: 4242,
        adopted: false,
        started_epoch_ms: Date.now() - 65_000,
        ready_epoch_ms: Date.now() - 60_000,
        start: null,
        admin: { enabled: true, address: "127.0.0.1", port: 12010, problem: null },
        stop: null,
        restart_epoch_ms: null,
        crashes: 0,
        failed_starts: 0,
        restarts: 0,
        last_exit: null,
        exits: [],
        message: null,
    };
}

function supervisedApp(name: string): AppEntry {
    return { name, role: name, realm: "", address: "127.0.0.1", port: 12000, revision: "abc1234", supervision: supervision(name) };
}

const validChain = {
    schema: 1,
    valid: true,
    rows_checked: 5,
    elapsed_ms: 1,
    budget_ms: 5000,
    pending_events: 3,
    collector_enabled: true,
    first_invalid_id: null,
    last_row_id: 5,
    problem: "",
};

let chain: Record<string, unknown> = validChain;
let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

function answer(body: unknown): Response {
    return new Response(JSON.stringify(body), { status: 200, headers: { "Content-Type": "application/json" } });
}

function open() {
    host = document.createElement("div");
    document.body.append(host);
    page = mount(Activity, { target: host });
    flushSync();
}

beforeEach(() => {
    chain = validChain;
    vi.stubGlobal("fetch", (path: string) => {
        if (path === "api/panel/audit/verify") return Promise.resolve(answer(chain));
        if (path === "api/activity") return Promise.resolve(answer({ schema: 1, kept: true, written: 0, unreadable: 0, activity: [] }));
        return Promise.resolve(answer({}));
    });
    session.csrf = "token";
    session.state = "signed-in";
    live.status = status;
    live.now = Date.now();
    live.apps = [supervisedApp("loginserver")];
});

afterEach(() => {
    if (page) unmount(page);
    page = null;
    host?.remove();
    vi.unstubAllGlobals();
});

describe("the activity page's audit chain card", () => {
    it("says how many audit events wait for the collector", async () => {
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("3 audit event(s) are waiting"));
        expect(host.textContent).toContain("Valid; 5 row(s) checked");
    });

    it("names the row the chain breaks at and the last row that fails with it", async () => {
        chain = {
            ...validChain,
            valid: false,
            first_invalid_id: 2,
            last_row_id: 4,
            problem: "audit row 2 does not match its contents or previous row",
        };
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("Broken at row 2"));
        expect(host.textContent?.replace(/\s+/g, " ")).toContain("rows 2 to 4 do not verify");
    });
});
