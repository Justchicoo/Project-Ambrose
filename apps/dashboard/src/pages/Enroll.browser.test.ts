/*
 * Project Ambrose by Imjustchico
 * Tests the required enrollment page in a real browser against a stubbed panel: it shows the setup secret grouped for typing with the link an authenticator app opens, keeps its button off until the password and a six-digit code are there, turns two-factor sign-in on with them, and shows the recovery codes once in a dialog that neither Escape nor a click outside closes and that stays open, with the panel still held at enrollment, until the operator says the codes are saved; once it closes the codes are nowhere in the page or the browser's storage.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { session } from "$lib/api.svelte";
import Enroll from "./Enroll.svelte";

type Sent = { method: string; url: string; body: string };

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
const operator = {
    id: 1,
    username: "merle",
    display_name: "Merle",
    owner: true,
    role: "owner",
    permissions: ["status.read"],
    grants: {},
    must_change_password: false,
    two_factor: false,
    two_factor_required: true,
};
const secret = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";

let sent: Sent[] = [];
let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

function settle() {
    return new Promise((done) => setTimeout(done, 30));
}

function answer(status: number, body: unknown) {
    return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json", "X-Request-Id": "req" } });
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

async function turnOn() {
    await vi.waitFor(() => expect(document.querySelector("#two-factor-code")).not.toBeNull());
    fill("#two-factor-password", "a long passphrase for merle");
    fill("#two-factor-code", "123456");
    button("Turn on two-factor sign-in")?.click();
    await settle();
    flushSync();
    await vi.waitFor(() => expect(document.body.textContent).toContain("Save your recovery codes"));
}

function stored(): string {
    const kept: string[] = [];
    for (const storage of [window.localStorage, window.sessionStorage])
        for (let index = 0; index < storage.length; index += 1) {
            const key = storage.key(index) ?? "";
            kept.push(key, storage.getItem(key) ?? "");
        }
    return kept.join("\n");
}

beforeEach(() => {
    sent = [];
    session.state = "signed-in";
    session.panel = true;
    session.csrf = "csrf-token";
    session.user = { ...operator };
    session.mustEnroll = true;
    vi.stubGlobal("fetch", async (url: string, init?: RequestInit) => {
        const method = init?.method ?? "GET";
        sent.push({ method, url: String(url), body: typeof init?.body === "string" ? init.body : "" });
        if (String(url) === "api/panel/me/two-factor/setup")
            return answer(200, {
                secret,
                uri: `otpauth://totp/Ambrose:merle?secret=${secret}&issuer=Ambrose&algorithm=SHA1&digits=6&period=30`,
                issuer: "Ambrose",
                account: "merle",
                algorithm: "SHA1",
                digits: 6,
                period: 30,
                fresh: true,
            });
        if (String(url) === "api/panel/me/two-factor/enable")
            return answer(200, { recovery_codes: codes, recovery_codes_left: 10, user: { ...operator, two_factor: true } });
        return answer(404, { error: "not_found", message: "Nothing here" });
    });
    host = document.createElement("div");
    document.body.append(host);
    page = mount(Enroll, { target: host });
    flushSync();
});

afterEach(() => {
    if (page) unmount(page);
    page = null;
    host.remove();
    vi.unstubAllGlobals();
    session.mustEnroll = false;
    session.user = null;
});

describe("required enrollment", () => {
    it("enrollment shows the secret grouped for typing, turns two-factor on with the password and a code, and shows the recovery codes once", async () => {
        await vi.waitFor(() => expect(host.textContent).toContain("GEZD GNBV GY3T QOJQ GEZD GNBV GY3T QOJQ"));
        const link = [...host.querySelectorAll<HTMLAnchorElement>("a")].find((anchor) => anchor.href.startsWith("otpauth://"));
        expect(link, "the link an authenticator app opens is offered").toBeDefined();
        expect(button("Turn on two-factor sign-in")?.disabled).toBe(true);
        expect(host.textContent).toContain("Enter your password.");

        await turnOn();
        const enabled = sent.find((request) => request.url === "api/panel/me/two-factor/enable");
        expect(enabled?.method).toBe("POST");
        expect(JSON.parse(enabled?.body ?? "{}")).toEqual({ password: "a long passphrase for merle", code: "123456" });

        for (const code of codes) expect(document.body.textContent).toContain(code);
        expect(session.mustEnroll, "the panel stays at enrollment until the codes are saved").toBe(true);

        document.dispatchEvent(new KeyboardEvent("keydown", { key: "Escape", bubbles: true }));
        document.body.dispatchEvent(new PointerEvent("pointerdown", { bubbles: true }));
        await settle();
        flushSync();
        expect(document.body.textContent, "neither Escape nor a click outside closes the dialog").toContain("Save your recovery codes");

        expect(button("Done")?.disabled).toBe(true);
        document.querySelector<HTMLButtonElement>("#recovery-codes-stored")?.click();
        flushSync();
        expect(button("Done")?.disabled).toBe(false);
        button("Done")?.click();
        await settle();
        flushSync();
        expect(session.mustEnroll).toBe(false);
        expect(session.user?.two_factor).toBe(true);
    });

    it("the recovery codes leave the page and browser storage when their dialog closes", async () => {
        await turnOn();
        document.querySelector<HTMLButtonElement>("#recovery-codes-stored")?.click();
        flushSync();
        button("Done")?.click();
        await settle();
        flushSync();
        await vi.waitFor(() => expect(document.body.textContent).not.toContain("Save your recovery codes"));

        const bare = codes.map((code) => code.replace("-", ""));
        const markup = document.documentElement.outerHTML;
        const kept = stored();
        for (const code of [...codes, ...bare]) {
            expect(markup, `${code} is still in the page`).not.toContain(code);
            expect(kept, `${code} is in the browser's storage`).not.toContain(code);
        }
    });
});
