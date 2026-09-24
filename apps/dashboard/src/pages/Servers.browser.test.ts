/*
 * Project Ambrose by Imjustchico
 * Tests the servers page in a real browser against a stubbed admin API: every supervised app shows its state, process and crashes with the buttons its state allows, a restart asks first and then sends the countdown that was typed, a start goes straight through, the captured output of the chosen app is shown with the supervisor's own notes apart from the app's, a panel an app served itself says the supervisor is not serving it instead of offering power buttons, and a viewer sees the apps and their state but none of the power buttons, while a sub-user granted a restart on one app sees that one button on that one app.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { session } from "$lib/api.svelte";
import { live } from "$lib/status.svelte";
import type { AppEntry, Status, Supervision } from "$lib/schemas";
import Servers from "./Servers.svelte";

function supervision(name: string, state: string, extra: Partial<Supervision> = {}): Supervision {
    return {
        name,
        program: `/bin/${name}`,
        config: `/etc/${name}.conf`,
        state,
        watching: true,
        desired: state === "offline" ? "stopped" : "running",
        pid: state === "offline" || state === "crashed" ? null : 4242,
        adopted: false,
        started_epoch_ms: state === "offline" ? null : Date.now() - 65_000,
        ready_epoch_ms: state === "running" ? Date.now() - 60_000 : null,
        admin: { enabled: true, address: "127.0.0.1", port: 12010, problem: null },
        stop: null,
        restart_epoch_ms: null,
        crashes: 0,
        failed_starts: 0,
        restarts: 0,
        last_exit: null,
        exits: [],
        message: null,
        ...extra,
    };
}

function app(name: string, state: string, extra: Partial<Supervision> = {}): AppEntry {
    return {
        name,
        role: name,
        realm: "",
        address: "127.0.0.1",
        port: 12000,
        revision: "abc1234",
        supervision: supervision(name, state, extra),
    };
}

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

const sent: { method: string; path: string; body: unknown }[] = [];

function answer(body: unknown, status = 200): Response {
    return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json" } });
}

let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

function open() {
    host = document.createElement("div");
    document.body.append(host);
    page = mount(Servers, { target: host });
    flushSync();
}

beforeEach(() => {
    sent.length = 0;
    vi.stubGlobal("fetch", (path: string, options: RequestInit) => {
        sent.push({ method: options.method ?? "GET", path, body: options.body ? JSON.parse(String(options.body)) : undefined });
        if (path.endsWith("/power")) return Promise.resolve(answer({ app: "loginserver", action: "restart", seconds: 30, accepted: true }));
        return Promise.resolve(
            answer({
                schema: 1,
                app: "loginserver",
                run: "current",
                lines: [
                    { seq: 1, stream: "stdout", text: "loginserver ready", epoch_ms: Date.now() },
                    { seq: 2, stream: "supervisor", text: "loginserver is ready: it printed its ready line", epoch_ms: Date.now() },
                ],
            }),
        );
    });
    session.csrf = "token";
    live.status = status;
    live.now = Date.now();
    live.apps = [
        { name: "supervisor", role: "supervisor", realm: "", address: "", port: 0, revision: "abc1234" },
        app("loginserver", "running"),
        app("gameserver", "crashed", { crashes: 2, message: "It exited without being asked and starts again in 1 s" }),
    ];
});

afterEach(() => {
    if (page) unmount(page);
    host.remove();
    session.user = null;
    vi.unstubAllGlobals();
});

describe("the servers page", () => {
    it("shows every supervised app with the buttons its state allows", async () => {
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("loginserver"));
        expect(host.textContent).toContain("Running");
        expect(host.textContent).toContain("Crashed");
        expect(host.textContent).toContain("It exited without being asked");
        expect(host.textContent).toContain("4242");
        const labels = [...host.querySelectorAll("button")].map((button) => button.textContent?.trim() ?? "");
        expect(labels.some((label) => label.startsWith("Restart"))).toBe(true);
        expect(labels.some((label) => label.startsWith("Stop"))).toBe(true);
        expect(labels.some((label) => label.startsWith("Start"))).toBe(true);
    });

    it("asks before a restart and sends the countdown that was typed", async () => {
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("loginserver"));
        const restart = [...host.querySelectorAll("button")].find((button) => button.textContent?.trim().startsWith("Restart"));
        restart?.click();
        flushSync();
        const field = document.querySelector<HTMLInputElement>("#countdown");
        expect(field).not.toBeNull();
        if (field) {
            field.value = "30";
            field.dispatchEvent(new Event("input", { bubbles: true }));
            flushSync();
        }
        const confirm = [...document.querySelectorAll("button")].find((button) => button.textContent?.trim() === "Restart it");
        confirm?.click();
        await vi.waitFor(() => expect(sent.some((call) => call.path.endsWith("/power"))).toBe(true));
        const power = sent.find((call) => call.path.endsWith("/power"));
        expect(power?.path).toBe("api/apps/loginserver/power");
        expect(power?.body).toEqual({ action: "restart", seconds: 30 });
    });

    it("starts a stopped app without asking", async () => {
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("gameserver"));
        const start = [...host.querySelectorAll("button")].find((button) => button.textContent?.trim().startsWith("Start"));
        start?.click();
        await vi.waitFor(() => expect(sent.some((call) => call.path === "api/apps/gameserver/power")).toBe(true));
        expect(sent.find((call) => call.path === "api/apps/gameserver/power")?.body).toEqual({ action: "start", seconds: 0 });
    });

    it("shows the captured output of the chosen app", async () => {
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("loginserver is ready: it printed its ready line"));
        expect(sent.some((call) => call.path === "api/apps/loginserver/output/current")).toBe(true);
        expect(host.textContent).toContain("Output of loginserver");
    });

    it("says the supervisor is not serving the panel when an app served it", async () => {
        live.apps = [{ name: "patchserver", role: "patch", realm: "", address: "127.0.0.1", port: 12500, revision: "abc1234" }];
        live.status = { ...status, app: "patchserver", role: "patch" };
        open();
        expect(host.textContent).toContain("The supervisor is not serving this panel");
        expect([...host.querySelectorAll("button")].some((button) => button.textContent?.trim().startsWith("Restart"))).toBe(false);
    });
});

describe("the power buttons a viewer and a sub-user see", () => {
    function signedInAs(role: string, permissions: string[], grants: Record<string, string[]> = {}) {
        session.state = "signed-in";
        session.user = {
            id: 2,
            username: "helper",
            display_name: "helper",
            owner: false,
            role,
            permissions,
            grants,
            must_change_password: false,
        };
    }

    it("shows a viewer the apps and their state but no power button", async () => {
        signedInAs("viewer", ["status.read"]);
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("loginserver"));
        expect(host.textContent).toContain("Running");
        const buttons = [...host.querySelectorAll("button")].map((button) => button.textContent?.trim() ?? "");
        expect(buttons.some((word) => word.startsWith("Restart"))).toBe(false);
        expect(buttons.some((word) => word.startsWith("Stop"))).toBe(false);
        expect(buttons.some((word) => word.startsWith("Start"))).toBe(false);
    });

    it("shows a sub-user the one button their grant names on the one app it names", async () => {
        signedInAs("viewer", ["status.read"], { loginserver: ["power.restart"] });
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("loginserver"));
        const buttons = [...host.querySelectorAll("button")].map((button) => button.textContent?.trim() ?? "");
        expect(buttons.filter((word) => word.startsWith("Restart")).length).toBe(1);
        expect(buttons.some((word) => word.startsWith("Stop"))).toBe(false);
        expect(buttons.some((word) => word.startsWith("Start"))).toBe(false);
    });
});
