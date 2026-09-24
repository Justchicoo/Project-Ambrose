/*
 * Project Ambrose by Imjustchico
 * Tests the database page in a real browser against a stubbed admin API: each database shows its state, address without a password and the connections in use, a pending file that only changes rows is offered for applying while one that changes the schema is marked restart-required with the statement that makes it so and the file behind it says what it waits for, and applying sends the database's name and reads the files again.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { session } from "$lib/api.svelte";
import { live } from "$lib/status.svelte";
import type { Status } from "$lib/schemas";
import Database from "./Database.svelte";

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

const databases = {
    schema: 1,
    databases: [
        {
            name: "login",
            key: "Login",
            state: "open",
            applying: false,
            updates_enabled: true,
            update_flag: 1,
            address: "ambrose@127.0.0.1:3306/ambrose_login",
            pool: {
                async_connections: 1,
                sync_connections: 2,
                async_active: 1,
                sync_leased: 0,
                sync_waiting: 0,
                queued: 3,
                reconnects: 0,
            },
            stores: ["character names"],
        },
    ],
};

const updates = {
    schema: 1,
    databases: [
        {
            name: "login",
            listed: true,
            error: null,
            applied: [
                {
                    name: "2026_01_01_00.sql",
                    state: "released",
                    hash: "a".repeat(64),
                    applied_epoch_ms: 1758500000000,
                    took_ms: 12,
                    present: true,
                    changed: false,
                },
            ],
            pending: [
                {
                    name: "2026_09_20_00.sql",
                    state: "released",
                    file: "/sql/2026_09_20_00.sql",
                    hash: "b".repeat(64),
                    kind: "data",
                    transactional: true,
                    line: null,
                    statement: null,
                    problem: null,
                    renamed_from: null,
                    restart_required: false,
                    waits_for: null,
                },
                {
                    name: "2026_09_21_00.sql",
                    state: "released",
                    file: "/sql/2026_09_21_00.sql",
                    hash: "c".repeat(64),
                    kind: "schema",
                    transactional: false,
                    line: 3,
                    statement: "ALTER TABLE `account` ADD `note` TEXT",
                    problem: null,
                    renamed_from: null,
                    restart_required: true,
                    waits_for: null,
                },
                {
                    name: "2026_09_22_00.sql",
                    state: "released",
                    file: "/sql/2026_09_22_00.sql",
                    hash: "d".repeat(64),
                    kind: "data",
                    transactional: true,
                    line: null,
                    statement: null,
                    problem: null,
                    renamed_from: null,
                    restart_required: true,
                    waits_for: "2026_09_21_00.sql",
                },
            ],
        },
    ],
};

const sent: { method: string; path: string; body: unknown }[] = [];

function answer(body: unknown): Response {
    return new Response(JSON.stringify(body), { status: 200, headers: { "Content-Type": "application/json" } });
}

let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

beforeEach(() => {
    sent.length = 0;
    vi.stubGlobal("fetch", (path: string, options: RequestInit) => {
        sent.push({ method: options.method ?? "GET", path, body: options.body ? JSON.parse(String(options.body)) : undefined });
        if (path.endsWith("database/apply"))
            return Promise.resolve(
                answer({
                    database: "login",
                    succeeded: true,
                    applied: ["2026_09_20_00.sql"],
                    stopped_at: "2026_09_21_00.sql",
                    failed_at: null,
                    failure: null,
                    stores: [{ name: "character names", loaded: true, errors: [], warnings: [] }],
                }),
            );
        if (path.endsWith("database/updates")) return Promise.resolve(answer(updates));
        return Promise.resolve(answer(databases));
    });
    session.csrf = "token";
    live.status = status;
    live.now = Date.now();
    live.apps = [{ name: "loginserver", role: "login", realm: "", address: "127.0.0.1", port: 12000, revision: "abc1234" }];
    host = document.createElement("div");
    document.body.append(host);
    page = mount(Database, { target: host });
    flushSync();
});

afterEach(() => {
    if (page) unmount(page);
    host.remove();
    vi.unstubAllGlobals();
});

describe("the database page", () => {
    it("shows each database with its address and the connections in use", async () => {
        await vi.waitFor(() => expect(host.textContent).toContain("ambrose@127.0.0.1:3306/ambrose_login"));
        expect(host.textContent).toContain("Open");
        expect(host.textContent).toContain("1 async, 2 sync");
        expect(host.textContent).toContain("1 running, 0 leased");
        expect(host.textContent).toContain("character names");
    });

    it("marks a schema update restart-required and the file behind it as waiting", async () => {
        await vi.waitFor(() => expect(host.textContent).toContain("2026_09_21_00.sql"));
        expect(host.textContent).toContain("Data only");
        expect(host.textContent).toContain("Restart required");
        expect(host.textContent).toContain("ALTER TABLE `account` ADD `note` TEXT");
        expect(host.textContent).toContain("It runs after 2026_09_21_00.sql");
        expect(host.textContent).toContain("3 pending, 1 applied");
    });

    it("offers only the data-only files and applies them by name", async () => {
        await vi.waitFor(() => expect(host.textContent).toContain("Apply 1 data-only update"));
        const apply = [...host.querySelectorAll("button")].find((button) => button.textContent?.includes("Apply 1 data-only update"));
        apply?.click();
        flushSync();
        const confirm = [...document.querySelectorAll("button")].find((button) => button.textContent?.trim() === "Apply them");
        confirm?.click();
        await vi.waitFor(() => expect(sent.some((call) => call.method === "POST")).toBe(true));
        const posted = sent.find((call) => call.method === "POST");
        expect(posted?.path).toBe("api/database/apply");
        expect(posted?.body).toEqual({ database: "login" });
        await vi.waitFor(() => expect(sent.filter((call) => call.path.endsWith("database/updates")).length).toBeGreaterThan(1));
    });
});
