/*
 * Project Ambrose by Imjustchico
 * Tests the operator's own two-factor page in a real browser against a stubbed panel: it says whether two-factor sign-in is on and how many recovery codes are left, turning it off keeps its button off until the password and a current code are both there, sends them, says the other sessions have ended and reads the state again, a refused turn-off says so and changes nothing, new recovery codes are shown once and are gone from the page when their dialog closes, moving to another app takes a code from the app in use as well as one from the new app, and an operator the panel's requirement covers is offered no way to turn it off.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { session } from "$lib/api.svelte";
import TwoFactor from "./TwoFactor.svelte";

type Sent = { method: string; url: string; body: string };
type Reply = { status: number; body: unknown };

const operator = {
    id: 1,
    username: "merle",
    display_name: "Merle",
    owner: true,
    role: "owner",
    permissions: ["status.read"],
    grants: {},
    must_change_password: false,
    two_factor: true,
    two_factor_required: false,
};
const on = {
    enabled: true,
    pending: false,
    required: false,
    recovery_codes_left: 8,
    enabled_epoch_ms: 1790000000000,
    window_steps: 1,
    issuer: "Ambrose",
};
const codes = [
    "7K2QM-XR4TD",
    "ABCDE-FGHJK",
    "MNPQR-STVWX",
    "YZ012-34567",
    "89ABC-DEFGH",
    "JKMNP-QRSTV",
    "WXYZ0-12345",
    "6789A-BCDEF",
    "GHJKM-NPQRS",
    "TVWXY-Z0123",
];

let sent: Sent[] = [];
let replies: Map<string, Reply[]> = new Map();
let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

function settle() {
    return new Promise((done) => setTimeout(done, 30));
}

function reply(method: string, url: string, status: number, body: unknown) {
    const key = `${method} ${url}`;
    replies.set(key, [...(replies.get(key) ?? []), { status, body }]);
}

function fill(id: string, value: string) {
    const input = document.querySelector<HTMLInputElement>(id);
    if (!input) throw new Error(`no ${id} field`);
    input.value = value;
    input.dispatchEvent(new Event("input", { bubbles: true }));
    flushSync();
}

function button(label: string): HTMLButtonElement | undefined {
    return [...document.querySelectorAll<HTMLButtonElement>("button")].find((found) => found.textContent?.trim() === label);
}

async function visit() {
    page = mount(TwoFactor, { target: host });
    flushSync();
    await vi.waitFor(() => expect(host.textContent).toContain("Your account"));
}

beforeEach(() => {
    sent = [];
    replies = new Map();
    session.state = "signed-in";
    session.panel = true;
    session.csrf = "csrf-token";
    session.user = { ...operator };
    session.mustEnroll = false;
    vi.stubGlobal("fetch", async (url: string, init?: RequestInit) => {
        const method = init?.method ?? "GET";
        sent.push({ method, url: String(url), body: typeof init?.body === "string" ? init.body : "" });
        const queued = replies.get(`${method} ${String(url)}`) ?? [];
        const next = queued.length > 1 ? queued.shift() : queued[0];
        const answer = next ?? { status: 404, body: { error: "not_found", message: "Nothing here" } };
        return new Response(JSON.stringify(answer.body), {
            status: answer.status,
            headers: { "Content-Type": "application/json", "X-Request-Id": "req" },
        });
    });
    host = document.createElement("div");
    document.body.append(host);
});

afterEach(() => {
    if (page) unmount(page);
    page = null;
    host.remove();
    vi.unstubAllGlobals();
    session.user = null;
});

describe("the two-factor page", () => {
    it("says two-factor sign-in is on and how many recovery codes are left", async () => {
        reply("GET", "api/panel/me/two-factor", 200, on);
        await visit();
        expect(host.textContent).toContain("On");
        expect(host.textContent).toContain("8 of 10 left");
    });

    it("turning two-factor off asks for the password and a current code", async () => {
        reply("GET", "api/panel/me/two-factor", 200, on);
        reply("GET", "api/panel/me/two-factor", 200, { ...on, enabled: false, recovery_codes_left: 0, enabled_epoch_ms: null });
        reply("POST", "api/panel/me/two-factor/disable", 200, { user: { ...operator, two_factor: false } });
        await visit();

        const off = () => button("Turn off two-factor sign-in");
        expect(off()?.disabled).toBe(true);
        fill("#turn-off-password", "a long passphrase for merle");
        expect(off()?.disabled, "a password alone does not turn it off").toBe(true);
        expect(host.textContent).toContain("Enter the six digits your authenticator app shows.");
        fill("#turn-off-code", "12345");
        expect(off()?.disabled).toBe(true);
        fill("#turn-off-code", "123 456");
        expect(off()?.disabled).toBe(false);

        off()?.click();
        await settle();
        flushSync();
        const disabled = sent.find((request) => request.url === "api/panel/me/two-factor/disable");
        expect(disabled?.method).toBe("POST");
        expect(JSON.parse(disabled?.body ?? "{}")).toEqual({ password: "a long passphrase for merle", code: "123456" });
        await vi.waitFor(() => expect(host.textContent).toContain("every other session you had has ended"));
        expect(session.user?.two_factor).toBe(false);
        expect(host.querySelector("#turn-off-password"), "once it is off there is nothing left to turn off").toBeNull();
    });

    it("a refused turn-off says so and changes nothing", async () => {
        reply("GET", "api/panel/me/two-factor", 200, on);
        reply("POST", "api/panel/me/two-factor/disable", 403, {
            error: "check_refused",
            message: "That password and code do not confirm it is you",
        });
        await visit();
        fill("#turn-off-password", "not the password");
        fill("#turn-off-code", "123456");
        button("Turn off two-factor sign-in")?.click();
        await settle();
        flushSync();
        expect(host.querySelector("[role=alert]")?.textContent).toContain("That password and code do not confirm it is you.");
        expect(session.user?.two_factor).toBe(true);
        expect(host.querySelector("#turn-off-password")).not.toBeNull();
    });

    it("regenerating shows the new codes once", async () => {
        reply("GET", "api/panel/me/two-factor", 200, on);
        reply("GET", "api/panel/me/two-factor", 200, { ...on, recovery_codes_left: 10 });
        reply("POST", "api/panel/me/two-factor/recovery-codes", 200, { recovery_codes: codes, recovery_codes_left: 10 });
        await visit();
        fill("#fresh-codes-password", "a long passphrase for merle");
        fill("#fresh-codes-code", "654321");
        button("Make new recovery codes")?.click();
        await settle();
        flushSync();
        await vi.waitFor(() => expect(document.body.textContent).toContain("Save your recovery codes"));
        const asked = sent.find((request) => request.url === "api/panel/me/two-factor/recovery-codes");
        expect(JSON.parse(asked?.body ?? "{}")).toEqual({ password: "a long passphrase for merle", code: "654321" });
        for (const code of codes) expect(document.body.textContent).toContain(code);

        document.querySelector<HTMLButtonElement>("#recovery-codes-stored")?.click();
        flushSync();
        button("Done")?.click();
        await settle();
        flushSync();
        await vi.waitFor(() => expect(document.body.textContent).not.toContain("Save your recovery codes"));
        const markup = document.documentElement.outerHTML;
        for (const code of codes) expect(markup).not.toContain(code);
        await vi.waitFor(() => expect(host.textContent).toContain("10 of 10 left"));
    });

    it("moving to another app asks for a code from the app in use as well as one from the new app", async () => {
        reply("GET", "api/panel/me/two-factor", 200, on);
        reply("POST", "api/panel/me/two-factor/setup", 200, {
            secret: "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ",
            uri: "otpauth://totp/Ambrose:merle?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ&issuer=Ambrose",
            issuer: "Ambrose",
            account: "merle",
            algorithm: "SHA1",
            digits: 6,
            period: 30,
            fresh: true,
        });
        reply("POST", "api/panel/me/two-factor/enable", 200, { recovery_codes: codes, recovery_codes_left: 10, user: { ...operator } });
        await visit();
        button("Move to another app")?.click();
        flushSync();
        await vi.waitFor(() => expect(host.querySelector("#two-factor-current")).not.toBeNull());
        expect(JSON.parse(sent.find((request) => request.url === "api/panel/me/two-factor/setup")?.body ?? "{}")).toEqual({
            replace: true,
        });

        const turnOn = () => button("Turn on two-factor sign-in");
        fill("#two-factor-password", "a long passphrase for merle");
        fill("#two-factor-code", "654321");
        expect(turnOn()?.disabled, "a code from the new app alone does not move it").toBe(true);
        expect(host.textContent).toContain("Enter the six digits the app you use now shows.");
        fill("#two-factor-current", "123 456");
        expect(turnOn()?.disabled).toBe(false);

        turnOn()?.click();
        await settle();
        flushSync();
        const enabled = sent.find((request) => request.url === "api/panel/me/two-factor/enable");
        expect(JSON.parse(enabled?.body ?? "{}")).toEqual({
            password: "a long passphrase for merle",
            code: "654321",
            current_code: "123456",
        });
        await vi.waitFor(() => expect(document.body.textContent).toContain("Save your recovery codes"));
    });

    it("offers no way to turn it off while the panel requires it", async () => {
        reply("GET", "api/panel/me/two-factor", 200, { ...on, required: true });
        await visit();
        expect(host.textContent).toContain("Required by this panel");
        expect(host.textContent).toContain("so it stays on");
        expect(host.querySelector("#turn-off-password")).toBeNull();
    });
});
