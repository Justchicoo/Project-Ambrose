/*
 * Project Ambrose by Imjustchico
 * Tests the console page in a real browser against a stubbed admin API: a command that cannot be undone is refused by the server rather than run, the page holds it and says how to answer, typing yes sends that same command again with the confirmation rather than sending the word yes as a command of its own, and typing anything else sends nothing at all and says the command was left alone.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { session } from "$lib/api.svelte";
import { live } from "$lib/status.svelte";
import type { AppEntry, Status, Supervision } from "$lib/schemas";
import Console from "./Console.svelte";

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

function runningApp(name: string): AppEntry {
    return { name, role: name, realm: "", address: "127.0.0.1", port: 12000, revision: "abc1234", supervision: supervision(name) };
}

const sent: { method: string; path: string; body: Record<string, unknown> | undefined }[] = [];

let printed: { seq: number; stream: string; text: string; epoch_ms: number | null }[] = [];

function answer(body: unknown, code = 200): Response {
    return new Response(JSON.stringify(body), { status: code, headers: { "Content-Type": "application/json" } });
}

let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

function open() {
    host = document.createElement("div");
    document.body.append(host);
    page = mount(Console, { target: host });
    flushSync();
}

async function type(text: string) {
    const field = host.querySelector<HTMLInputElement>("input");
    expect(field, "the console has a prompt to type into").not.toBeNull();
    if (!field) return;
    await vi.waitFor(() => {
        flushSync();
        expect(field.disabled, "the prompt must be open before anything is typed").toBe(false);
    });
    field.value = text;
    field.dispatchEvent(new Event("input", { bubbles: true }));
    flushSync();
    field.form?.dispatchEvent(new Event("submit", { bubbles: true, cancelable: true }));
    await vi.waitFor(() => {
        flushSync();
        expect(field.disabled, "the prompt reopens once the command has been answered").toBe(false);
    });
}

function commandsSent() {
    return sent.filter((one) => one.path.endsWith("/command")).map((one) => one.body);
}

beforeEach(() => {
    sent.length = 0;
    printed = [];
    vi.stubGlobal("fetch", (path: string, options: RequestInit) => {
        const body = options.body ? (JSON.parse(String(options.body)) as Record<string, unknown>) : undefined;
        sent.push({ method: options.method ?? "GET", path, body });
        if (path.endsWith("/command")) {
            if (body?.command === "shutdown" && body?.confirm !== true)
                return Promise.resolve(
                    answer(
                        {
                            command: "shutdown",
                            success: false,
                            refused: true,
                            needs_confirm: true,
                            reason: "this command changes something that cannot be undone, so it needs confirm",
                            request_id: "req-1",
                            lines: [],
                        },
                        409,
                    ),
                );
            if (body?.command === "shutdown" && body?.confirm === true)
                return Promise.resolve(
                    answer({
                        command: "shutdown",
                        success: true,
                        refused: false,
                        needs_confirm: false,
                        reason: "",
                        request_id: "req-2",
                        lines: ["stopping"],
                    }),
                );
            return Promise.resolve(
                answer(
                    {
                        command: String(body?.command ?? ""),
                        success: false,
                        refused: true,
                        needs_confirm: false,
                        reason: "there is no such command",
                        request_id: "req-3",
                        lines: [],
                    },
                    409,
                ),
            );
        }
        if (path.includes("/output/current")) {
            return Promise.resolve(
                answer({
                    schema: 1,
                    app: "loginserver",
                    run: "current",
                    lines: printed,
                }),
            );
        }
        return Promise.resolve(answer({}));
    });
    session.csrf = "token";
    session.state = "signed-in";
    live.status = status;
    live.now = Date.now();
    live.apps = [runningApp("loginserver")];
});

afterEach(() => {
    if (page) unmount(page);
    page = null;
    host?.remove();
    vi.unstubAllGlobals();
});

describe("the console asking before something that cannot be undone", () => {
    it("sends the same command again with the confirmation when yes is typed", async () => {
        open();
        await type("shutdown");
        await type("yes");

        const commands = commandsSent();
        expect(commands.length, "one refused send and one confirmed send").toBe(2);
        expect(commands[0]).toEqual({ command: "shutdown" });
        expect(commands[1], "yes must re-send shutdown, not send the word yes").toEqual({ command: "shutdown", confirm: true });
    });

    it("sends nothing when anything other than yes is typed", async () => {
        open();
        await type("shutdown");
        await type("no");

        const commands = commandsSent();
        expect(commands.length, "cancelling must send nothing at all").toBe(1);
        expect(commands[0]).toEqual({ command: "shutdown" });
        expect(host.textContent).toContain("left alone");
    });

    it("shows what the server printed, without anybody typing a command", async () => {
        printed = [
            { seq: 1, stream: "out", text: "loginserver ready", epoch_ms: Date.now() },
            { seq: 2, stream: "out", text: "Listening on 0.0.0.0:12000", epoch_ms: Date.now() },
        ];
        open();

        await vi.waitFor(() => {
            flushSync();
            expect(host.textContent ?? "").toContain("loginserver ready");
        });
        const text = host.textContent ?? "";
        expect(text, "everything the server says belongs here").toContain("Listening on 0.0.0.0:12000");
        expect(text, "the empty state is gone once the server has said something").not.toContain("Waiting for");
    });

    it("adds only what is new rather than repeating the whole run each beat", async () => {
        printed = [{ seq: 1, stream: "out", text: "first line", epoch_ms: Date.now() }];
        open();
        await vi.waitFor(() => {
            flushSync();
            expect(host.textContent ?? "").toContain("first line");
        });

        printed = [
            { seq: 1, stream: "out", text: "first line", epoch_ms: Date.now() },
            { seq: 2, stream: "out", text: "second line", epoch_ms: Date.now() },
        ];
        live.now = Date.now() + 1000;
        await vi.waitFor(() => {
            flushSync();
            expect(host.textContent ?? "").toContain("second line");
        });

        const body = host.textContent ?? "";
        const first = body.split("first line").length - 1;
        expect(first, "a line the console already holds must not be added again").toBe(1);
    });
});
