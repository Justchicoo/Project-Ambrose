/*
 * Project Ambrose by Imjustchico
 * Tests the config page in a real browser against a stubbed admin API: each option shows the value in use, the shipped default, the layer it was read from with its file and line, a secret shows only the mask, an option the app declares restart-required carries that badge and the reason while the others carry neither, and the search narrows the list to what matches.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { live } from "$lib/status.svelte";
import type { Status } from "$lib/schemas";
import Settings from "./Settings.svelte";

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

const answer = {
    schema: 1,
    file: "/etc/loginserver.conf",
    settings: [
        {
            key: "Login.Name",
            value: "Ambrose",
            layer: "config",
            file: "/etc/loginserver.conf",
            line: 12,
            default: "Ambrose",
            default_file: "/etc/loginserver.conf.dist",
            secret: false,
            restart_reason: null,
        },
        {
            key: "Setup.Mode",
            value: "off",
            layer: "environment",
            file: "AMBROSE_SETUP_MODE",
            line: 0,
            default: "auto",
            default_file: "/etc/loginserver.conf.dist",
            secret: false,
            restart_reason: "Setup runs only while the server starts",
        },
        {
            key: "LoginDatabaseInfo",
            value: "127.0.0.1;3306;ambrose;***;ambrose_login",
            layer: "config",
            file: "/etc/loginserver.conf",
            line: 20,
            default: "127.0.0.1;3306;ambrose;***;ambrose_login",
            secret: true,
            default_file: "/etc/loginserver.conf.dist",
            restart_reason: null,
        },
    ],
};

let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

beforeEach(() => {
    vi.stubGlobal("fetch", () =>
        Promise.resolve(new Response(JSON.stringify(answer), { status: 200, headers: { "Content-Type": "application/json" } })),
    );
    live.status = status;
    live.apps = [{ name: "loginserver", role: "login", realm: "", address: "127.0.0.1", port: 12000, revision: "abc1234" }];
    host = document.createElement("div");
    document.body.append(host);
    page = mount(Settings, { target: host });
    flushSync();
});

afterEach(() => {
    if (page) unmount(page);
    host.remove();
    vi.unstubAllGlobals();
});

describe("the config page", () => {
    it("shows each value with its default and where it was read from", async () => {
        await vi.waitFor(() => expect(host.textContent).toContain("Login.Name"));
        expect(host.textContent).toContain("3 options from");
        expect(host.textContent).toContain("/etc/loginserver.conf:12");
        expect(host.textContent).toContain("Environment variable");
        expect(host.textContent).toContain("AMBROSE_SETUP_MODE");
        expect(host.textContent).toContain("Changed from the default");
    });

    it("marks only the options the app declares restart-required and masks secrets", async () => {
        await vi.waitFor(() => expect(host.textContent).toContain("Setup.Mode"));
        expect(host.textContent).toContain("Restart required");
        expect(host.textContent).toContain("Setup runs only while the server starts");
        expect(host.textContent).toContain("1 need a restart");
        expect(host.textContent).toContain("Secret");
        expect(host.textContent).toContain("127.0.0.1;3306;ambrose;***;ambrose_login");
    });

    it("narrows the list to what the search matches", async () => {
        await vi.waitFor(() => expect(host.textContent).toContain("Login.Name"));
        const search = host.querySelector<HTMLInputElement>("input");
        expect(search).not.toBeNull();
        if (search) {
            search.value = "setup";
            search.dispatchEvent(new Event("input", { bubbles: true }));
            flushSync();
        }
        expect(host.textContent).toContain("Setup.Mode");
        expect(host.textContent).not.toContain("Login.Name");
    });
});
