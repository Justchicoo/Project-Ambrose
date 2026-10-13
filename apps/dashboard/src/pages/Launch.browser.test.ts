/*
 * Project Ambrose by Imjustchico
 * Tests the launch page in a real browser against a stubbed admin API: a caller allowed only to read sees the settings, the preview of the exact command line and the override locked with the command-line override layer named, with secret values masked and no save control, while a caller allowed to edit gets the save control and saving posts the settings with a reason.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { session } from "$lib/api.svelte";
import { live } from "$lib/status.svelte";
import type { Status } from "$lib/schemas";
import Launch from "./Launch.svelte";

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

const launch = {
    schema: 1,
    app: "gameserver",
    build: "",
    working_directory: "",
    start_timeout: null,
    stop_timeout: 90,
    restart_policy: "on-crash",
    backoff_seconds: null,
    max_restarts: null,
    crash_window_seconds: null,
    autostart: null,
    priority: null,
    memory_limit_bytes: null,
    cpu_limit_percent: null,
    overrides: { "Rate.Drop.Item": "1.5" },
    environment: { DB_PASSWORD: "********", PLAIN: "visible" },
    secret_environment: ["DB_PASSWORD"],
    preview_command_line: "gameserver --config gameserver.conf --set Rate.Drop.Item=1.5",
    preview_environment: [
        { name: "DB_PASSWORD", value: "********", secret: true },
        { name: "PLAIN", value: "visible", secret: false },
    ],
    locked_keys: [{ key: "Rate.Drop.Item", locked: true, layer: "command-line override" }],
};

function answer(body: unknown, code = 200): Response {
    return new Response(JSON.stringify(body), { status: code, headers: { "Content-Type": "application/json" } });
}

let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;
let saved: unknown = null;

function open() {
    host = document.createElement("div");
    document.body.append(host);
    page = mount(Launch, { target: host });
    flushSync();
}

function choose(app: string) {
    const select = host.querySelector<HTMLSelectElement>("#launch-app");
    if (!select) throw new Error("the app picker did not render");
    select.value = app;
    select.dispatchEvent(new Event("change", { bubbles: true }));
    flushSync();
}

function signIn(permissions: string[]) {
    session.state = "signed-in";
    session.user = {
        id: 2,
        username: "merle",
        display_name: "Merle",
        owner: false,
        role: "viewer",
        permissions,
        grants: {},
        must_change_password: false,
        two_factor: false,
        two_factor_required: false,
    };
}

beforeEach(() => {
    saved = null;
    vi.stubGlobal("fetch", (path: string, init?: RequestInit) => {
        if (path === "api/panel/apps/gameserver/launch") {
            if (init?.method === "PUT") saved = JSON.parse(init.body as string);
            return Promise.resolve(answer(launch));
        }
        return Promise.resolve(answer({ error: "not_found", message: `nothing at ${path}` }, 404));
    });
    session.state = "checking";
    session.csrf = "token";
    session.user = null;
    live.status = status;
    live.apps = [
        { name: "supervisor", supervision: null },
        { name: "gameserver", supervision: { admin: { enabled: true } } },
    ] as never;
});

afterEach(() => {
    if (page) unmount(page);
    host.remove();
    session.user = null;
    vi.unstubAllGlobals();
});

describe("the launch page", () => {
    it("shows a reader the preview, the masked secret and the locked override naming its layer", async () => {
        signIn(["launch.read"]);
        open();
        choose("gameserver");
        await vi.waitFor(() => expect(host.textContent).toContain("--set Rate.Drop.Item=1.5"));
        expect(host.textContent).toContain("Rate.Drop.Item is locked by the command-line override layer");
        expect(host.textContent).not.toContain("s3cret");
        expect([...host.querySelectorAll("button")].map((button) => button.textContent?.trim())).not.toContain("Save launch settings");
    });

    it("lets an editor save the settings with a reason", async () => {
        signIn(["launch.read", "launch.edit"]);
        open();
        choose("gameserver");
        await vi.waitFor(() => expect(host.textContent).toContain("--set Rate.Drop.Item=1.5"));
        const reason = host.querySelector<HTMLInputElement>("#launch-reason");
        expect(reason).not.toBeNull();
        reason!.value = "longer stop";
        reason!.dispatchEvent(new Event("input", { bubbles: true }));
        const save = [...host.querySelectorAll("button")].find((button) => button.textContent?.trim() === "Save launch settings");
        expect(save).not.toBeUndefined();
        save!.click();
        await vi.waitFor(() => expect(saved).not.toBeNull());
        expect((saved as Record<string, unknown>)["reason"]).toBe("longer stop");
        expect((saved as Record<string, unknown>)["stop_timeout"]).toBe(90);
    });
});
