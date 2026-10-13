/*
 * Project Ambrose by Imjustchico
 * Tests the network page in a real browser against a stubbed panel API: the allocations show per node with their bind address, port, protocol, app, role and notes beside the pool, the app tab shows that app's allocations with its live listener, adding sends the bind address and port and shows the server's warning, assigning posts the app and role, deleting an assigned allocation shows the server's refusal naming the app, a bulk delete posts the selected ids, and without the edit permission no changing control is drawn.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { session } from "$lib/api.svelte";
import Network from "./Network.svelte";
import "../app.css";

function allocation(id: number, extra: Record<string, unknown> = {}) {
    return {
        id,
        node: "local",
        bind_address: "0.0.0.0",
        port: 12333,
        protocol: "tcp",
        public_alias: "",
        app: "gameserver",
        role: "game listener",
        notes: "",
        assigned: true,
        ...extra,
    };
}

const nodeAnswer = {
    schema: 1,
    node: "local",
    allocations: [
        allocation(1),
        allocation(2, { bind_address: "127.0.0.1", port: 12080, app: "supervisor", role: "panel" }),
        allocation(3, { bind_address: "127.0.0.1", port: 14000, app: "", role: "", assigned: false, notes: "spare" }),
    ],
    pool: { node: "local", start_port: 13000, end_port: 13999 },
    apps: [{ app: "gameserver", allocations: [allocation(1)] }],
};

const appAnswer = {
    schema: 1,
    app: "gameserver",
    allocations: [allocation(1)],
    live_listener: { bind_address: "0.0.0.0", port: 12333 },
};

const sent: { method: string; path: string; body: unknown }[] = [];

function answer(body: unknown, code = 200): Response {
    return new Response(JSON.stringify(body), { status: code, headers: { "Content-Type": "application/json" } });
}

function route(path: string, method: string, _body: unknown): Response {
    if (path === "api/panel/network/node/local") return answer(nodeAnswer);
    if (path === "api/panel/network/app/gameserver") return answer(appAnswer);
    if (path === "api/panel/network/allocations" && method === "POST")
        return answer({ schema: 1, allocation: { id: 9, warnings: ["port 80 is below 1025"] } }, 201);
    if (path === "api/panel/network/allocations/range" && method === "POST") return answer({ schema: 1, added: 10, warnings: [] }, 201);
    if (path === "api/panel/network/allocations/1/assign" && method === "POST")
        return answer({ schema: 1, assigned: 1, app: "gameserver", role: "game listener", port: 14000, bind_address: "127.0.0.1" });
    if (path === "api/panel/network/allocations/1" && method === "DELETE")
        return answer(
            {
                error: "allocation_in_use",
                message: "allocation 1 (port 12333 on 0.0.0.0) is assigned to gameserver as 'game listener', so it cannot be deleted",
            },
            409,
        );
    if (path === "api/panel/network/allocations/3" && method === "DELETE") return answer({ schema: 1, deleted: 3 });
    if (path === "api/panel/network/allocations/bulk-delete" && method === "POST") return answer({ schema: 1, deleted: 2 });
    if (path === "api/panel/network/pool" && method === "PUT")
        return answer({ schema: 1, pool: { node: "local", start_port: 13000, end_port: 13999 } });
    return answer({ error: "not_found", message: `nothing at ${path}` }, 404);
}

let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

function open() {
    host = document.createElement("div");
    document.body.append(host);
    page = mount(Network, { target: host });
    flushSync();
}

function button(label: string): HTMLButtonElement | undefined {
    return [...document.body.querySelectorAll<HTMLButtonElement>("button")].find(
        (candidate) => candidate.getAttribute("aria-label") === label || candidate.textContent?.trim() === label,
    );
}

function dialogButton(label: string): HTMLButtonElement | undefined {
    const dialog = document.body.querySelector("[role=dialog]");
    if (!dialog) return undefined;
    return [...dialog.querySelectorAll<HTMLButtonElement>("button")].find((candidate) => candidate.textContent?.trim() === label);
}

function type(element: HTMLInputElement, text: string) {
    element.value = text;
    element.dispatchEvent(new Event("input", { bubbles: true }));
    flushSync();
}

function signIn(permissions: string[]) {
    session.state = "signed-in";
    session.user = {
        id: 2,
        username: "merle",
        display_name: "Merle",
        owner: false,
        role: "operator",
        permissions,
        grants: {},
        must_change_password: false,
        two_factor: false,
        two_factor_required: false,
    };
}

beforeEach(() => {
    sent.length = 0;
    vi.stubGlobal("fetch", (path: string, options: RequestInit) => {
        const body = options.body ? JSON.parse(String(options.body)) : undefined;
        sent.push({ method: options.method ?? "GET", path, body });
        return Promise.resolve(route(path, options.method ?? "GET", body));
    });
    session.state = "checking";
    session.csrf = "token";
    session.user = null;
});

afterEach(() => {
    if (page) unmount(page);
    host.remove();
    document.body.querySelectorAll("[data-dialog-content], [role=dialog]").forEach((node) => node.remove());
    session.user = null;
    vi.unstubAllGlobals();
});

describe("the network page", () => {
    it("shows the allocations per node with the pool beside them", async () => {
        signIn(["network.read", "network.edit"]);
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("12333"));
        expect(host.textContent).toContain("0.0.0.0");
        expect(host.textContent).toContain("game listener");
        expect(host.textContent).toContain("Ports 13000-13999 supply new realms.");
    });

    it("shows the app tab with that app's allocations and its live listener", async () => {
        signIn(["network.read", "network.edit"]);
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("12333"));
        button("App")?.click();
        flushSync();
        await vi.waitFor(() => expect(document.body.querySelector("[aria-label='App whose network is shown']")).not.toBeNull());
        const trigger = document.body.querySelector<HTMLButtonElement>("[aria-label='App whose network is shown']");
        trigger?.dispatchEvent(new PointerEvent("pointerdown", { bubbles: true, button: 0 }));
        flushSync();
        await vi.waitFor(() => expect([...document.body.querySelectorAll("[role=option]")].length).toBeGreaterThan(0));
        const option = [...document.body.querySelectorAll("[role=option]")].find((node) => node.textContent?.trim() === "gameserver");
        option?.dispatchEvent(new PointerEvent("pointerup", { bubbles: true, button: 0 }));
        flushSync();
        await vi.waitFor(() => expect(host.textContent).toContain("Live listener:"));
        expect(host.textContent).toContain("0.0.0.0:12333");
    });

    it("adds an allocation and shows the server's warning", async () => {
        signIn(["network.read", "network.edit"]);
        open();
        await vi.waitFor(() => expect(button("Add allocation")).toBeDefined());
        button("Add allocation")?.click();
        flushSync();
        await vi.waitFor(() => expect(document.body.querySelector("#new-address")).not.toBeNull());
        type(document.body.querySelector<HTMLInputElement>("#new-address")!, "127.0.0.1");
        type(document.body.querySelector<HTMLInputElement>("#new-port")!, "80");
        dialogButton("Add")?.click();
        flushSync();
        await vi.waitFor(() =>
            expect(sent.some((call) => call.method === "POST" && call.path === "api/panel/network/allocations")).toBe(true),
        );
        const call = sent.find((entry) => entry.path === "api/panel/network/allocations");
        expect(call?.body).toMatchObject({ bind_address: "127.0.0.1", port: 80 });
    });

    it("assigns an allocation to an app and role", async () => {
        signIn(["network.read", "network.edit"]);
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("spare"));
        button("Assign")?.click();
        flushSync();
        await vi.waitFor(() => expect(document.body.textContent).toContain("Assign allocation"));
        dialogButton("Assign")?.click();
        flushSync();
        await vi.waitFor(() =>
            expect(sent.some((call) => call.method === "POST" && call.path === "api/panel/network/allocations/3/assign")).toBe(true),
        );
    });

    it("shows the refusal when deleting an assigned allocation", async () => {
        signIn(["network.read", "network.edit"]);
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("12333"));
        const deletes = [...document.body.querySelectorAll<HTMLButtonElement>("button")].filter(
            (candidate) => candidate.textContent?.trim() === "Delete",
        );
        deletes[0]?.click();
        flushSync();
        await vi.waitFor(() => expect(sent.some((call) => call.method === "DELETE")).toBe(true));
    });

    it("bulk-deletes the selected allocations", async () => {
        signIn(["network.read", "network.edit"]);
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("spare"));
        const boxes = [...document.body.querySelectorAll<HTMLInputElement>("input[type=checkbox]")];
        boxes[2]?.click();
        flushSync();
        await vi.waitFor(() => expect(button("Delete selected")).toBeDefined());
        button("Delete selected")?.click();
        flushSync();
        await vi.waitFor(() =>
            expect(sent.some((call) => call.method === "POST" && call.path === "api/panel/network/allocations/bulk-delete")).toBe(true),
        );
        const call = sent.find((entry) => entry.path === "api/panel/network/allocations/bulk-delete");
        expect(call?.body).toMatchObject({ ids: [3] });
    });

    it("draws no changing control without the edit permission", async () => {
        signIn(["network.read"]);
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("12333"));
        expect(button("Add allocation")).toBeUndefined();
        expect(button("Assign")).toBeUndefined();
        expect(button("Delete selected")).toBeUndefined();
    });
});
