/*
 * Project Ambrose by Imjustchico
 * Tests the files page in a real browser against a stubbed file API: it walks into a folder and back out by its breadcrumbs, a selection of the whole filtered set shows the server's count rather than the rows on screen, on a read-only root every control that would change a file is drawn disabled with the root's policy named beside it, a client-derived file cannot be viewed and says why, a configuration file shows how many secrets are hidden and offers to show them only to a caller who may see them, and a server with no file roots says so.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { page as browser } from "vitest/browser";
import { session } from "$lib/api.svelte";
import Files from "./Files.svelte";

const InstallPolicy = "The install root is the running build; it changes only through updates";

type Row = { name: string; kind: string; size: number; rule?: { effect: string; pattern: string; why: string } | null };

function operations(refused: string[], reason: string) {
    const all = [
        "list",
        "read",
        "preview",
        "download",
        "archive",
        "share",
        "write",
        "upload",
        "create",
        "rename",
        "move",
        "copy",
        "delete",
        "permissions",
        "extract",
        "truncate",
        "sftp",
        "pull",
    ];
    return Object.fromEntries(
        all.map((name) => [
            name,
            refused.includes(name) ? { allowed: false, code: "refused_by_policy", reason, rule: null } : { allowed: true },
        ]),
    );
}

const changes = ["write", "upload", "create", "rename", "move", "copy", "delete", "permissions", "extract", "truncate", "pull"];
const installPolicy = {
    summary: "Read-only: the running build, which changes only through updates",
    client_derived: false,
    read_only: true,
    operations: operations(changes, InstallPolicy),
};
const logsPolicy = {
    summary: "Logs: read, download, truncate or trash them; the apps write them",
    client_derived: false,
    read_only: false,
    operations: operations([], ""),
};
const configPolicy = {
    summary: "Writable configuration files",
    client_derived: false,
    read_only: false,
    operations: operations(["download"], "A configuration file holds secrets"),
};

function root(id: string, label: string, policy: typeof installPolicy, apps: string[] = []) {
    return {
        id,
        label,
        kind: id,
        apps,
        present: true,
        problem: null,
        client_derived: false,
        read_only: policy.read_only,
        policy,
        volume: { name: "/srv", free: 50 * 2 ** 30, total: 100 * 2 ** 30, minimum: 2 ** 30, reserved: 0 },
        rules: [],
    };
}

let roots: ReturnType<typeof root>[] = [];
let folders: Record<string, Record<string, Row[]>> = {};
let requests: string[] = [];
let content: unknown = null;
let filesMissing = false;

function answer(body: unknown, code = 200): Response {
    return new Response(JSON.stringify(body), { status: code, headers: { "Content-Type": "application/json" } });
}

function listing(id: string, query: URLSearchParams) {
    const path = query.get("path") ?? "";
    const filter = (query.get("q") ?? "").toLowerCase();
    const policy = roots.find((one) => one.id === id)?.policy ?? logsPolicy;
    const every = (folders[id]?.[path] ?? []).filter((row) => row.name.toLowerCase().includes(filter));
    const offset = Number(query.get("offset") ?? "0");
    const limit = Number(query.get("limit") ?? "100");
    const shown = every.slice(offset, offset + limit).map((row) => ({
        name: row.name,
        kind: row.kind,
        size: row.size,
        modified_ms: 1790000000000,
        openable: true,
        problem: null,
        rule: row.rule ?? null,
    }));
    return {
        schema: 1,
        root: id,
        path,
        modified_ms: 1790000000000,
        policy,
        rule: null,
        entries: shown,
        total: every.length,
        offset,
        limit,
        truncated: false,
        sort: query.get("sort") ?? "name",
        order: query.get("order") ?? "asc",
        filter,
    };
}

let view: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

function open() {
    host = document.createElement("div");
    document.body.append(host);
    view = mount(Files, { target: host });
    flushSync();
}

function signIn(permissions: string[]) {
    session.state = "signed-in";
    session.user = {
        id: 1,
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

function buttonNamed(label: string): HTMLButtonElement | undefined {
    return [...host.querySelectorAll<HTMLButtonElement>("button")].find((button) => button.textContent?.trim() === label);
}

function reasonOf(element: Element | undefined): string {
    const id = element?.getAttribute("aria-describedby");
    return id ? (document.getElementById(id)?.textContent ?? "") : "";
}

beforeEach(async () => {
    await browser.viewport(1280, 900);
    history.replaceState(history.state, "", "#files");
    requests = [];
    filesMissing = false;
    content = null;
    roots = [
        root("install", "Install", installPolicy),
        root("logs", "Logs", logsPolicy, ["supervisor", "gameserver"]),
        root("config", "Configuration", configPolicy, ["supervisor"]),
    ];
    folders = {
        install: {
            "": [
                { name: "bin", kind: "folder", size: 0 },
                { name: "readme.txt", kind: "file", size: 12 },
            ],
            bin: [{ name: "supervisor.exe", kind: "file", size: 4096 }],
        },
        logs: {
            "": [
                ...Array.from({ length: 250 }, (_, index) => ({
                    name: `game-${String(index).padStart(3, "0")}.log`,
                    kind: "file",
                    size: index,
                })),
                ...Array.from({ length: 30 }, (_, index) => ({ name: `note-${index}.txt`, kind: "file", size: index })),
            ],
        },
        config: { "": [{ name: "supervisor.conf", kind: "file", size: 90 }] },
    };
    vi.stubGlobal("fetch", (path: string) => {
        requests.push(path);
        const [route, rest] = path.split("?");
        const query = new URLSearchParams(rest ?? "");
        if (route === "api/files")
            return Promise.resolve(
                filesMissing
                    ? answer({ error: "not_found", message: "nothing at /api/files" }, 404)
                    : answer({
                          schema: 1,
                          roots,
                          apps: [
                              { name: "supervisor", program: "supervisor" },
                              { name: "gameserver", program: "gameserver" },
                          ],
                          notes: [],
                      }),
            );
        const matched = /^api\/files\/([a-z-]+)\/(list|content|rules)$/.exec(route);
        if (matched && matched[2] === "list") return Promise.resolve(answer(listing(matched[1], query)));
        if (matched && matched[2] === "content")
            return Promise.resolve(
                content === null ? answer({ error: "not_found", message: "Nothing is at this path" }, 404) : answer(content),
            );
        return Promise.resolve(answer({ error: "not_found", message: `nothing at ${path}` }, 404));
    });
    signIn(["files.list", "files.read", "files.download", "files.write", "files.upload", "files.delete", "files.archive"]);
});

afterEach(() => {
    if (view) unmount(view);
    view = null;
    host.remove();
    session.user = null;
    vi.unstubAllGlobals();
    history.replaceState(history.state, "", "#");
});

describe("the files page", () => {
    it("walks into a folder and back out by its breadcrumbs", async () => {
        open();
        await vi.waitFor(() => expect(buttonNamed("bin")).toBeDefined());
        expect(host.textContent).toContain("readme.txt");
        buttonNamed("bin")?.click();
        await vi.waitFor(() => expect(host.textContent).toContain("supervisor.exe"));
        const crumbs = host.querySelector("nav[aria-label='breadcrumb']") ?? host.querySelector("[data-slot=breadcrumb]");
        expect(crumbs?.textContent).toContain("install");
        expect(crumbs?.textContent).toContain("bin");
        expect(window.location.hash).toBe("#files?root=install&path=bin");
        const back = [...host.querySelectorAll<HTMLAnchorElement>("[data-slot=breadcrumb-link]")].find(
            (link) => link.textContent?.trim() === "install",
        );
        expect(back).toBeDefined();
        back?.click();
        await vi.waitFor(() => expect(host.textContent).toContain("readme.txt"));
        expect(host.textContent).not.toContain("supervisor.exe");
        expect(requests.some((request) => request.includes("path=bin"))).toBe(true);
    });

    it("selects the whole filtered set and shows its count rather than the rows on screen", async () => {
        history.replaceState(history.state, "", "#files?root=logs");
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("game-000.log"));
        const filter = host.querySelector<HTMLInputElement>("input[aria-label='Filter by name']");
        if (!filter) throw new Error("the page has no filter");
        filter.value = "game";
        filter.dispatchEvent(new Event("input", { bubbles: true }));
        await vi.waitFor(() => expect(host.querySelector("[data-testid=page-range]")?.textContent).toContain("of 250"));
        expect(host.textContent).not.toContain("note-1.txt");
        const all = host.querySelector<HTMLInputElement>("input[aria-label='Select all on this page']");
        all?.click();
        await vi.waitFor(() => expect(host.querySelector("[data-testid=selection-count]")?.textContent).toBe("100 selected"));
        const every = buttonNamed("Select all 250 matching");
        expect(every).toBeDefined();
        every?.click();
        await vi.waitFor(() =>
            expect(host.querySelector("[data-testid=selection-count]")?.textContent).toBe('All 250 matching "game" selected'),
        );
        expect(host.querySelectorAll<HTMLInputElement>("tbody input[type=checkbox]:checked").length).toBe(100);
    });

    it("disables every write control on a read-only root and names the root's policy", async () => {
        open();
        await vi.waitFor(() => expect(buttonNamed("bin")).toBeDefined());
        expect(host.textContent).toContain(installPolicy.summary);
        for (const label of ["Upload", "New folder", "New file"]) {
            const control = buttonNamed(label);
            expect(control, label).toBeDefined();
            expect(control?.disabled, label).toBe(true);
            expect(reasonOf(control), label).toBe(InstallPolicy);
        }
        host.querySelector<HTMLInputElement>("input[aria-label='Select readme.txt']")?.click();
        await vi.waitFor(() => expect(host.querySelector("[data-testid=selection-count]")?.textContent).toBe("1 selected"));
        for (const label of ["Move", "Copy", "Delete"]) {
            const control = buttonNamed(label);
            expect(control?.disabled, label).toBe(true);
            expect(reasonOf(control), label).toBe(InstallPolicy);
        }
        expect(buttonNamed("Copy paths")?.disabled).toBe(false);
    });

    it("says a client-derived file cannot be viewed, and why", async () => {
        folders.install[""].push({
            name: "types.json",
            kind: "file",
            size: 10,
            rule: {
                effect: "client_derived",
                pattern: "*",
                why: "Built from your own Wizard101 install, so it is listed but never handed to a browser",
            },
        });
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("types.json"));
        expect(buttonNamed("types.json")).toBeUndefined();
        expect(host.textContent).toContain("Client-derived");
    });

    it("shows how many secrets are hidden and offers them only to a caller who may see them", async () => {
        history.replaceState(history.state, "", "#files?root=config");
        content = {
            schema: 1,
            root: "config",
            path: "supervisor.conf",
            name: "supervisor.conf",
            size: 90,
            modified_ms: 1790000000000,
            etag: '"abc-1"',
            offset: 0,
            length: 60,
            next_offset: null,
            eof: true,
            binary: false,
            bom: false,
            text: "Admin.Token = ***\nSupervisor.Apps = gameserver\n",
            redacted: true,
            redacted_keys: ["Admin.Token"],
            revealed: false,
            revealed_keys: [],
        };
        open();
        await vi.waitFor(() => expect(buttonNamed("supervisor.conf")).toBeDefined());
        buttonNamed("supervisor.conf")?.click();
        await vi.waitFor(() => expect(host.querySelector("[data-testid=file-text]")?.textContent).toContain("Admin.Token = ***"));
        expect(host.textContent).toContain("1 secret value is hidden: Admin.Token");
        expect(buttonNamed("Show secrets")).toBeUndefined();
        expect(requests.some((request) => request.includes("path=supervisor.conf"))).toBe(true);
    });

    it("says so when the server that served it keeps no file roots", async () => {
        filesMissing = true;
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("No file roots here"));
    });
});
