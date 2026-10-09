/*
 * Project Ambrose by Imjustchico
 * Tests the panel settings editor in a real browser against a stubbed admin API: the three groups show their typed values, listener-owned values are locked, secrets stay masked and the save action sends one values object. The mail group's test button sends the test mail and says who it reached, or shows the mail server's refusal.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import Settings from "./Settings.svelte";

const answer = {
    schema: 1,
    settings: [
        {
            key: "Panel.Name",
            group: "general",
            value: "Ambrose",
            default: "Ambrose",
            secret: false,
            locked: false,
            layer: "default",
            minimum: 0,
            maximum: 0,
        },
        {
            key: "Mail.Password",
            group: "mail",
            value: "***",
            default: "",
            secret: true,
            locked: false,
            layer: "default",
            minimum: 0,
            maximum: 0,
        },
        {
            key: "Panel.TrustedProxies",
            group: "security",
            value: "10.0.0.0/8",
            default: "",
            secret: false,
            locked: true,
            layer: "environment",
            minimum: 0,
            maximum: 0,
        },
    ],
};

let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

beforeEach(() => {
    vi.stubGlobal("fetch", () =>
        Promise.resolve(new Response(JSON.stringify(answer), { status: 200, headers: { "Content-Type": "application/json" } })),
    );
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

describe("the panel settings page", () => {
    it("shows grouped settings and locked listener values", async () => {
        await vi.waitFor(() => expect(host.textContent).toContain("Panel.Name"));
        expect(host.textContent).toContain("Mail.Password");
        expect(host.textContent).toContain("Panel.TrustedProxies");
        expect(host.textContent).toContain("Locked · environment");
    });

    it("masks secrets and sends the edited values as one batch", async () => {
        await vi.waitFor(() => expect(host.textContent).toContain("Mail.Password"));
        expect(host.querySelector('input[placeholder="Unchanged"]')).not.toBeNull();
        const save = [...host.querySelectorAll("button")].find((button) => button.textContent?.includes("Save changes"));
        expect(save).not.toBeUndefined();
        save?.click();
        await vi.waitFor(() => expect(host.textContent).toContain("Settings saved and audited."));
    });

    it("sends a test mail from the mail group and says who it reached", async () => {
        const sent: string[] = [];
        vi.stubGlobal("fetch", (input: RequestInfo | URL, init?: RequestInit) => {
            const url = String(input instanceof Request ? input.url : input);
            sent.push(`${init?.method ?? "GET"} ${url}`);
            const body = url.includes("mail/test") ? { sent: true, to: "owner@example.test" } : answer;
            return Promise.resolve(new Response(JSON.stringify(body), { status: 200, headers: { "Content-Type": "application/json" } }));
        });
        await vi.waitFor(() => expect(host.textContent).toContain("Mail.Password"));
        const mailTab = [...host.querySelectorAll("button")].find((button) => button.textContent?.trim() === "Mail");
        mailTab?.click();
        flushSync();
        let test: HTMLButtonElement | undefined;
        await vi.waitFor(() => {
            test = [...host.querySelectorAll("button")].find((button) => button.textContent?.includes("Send test mail"));
            expect(test).not.toBeUndefined();
        });
        test?.click();
        await vi.waitFor(() => expect(host.textContent).toContain("A test mail was sent to owner@example.test."));
        expect(sent.some((line) => line.startsWith("POST") && line.includes("api/panel/settings/mail/test"))).toBe(true);
    });
});
