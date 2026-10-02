/*
 * Project Ambrose by Imjustchico
 * Tests the activity page in a browser: a sentence carrying markup is shown as text and never parsed, a hidden address says so, the result filter and the older-rows cursor reach the server as query values, the export links carry the same filters, and an operator without activity.read is offered only their own activity.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { session } from "$lib/api.svelte";
import type { PanelActivityRow } from "$lib/schemas";
import Activity from "./Activity.svelte";

const rows: PanelActivityRow[] = [
    {
        id: 12,
        event_id: "event-12",
        epoch_ms: Date.now() - 120000,
        name: "app:power.restart",
        sentence: "<b>owner</b> restarted gameserver",
        class: "security",
        actor: { type: "user", id: "1", name: "<b>owner</b>" },
        api_key_id: null,
        scheduled: false,
        address_shown: false,
        address: null,
        user_agent: null,
        node: null,
        result: "denied",
        error: null,
        reason: "=SUM(A1)",
        properties: { method: "POST" },
        subjects: [{ kind: "app", id: "gameserver", name: "gameserver" }],
    },
];

const sent: string[] = [];
let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;
let permissions: string[];

function answer(body: unknown): Response {
    return new Response(JSON.stringify(body), { status: 200, headers: { "Content-Type": "application/json" } });
}

function render() {
    session.panel = true;
    session.via = "session";
    session.state = "signed-in";
    session.csrf = "csrf-token";
    session.user = {
        id: 1,
        username: "owner",
        display_name: "Owner",
        owner: true,
        role: "owner",
        permissions,
        grants: {},
        must_change_password: false,
        two_factor: false,
        two_factor_required: false,
    };
    host = document.createElement("div");
    document.body.append(host);
    page = mount(Activity, { target: host });
    flushSync();
}

beforeEach(() => {
    sent.length = 0;
    permissions = ["activity.read", "activity.export"];
    vi.stubGlobal("fetch", (path: string) => {
        sent.push(path);
        if (path.startsWith("api/panel/activity") || path.startsWith("api/panel/me/activity")) {
            const older = path.includes("cursor=");
            return Promise.resolve(
                answer({
                    schema: 1,
                    rows: older ? [{ ...rows[0], id: 3, event_id: "event-3", sentence: "an older row" }] : rows,
                    next_cursor: older ? null : "12",
                    sees_addresses: false,
                    can_export: true,
                }),
            );
        }
        return Promise.resolve(answer({}));
    });
});

afterEach(() => {
    if (page) unmount(page);
    page = null;
    host.remove();
    vi.restoreAllMocks();
    vi.unstubAllGlobals();
});

describe("the activity page", () => {
    it("shows a sentence as text, hides an address and pages with the server's cursor", async () => {
        render();
        await vi.waitFor(() => expect(host.textContent).toContain("<b>owner</b> restarted gameserver"));
        expect(host.querySelector(".activity-sentence b")).toBeNull();
        expect(host.textContent).toContain("Hidden");
        expect(host.textContent).toContain("Denied");

        const older = [...host.querySelectorAll("button")].find((button) => button.textContent?.includes("Show older"));
        older?.click();
        await vi.waitFor(() => expect(host.textContent).toContain("an older row"));
        expect(sent.some((path) => path.startsWith("api/panel/activity?") && path.includes("cursor=12"))).toBe(true);
    });

    it("sends the result filter and carries the filters into the export links", async () => {
        render();
        await vi.waitFor(() => expect(sent.length).toBeGreaterThan(0));
        const result = host.querySelector<HTMLSelectElement>("#activity-result");
        expect(result).not.toBeNull();
        if (!result) return;
        result.value = "denied";
        result.dispatchEvent(new Event("change"));
        flushSync();
        await vi.waitFor(() => expect(sent.some((path) => path.includes("result=denied"))).toBe(true));
        await vi.waitFor(() => {
            const csv = [...host.querySelectorAll("a")].find((link) => link.textContent?.includes("Export CSV"));
            expect(csv?.getAttribute("href")).toContain("api/panel/activity/export?");
            expect(csv?.getAttribute("href")).toContain("result=denied");
            expect(csv?.getAttribute("href")).toContain("format=csv");
        });
    });

    it("offers an operator without activity.read only their own activity", async () => {
        permissions = [];
        render();
        await vi.waitFor(() => expect(sent.some((path) => path.startsWith("api/panel/me/activity"))).toBe(true));
        const options = [...(host.querySelector<HTMLSelectElement>("#activity-scope")?.options ?? [])].map((option) => option.value);
        expect(options).toEqual(["me"]);
        expect(sent.some((path) => path.startsWith("api/panel/activity"))).toBe(false);
    });
});
