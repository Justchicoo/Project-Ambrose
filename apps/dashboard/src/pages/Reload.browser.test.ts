/*
 * Project Ambrose by Imjustchico
 * Tests the reload page in a real browser against a stubbed admin API: a caller allowed only to read what can be reloaded sees the targets with no button to run one, a caller allowed to run a reload gets one per target and one for all, the page says it is reading while the app has not answered rather than showing an empty list, a read that fails shows its reason with no claim that nothing can be reloaded, and the changes that need a restart are listed with why.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { session } from "$lib/api.svelte";
import { live } from "$lib/status.svelte";
import type { Status } from "$lib/schemas";
import Reload from "./Reload.svelte";

const status: Status = {
    schema: 1,
    app: "gameserver",
    role: "game",
    realm: "Ambrose",
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

const targets = {
    schema: 1,
    targets: [
        { target: "messages", generation: 2, ran: true, ok: true, errors: [], finished_ms: 1790000000000 },
        {
            target: "locale",
            generation: 1,
            ran: true,
            ok: false,
            errors: ["Locale/en-US/Quests.lang: line 4 has no key"],
            finished_ms: null,
        },
    ],
};

let reloadAnswer: () => Promise<Response>;

function answer(body: unknown, code = 200): Response {
    return new Response(JSON.stringify(body), { status: code, headers: { "Content-Type": "application/json" } });
}

let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

function open() {
    host = document.createElement("div");
    document.body.append(host);
    page = mount(Reload, { target: host });
    flushSync();
}

function buttons(): string[] {
    return [...host.querySelectorAll<HTMLButtonElement>("button")].map(
        (button) => button.getAttribute("aria-label") ?? button.textContent?.trim() ?? "",
    );
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
    reloadAnswer = () => Promise.resolve(answer(targets));
    vi.stubGlobal("fetch", (path: string) => {
        if (path === "api/reload") return reloadAnswer();
        return Promise.resolve(answer({ error: "not_found", message: `nothing at ${path}` }, 404));
    });
    session.state = "checking";
    session.csrf = "token";
    session.user = null;
    live.status = status;
    live.apps = [];
});

afterEach(() => {
    if (page) unmount(page);
    host.remove();
    session.user = null;
    vi.unstubAllGlobals();
});

describe("the reload page", () => {
    it("shows a caller who may only read the targets no button to run one", async () => {
        signIn(["reload.read"]);
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("messages"));
        expect(host.textContent).toContain("Locale/en-US/Quests.lang: line 4 has no key");
        expect(buttons().filter((label) => label.startsWith("Reload"))).toEqual([]);
    });

    it("gives a caller who may run a reload a button per target and one for all", async () => {
        signIn(["reload.read", "reload.run"]);
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("messages"));
        expect(buttons().filter((label) => label.startsWith("Reload"))).toEqual(["Reload all", "Reload messages", "Reload locale"]);
    });

    it("lists the changes no reload can take, each with why", async () => {
        signIn(["reload.read"]);
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("Changes that need a restart"));
        expect(host.textContent).toContain("Binary upgrade");
        expect(host.textContent).toContain("The running process cannot replace its executable and code safely.");
        expect(host.textContent).toContain("Client-side WAD changes");
        expect(host.textContent).toContain("The client must be re-patched before it can use the changed archive.");
        expect(host.querySelector("a[href='#config']")).not.toBeNull();
    });

    it("says it is reading until the app answers, and never calls a failed read an empty list", async () => {
        let release: (response: Response) => void = () => {};
        reloadAnswer = () => new Promise((resolve) => (release = resolve));
        signIn(["reload.read", "reload.run"]);
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("Reading what gameserver can reload"));
        expect(host.textContent).not.toContain("nothing registered");
        release(answer({ error: "app_not_running", message: "gameserver is not running" }, 503));
        await vi.waitFor(() => expect(host.textContent).toContain("gameserver is not running"));
        expect(host.textContent).toContain("What can be reloaded is not known until gameserver answers");
        expect(host.textContent).not.toContain("nothing registered");
    });
});
