/*
 * Project Ambrose by Imjustchico
 * Tests the configuration page in a real browser against a stubbed admin API: live settings show by category with their value, default and layer, a lock with the layer that holds it, and secret and restricted marks; a value out of bounds or of the wrong type cannot be sent, while a refusal from the server is shown with its message; a change goes through a review step with a reason; a revert from the history sends the value the change replaced with a reason naming it; an imported preset with one value out of bounds shows the refusal in its diff and offers no way to apply it, one the app refuses in its dry run shows the app's own message, and one whose dry run could not be made cannot be applied until it is checked again, after which only the entries that change are sent in one batch; an export takes the keys chosen, from any category whatever the search shows; a secret's reveal control is offered only to a caller allowed to see secrets and asks the app for that one key, a secret is typed into a masked input, a fresh read hides a revealed secret again, and a long revealed value leaves the row's buttons inside the card on a wide screen; the config file's own options show their shipped default, layer and file with a secret masked, and only those that need a restart are marked, with why; and a search narrows every list.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { page as browser } from "vitest/browser";
import { session } from "$lib/api.svelte";
import { live } from "$lib/status.svelte";
import type { Status } from "$lib/schemas";
import Config from "./Config.svelte";
import "../app.css";

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

function declared(key: string, type: string, value: string, extra: Record<string, unknown> = {}) {
    return {
        key,
        value,
        layer: "config",
        file: "/srv/gameserver.conf",
        line: 3,
        default: value,
        default_file: "/srv/gameserver.conf.dist",
        secret: false,
        restart_reason: null,
        declared: true,
        origin: "gameserver.conf line 3",
        type,
        declared_default: value,
        min: null,
        max: null,
        bounds: "",
        unit: "",
        category: "World",
        description: `What ${key} does`,
        apply: "live",
        lock: null,
        visibility: "normal",
        edit: "normal",
        persisted: null,
        revealed: false,
        ...extra,
    };
}

const settings = [
    declared("World.UpdateInterval", "unsigned", "100", {
        min: "1",
        max: "10000",
        bounds: "from 1 to 10000 ms",
        unit: "ms",
        layer: "live",
        persisted: "100",
        declared_default: "50",
    }),
    declared("Rate.Drop.Item", "float", "1", { min: "0", max: "100", bounds: "from 0 to 100 times", unit: "times", category: "Rates" }),
    declared("Zone.UnloadDelay", "unsigned", "90", {
        category: "Zones",
        layer: "override",
        lock: { layer: "override", origin: "a command-line override" },
    }),
    declared("Account.VerifierKeys", "string", "1:***", {
        category: "Accounts",
        secret: true,
        visibility: "secret",
        edit: "restricted",
        apply: "next_use",
    }),
    {
        key: "LogsDir",
        value: "logs",
        layer: "config",
        file: "/srv/gameserver.conf",
        line: 9,
        default: "logs.dist",
        default_file: "/srv/gameserver.conf.dist",
        secret: false,
        restart_reason: null,
        declared: false,
    },
    {
        key: "LoginDatabaseInfo",
        value: "127.0.0.1;3306;ambrose;***;ambrose_login",
        layer: "environment",
        file: "AMBROSE_LOGIN_DATABASE_INFO",
        line: 0,
        default: "",
        default_file: "/srv/gameserver.conf.dist",
        secret: true,
        restart_reason: null,
        declared: false,
    },
    {
        key: "Setup.Mode",
        value: "auto",
        layer: "config",
        file: "/srv/gameserver.conf",
        line: 12,
        default: "auto",
        default_file: "/srv/gameserver.conf.dist",
        secret: false,
        restart_reason: "Setup runs only while the server starts",
        declared: false,
    },
];

const sent: { method: string; path: string; body: unknown }[] = [];
let putAnswer: { status: number; body: unknown } = { status: 200, body: {} };
let dryRunAnswer: { status: number; body: unknown } = { status: 200, body: {} };
let revealedValue = "1:abcdef";
let latestEvent = 0;

function answer(body: unknown, code = 200): Response {
    return new Response(JSON.stringify(body), { status: code, headers: { "Content-Type": "application/json" } });
}

function route(path: string, method: string, body: unknown): Response {
    if (path.startsWith("api/events/after/")) {
        const after = Number(path.slice("api/events/after/".length));
        const records =
            latestEvent > after
                ? [
                      {
                          type: "event",
                          sequence: latestEvent,
                          epoch_ms: 1790000000000,
                          kind: "setting.changed",
                          subject: "Account.VerifierKeys",
                          data: {},
                      },
                  ]
                : [];
        return answer({ schema: 1, oldest: 0, latest: latestEvent, dropped: null, records });
    }
    if (path === "api/settings?reveal=Account.VerifierKeys")
        return answer({
            schema: 2,
            file: "/srv/gameserver.conf",
            revealed: true,
            revealed_keys: ["Account.VerifierKeys"],
            settings: settings.map((setting) =>
                setting.key === "Account.VerifierKeys" ? { ...setting, value: revealedValue, revealed: true } : setting,
            ),
        });
    if (path === "api/settings") return answer({ schema: 2, file: "/srv/gameserver.conf", revealed: false, revealed_keys: [], settings });
    if (path === "api/settings/World.UpdateInterval/history")
        return answer({
            schema: 1,
            key: "World.UpdateInterval",
            visibility: "normal",
            entries: [
                {
                    id: 3,
                    old: "50",
                    new: "100",
                    who: "Merle",
                    account_id: 0,
                    source: "panel",
                    reason: "faster ticks",
                    epoch_seconds: 1790000000,
                },
            ],
        });
    if (path.startsWith("api/settings/") && method === "PUT") return answer(putAnswer.body, putAnswer.status);
    if (path === "api/settings/batch") {
        const request = body as { dry_run?: boolean };
        if (request.dry_run) return answer(dryRunAnswer.body, dryRunAnswer.status);
        return answer({ schema: 1, dry_run: false, changed: [], unchanged: [], message: "applied" });
    }
    return answer({ error: "not_found", message: `nothing at ${path}` }, 404);
}

let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

function open() {
    host = document.createElement("div");
    document.body.append(host);
    page = mount(Config, { target: host });
    flushSync();
}

function button(label: string): HTMLButtonElement | undefined {
    return [...document.body.querySelectorAll<HTMLButtonElement>("button")].find(
        (candidate) => candidate.getAttribute("aria-label") === label || candidate.textContent?.trim() === label,
    );
}

function type(element: HTMLInputElement | HTMLTextAreaElement, text: string) {
    element.value = text;
    element.dispatchEvent(new Event("input", { bubbles: true }));
    flushSync();
}

async function openChange(key: string) {
    await vi.waitFor(() => expect(button(`Change ${key}`)).toBeDefined());
    button(`Change ${key}`)?.click();
    flushSync();
    await vi.waitFor(() => expect(document.body.querySelector("#setting-value")).not.toBeNull());
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

async function importPreset(settingsInFile: Record<string, string>) {
    await vi.waitFor(() => expect(button("Import")).toBeDefined());
    button("Import")?.click();
    flushSync();
    await vi.waitFor(() => expect(document.body.querySelector("#preset-file")).not.toBeNull());
    const input = document.body.querySelector<HTMLInputElement>("#preset-file");
    const data = new DataTransfer();
    data.items.add(
        new File(
            [JSON.stringify({ format: "ambrose-settings-preset", version: 1, app: "gameserver", settings: settingsInFile })],
            "preset.json",
            { type: "application/json" },
        ),
    );
    if (input) {
        input.files = data.files;
        input.dispatchEvent(new Event("change", { bubbles: true }));
    }
}

beforeEach(() => {
    sent.length = 0;
    putAnswer = {
        status: 200,
        body: {
            schema: 1,
            key: "World.UpdateInterval",
            changed: true,
            value: "200",
            layer: "live",
            apply: "live",
            restart_reason: null,
            message: "World.UpdateInterval is now 200",
        },
    };
    dryRunAnswer = { status: 200, body: { schema: 1, dry_run: true, changed: [], unchanged: [], message: "would change" } };
    revealedValue = "1:abcdef";
    latestEvent = 0;
    vi.stubGlobal("fetch", (path: string, options: RequestInit) => {
        const body = options.body ? JSON.parse(String(options.body)) : undefined;
        sent.push({ method: options.method ?? "GET", path, body });
        return Promise.resolve(route(path, options.method ?? "GET", body));
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
    document.body.querySelectorAll("[data-dialog-content], [role=dialog]").forEach((node) => node.remove());
    session.user = null;
    vi.unstubAllGlobals();
});

describe("the configuration page", () => {
    it("shows live settings by category with their value, default, layer, locks and marks", async () => {
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("World.UpdateInterval"));
        for (const category of ["Accounts", "Rates", "World", "Zones"]) expect(host.textContent).toContain(category);
        expect(host.textContent).toContain("Set live · default 50");
        expect(host.textContent).toContain("This app's .conf · default 1");
        expect(host.textContent).toContain("Locked · Command line");
        expect(host.textContent).toContain("Set by a command-line override");
        expect(host.textContent).toContain("Secret");
        expect(host.textContent).toContain("Restricted");
        expect(button("Change Zone.UnloadDelay")).toBeUndefined();
        expect(button("Reset World.UpdateInterval")).toBeDefined();
        expect(button("Reset Rate.Drop.Item")).toBeUndefined();
    });

    it("keeps a value out of bounds or of the wrong type from being sent and shows the server's refusal", async () => {
        open();
        await openChange("World.UpdateInterval");
        const value = document.body.querySelector<HTMLInputElement>("#setting-value");
        const reason = document.body.querySelector<HTMLTextAreaElement>("#setting-reason");
        if (!value || !reason) throw new Error("the change dialog has no inputs");
        type(reason, "faster ticks");
        type(value, "0");
        expect(document.body.textContent).toContain("from 1 to 10000 ms; 0 ms is outside that");
        expect(button("Review")?.disabled).toBe(true);
        type(value, "fast");
        expect(document.body.textContent).toContain("takes a whole number of zero or more");
        expect(button("Review")?.disabled).toBe(true);

        putAnswer = {
            status: 422,
            body: {
                error: "invalid",
                message: "World.UpdateInterval was not changed",
                fields: { value: "a check the server holds refused it" },
            },
        };
        type(value, "100");
        expect(document.body.textContent).toContain("That is the value it already holds");
        expect(button("Review")?.disabled).toBe(true);
        type(value, "200");
        button("Review")?.click();
        flushSync();
        expect(document.body.textContent).toContain("Takes hold at once");
        button("Apply")?.click();
        await vi.waitFor(() =>
            expect(document.body.textContent).toContain("World.UpdateInterval was not changed: a check the server holds refused it"),
        );
        const put = sent.find((request) => request.method === "PUT");
        expect(put?.body).toEqual({ value: "200", reason: "faster ticks" });
        expect(sent.filter((request) => request.method === "PUT").length).toBe(1);
    });

    it("reverts to the value a change in the history replaced, with a reason naming it", async () => {
        putAnswer = {
            status: 200,
            body: {
                schema: 1,
                key: "World.UpdateInterval",
                changed: true,
                value: "50",
                layer: "live",
                apply: "live",
                restart_reason: null,
                message: "World.UpdateInterval is now 50",
            },
        };
        open();
        await vi.waitFor(() => expect(button("History of World.UpdateInterval")).toBeDefined());
        button("History of World.UpdateInterval")?.click();
        flushSync();
        await vi.waitFor(() => expect(document.body.textContent).toContain("faster ticks"));
        expect(document.body.textContent).toContain("Merle");
        expect(document.body.textContent).toContain("50 → 100");
        button("Revert to 50")?.click();
        flushSync();
        await vi.waitFor(() => expect(document.body.querySelector<HTMLInputElement>("#setting-value")?.value).toBe("50"));
        button("Review")?.click();
        flushSync();
        expect(document.body.textContent).toContain("Revert change 3, which was for: faster ticks");
        button("Apply")?.click();
        await vi.waitFor(() => expect(sent.some((request) => request.method === "PUT")).toBe(true));
        const put = sent.find((request) => request.method === "PUT");
        expect(put?.path).toBe("api/settings/World.UpdateInterval");
        expect(put?.body).toEqual({ value: "50", reason: "Revert change 3, which was for: faster ticks" });
        await vi.waitFor(() => expect(host.textContent).toContain("World.UpdateInterval is now 50"));
    });

    it("shows an out-of-bounds preset value in the diff and applies nothing", async () => {
        open();
        await importPreset({ "Rate.Drop.Item": "250", "World.UpdateInterval": "200", "Zone.UnloadDelay": "30" });
        await vi.waitFor(() =>
            expect(document.body.textContent).toContain("Rate.Drop.Item must be from 0 to 100 times; 250 times is outside that"),
        );
        expect(document.body.textContent).toContain("Zone.UnloadDelay is set by a command-line override");
        expect(document.body.textContent).toContain("2 refused");
        expect(document.body.querySelector("#preset-reason")).toBeNull();
        await vi.waitFor(() => expect(sent.some((request) => request.path === "api/settings/batch")).toBe(true));
        const dryRun = sent.find((request) => request.path === "api/settings/batch");
        expect(dryRun?.body).toEqual({ entries: [{ key: "World.UpdateInterval", value: "200" }], dry_run: true });
        const apply = [...document.body.querySelectorAll<HTMLButtonElement>("button")].find((candidate) =>
            candidate.textContent?.startsWith("Apply "),
        );
        expect(apply?.disabled).toBe(true);
        expect(sent.filter((request) => request.path === "api/settings/batch" && !(request.body as { dry_run?: boolean }).dry_run)).toEqual(
            [],
        );
    });

    it("applies a preset only once the app's dry run of it passed, sending only what changes", async () => {
        dryRunAnswer = { status: 503, body: { error: "app_not_running", message: "gameserver is not running" } };
        open();
        await importPreset({ "World.UpdateInterval": "200", "Rate.Drop.Item": "1" });
        await vi.waitFor(() => expect(document.body.textContent).toContain("gameserver is not running"));
        expect(document.body.textContent).toContain("1 change, 1 already held");
        const reason = document.body.querySelector<HTMLTextAreaElement>("#preset-reason");
        if (!reason) throw new Error("the import has no reason field");
        type(reason, "weekend rates");
        const apply = () =>
            [...document.body.querySelectorAll<HTMLButtonElement>("button")].find((candidate) =>
                candidate.textContent?.startsWith("Apply "),
            );
        expect(apply()?.disabled).toBe(true);

        dryRunAnswer = { status: 200, body: { schema: 1, dry_run: true, changed: [], unchanged: [], message: "would change" } };
        button("Check again")?.click();
        await vi.waitFor(() => expect(apply()?.disabled).toBe(false));
        apply()?.click();
        await vi.waitFor(() => expect(host.textContent).toContain("applied"));
        const batches = sent.filter((request) => request.path === "api/settings/batch" && !(request.body as { dry_run?: boolean }).dry_run);
        expect(batches.map((request) => request.body)).toEqual([
            { entries: [{ key: "World.UpdateInterval", value: "200" }], reason: "weekend rates" },
        ]);
    });

    it("exports the keys chosen, from any category whatever the search shows", async () => {
        let exported: Blob | null = null;
        vi.spyOn(URL, "createObjectURL").mockImplementation((blob) => {
            exported = blob as Blob;
            return "blob:preset";
        });
        vi.spyOn(URL, "revokeObjectURL").mockImplementation(() => {});
        vi.spyOn(HTMLAnchorElement.prototype, "click").mockImplementation(() => {});
        open();
        await vi.waitFor(() => expect(button("Export")).toBeDefined());
        const search = document.body.querySelector<HTMLInputElement>("input[aria-label='Search settings']");
        if (!search) throw new Error("the page has no search");
        type(search, "Drop");
        button("Export")?.click();
        flushSync();
        await vi.waitFor(() => expect(document.body.textContent).toContain("Rates (1 of 1)"));
        expect(document.body.textContent).toContain("World (0 of 1)");
        expect(document.body.textContent).toContain("Zones (0 of 1)");
        expect(document.body.textContent).not.toContain("Accounts (");
        button("Choose keys in World")?.click();
        flushSync();
        document.body.querySelector<HTMLInputElement>("#export-key-World\\.UpdateInterval")?.click();
        flushSync();
        button("Export 2 settings")?.click();
        await vi.waitFor(() => expect(exported).not.toBeNull());
        const preset = JSON.parse(await (exported as unknown as Blob).text()) as {
            format: string;
            app: string;
            settings: Record<string, string>;
        };
        expect(preset.format).toBe("ambrose-settings-preset");
        expect(preset.app).toBe("gameserver");
        expect(preset.settings).toEqual({ "Rate.Drop.Item": "1", "World.UpdateInterval": "100" });
        vi.restoreAllMocks();
    });

    it("shows the app's own refusal from the dry run in the diff", async () => {
        dryRunAnswer = {
            status: 422,
            body: {
                error: "invalid",
                message: "None of the 1 settings would change",
                fields: { "World.UpdateInterval": "a check the server holds refused it" },
                errors: [{ key: "World.UpdateInterval", code: "invalid", message: "a check the server holds refused it" }],
            },
        };
        open();
        await importPreset({ "World.UpdateInterval": "200" });
        await vi.waitFor(() => expect(document.body.textContent).toContain("a check the server holds refused it"));
        expect(document.body.textContent).toContain("1 refused");
    });

    it("offers a secret's reveal only to a caller allowed to see secrets, for that one key", async () => {
        signIn(["settings.read", "settings.edit"]);
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("Account.VerifierKeys"));
        expect(button("Reveal Account.VerifierKeys")).toBeUndefined();
        expect(button("Change Account.VerifierKeys")).toBeUndefined();
        if (page) unmount(page);
        host.remove();

        signIn(["settings.read", "settings.edit", "settings.edit.restricted", "settings.secrets.read"]);
        open();
        await vi.waitFor(() => expect(button("Reveal Account.VerifierKeys")).toBeDefined());
        expect(host.textContent).toContain("1:***");
        button("Reveal Account.VerifierKeys")?.click();
        await vi.waitFor(() => expect(host.textContent).toContain("1:abcdef"));
        expect(sent.some((request) => request.path === "api/settings?reveal=Account.VerifierKeys")).toBe(true);
        expect(button("Change Account.VerifierKeys")).toBeDefined();
        await openChange("Account.VerifierKeys");
        const value = document.body.querySelector("#setting-value");
        expect(value?.tagName).toBe("INPUT");
        expect(value?.getAttribute("type")).toBe("password");
    });

    it("hides a revealed secret again when the settings are read afresh", async () => {
        signIn(["settings.read", "settings.secrets.read"]);
        open();
        await vi.waitFor(() => expect(button("Reveal Account.VerifierKeys")).toBeDefined());
        button("Reveal Account.VerifierKeys")?.click();
        await vi.waitFor(() => expect(host.textContent).toContain("1:abcdef"));
        const reads = sent.filter((request) => request.path === "api/settings").length;
        latestEvent = 1;
        await vi.waitFor(() => expect(sent.filter((request) => request.path === "api/settings").length).toBeGreaterThan(reads), {
            timeout: 5000,
        });
        await vi.waitFor(() => expect(host.textContent).not.toContain("1:abcdef"));
        expect(host.textContent).toContain("1:***");
        expect(button("Reveal Account.VerifierKeys")).toBeDefined();
    });

    it("keeps a long revealed value from pushing the row's buttons out of the card on a wide screen", async () => {
        const width = window.innerWidth;
        const height = window.innerHeight;
        await browser.viewport(1280, 800);
        try {
            revealedValue = `1:${"a".repeat(64)},2:${"b".repeat(64)},3:${"c".repeat(64)},4:${"d".repeat(64)}`;
            signIn(["settings.read", "settings.secrets.read"]);
            open();
            await vi.waitFor(() => expect(button("Reveal Account.VerifierKeys")).toBeDefined());
            button("Reveal Account.VerifierKeys")?.click();
            await vi.waitFor(() => expect(button("Hide Account.VerifierKeys")).toBeDefined());
            const hide = button("Hide Account.VerifierKeys");
            const card = hide?.closest("[data-slot=card]");
            if (!hide || !card) throw new Error("the revealed row has no Hide button inside a card");
            const inner = hide.getBoundingClientRect();
            const outer = card.getBoundingClientRect();
            expect(inner.left).toBeGreaterThanOrEqual(outer.left);
            expect(inner.right).toBeLessThanOrEqual(outer.right);
            expect(document.documentElement.scrollWidth).toBeLessThanOrEqual(1280);
        } finally {
            await browser.viewport(width, height);
        }
    });

    it("lists the config file's own options with their shipped default, layer and file, secrets masked, and marks only those that need a restart, with why", async () => {
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("LogsDir"));
        const row = (key: string) =>
            [...host.querySelectorAll("tr")].find((candidate) => candidate.querySelector("td .font-mono")?.textContent?.trim() === key);
        expect(row("LogsDir")?.textContent).toContain("logs.dist");
        expect(row("LogsDir")?.textContent).toContain("/srv/gameserver.conf:9");
        expect(row("LogsDir")?.textContent).toContain("This app's .conf");
        expect(row("LoginDatabaseInfo")?.textContent).toContain("Environment variable");
        expect(row("LoginDatabaseInfo")?.textContent).toContain("AMBROSE_LOGIN_DATABASE_INFO");
        expect(row("LoginDatabaseInfo")?.textContent).toContain("127.0.0.1;3306;ambrose;***;ambrose_login");
        expect(row("LoginDatabaseInfo")?.textContent).toContain("Secret");
        expect(host.textContent).toContain("Takes effect at the next start");
        expect(host.textContent).toContain("Setup runs only while the server starts");
        expect(host.textContent).toContain("4 live settings and 3 other options");
        expect(host.textContent).toContain("1 need a restart");
        const marked = [...host.querySelectorAll("tr")]
            .filter((candidate) => candidate.textContent?.includes("Restart required"))
            .map((candidate) => candidate.querySelector("td .font-mono")?.textContent?.trim());
        expect(marked).toEqual(["Setup.Mode"]);
    });

    it("narrows every list to the settings a search matches", async () => {
        open();
        await vi.waitFor(() => expect(host.textContent).toContain("LogsDir"));
        const search = host.querySelector<HTMLInputElement>("input[aria-label='Search settings']");
        if (!search) throw new Error("the page has no search");
        type(search, "Drop");
        expect(host.querySelector("[data-setting='Rate.Drop.Item']")).not.toBeNull();
        expect(host.querySelector("[data-setting='World.UpdateInterval']")).toBeNull();
        expect(host.textContent).not.toContain("LogsDir");
        type(search, "nothing is called this");
        expect(host.textContent).toContain("No live setting matches");
    });
});
