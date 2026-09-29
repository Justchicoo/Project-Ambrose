/*
 * Project Ambrose by Imjustchico
 * Tests the sign-in page in a real browser against a stubbed server: a 422 marks the field it names beside the input and shows its request id, a field the form does not have is listed with the id, a wrong token is said above the form with its id, the right token signs the browser in, and a one-time owner link's token fills the form and leaves the address and the browser's history at once, so the page lands on the overview; on the panel, a password that asks for a second factor opens nothing and shows a code step instead, the right code signs in, a wrong one says so, a recovery code can be used instead, and a sign-in that ran out goes back to the name and password.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { session } from "$lib/api.svelte";
import SignIn from "./SignIn.svelte";

type Reply = { status: number; body: unknown; id: string };

let replies: Reply[] = [];
let sent: { url: string; body: string }[] = [];
let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

function settle() {
    return new Promise((done) => setTimeout(done, 20));
}

async function submit(token: string) {
    const input = host.querySelector<HTMLInputElement>("#sign-in-token");
    if (!input) throw new Error("no token field");
    input.value = token;
    input.dispatchEvent(new Event("input", { bubbles: true }));
    flushSync();
    host.querySelector("form")?.requestSubmit();
    await settle();
    flushSync();
}

beforeEach(() => {
    replies = [];
    sent = [];
    session.state = "signed-out";
    session.csrf = null;
    session.ended = false;
    session.panel = false;
    session.needsOwner = false;
    session.user = null;
    vi.stubGlobal("fetch", async (url: string, init?: RequestInit) => {
        sent.push({ url: String(url), body: typeof init?.body === "string" ? init.body : "" });
        const reply = replies.shift();
        if (!reply) throw new TypeError("no answer");
        return new Response(JSON.stringify(reply.body), {
            status: reply.status,
            headers: { "Content-Type": "application/json", "X-Request-Id": reply.id },
        });
    });
    host = document.createElement("div");
    document.body.append(host);
    page = mount(SignIn, { target: host });
});

afterEach(() => {
    if (page) unmount(page);
    host.remove();
    vi.unstubAllGlobals();
});

describe("the sign-in page", () => {
    it("marks the field a 422 names beside it and shows the request id", async () => {
        replies.push({
            status: 422,
            body: {
                error: "invalid",
                message: "Signing in takes the token",
                fields: { token: "Enter this app's admin token" },
                request_id: "id-422",
            },
            id: "id-422",
        });
        await submit("not-empty");
        const problem = host.querySelector("#sign-in-token-problem");
        expect(problem?.textContent).toBe("Enter this app's admin token");
        expect(host.querySelector("#sign-in-token")?.getAttribute("aria-invalid")).toBe("true");
        expect(host.textContent).toContain("id-422");
    });

    it("lists a field the form does not have with the request id", async () => {
        replies.push({
            status: 422,
            body: {
                error: "invalid",
                message: "Only the token",
                fields: { remember: "Signing in takes only the token" },
                request_id: "id-extra",
            },
            id: "id-extra",
        });
        await submit("token-value");
        const alert = host.querySelector("[role=alert]");
        expect(alert?.textContent).toContain("remember");
        expect(alert?.textContent).toContain("Signing in takes only the token");
        expect(alert?.textContent).toContain("id-extra");
    });

    it("says a wrong token above the form with its request id", async () => {
        replies.push({
            status: 401,
            body: { error: "wrong_token", message: "That is not this app's admin token", request_id: "id-401" },
            id: "id-401",
        });
        await submit("wrong-token");
        const alert = host.querySelector("[role=alert]");
        expect(alert?.textContent).toContain("That is not this app's admin token.");
        expect(alert?.textContent).toContain("id-401");
        expect(session.state).toBe("signed-out");
    });

    it("marks an empty token without asking the server", async () => {
        await submit("   ");
        expect(host.querySelector("#sign-in-token-problem")?.textContent).toBe("Enter this app's admin token.");
    });

    it("signs in with the right token", async () => {
        replies.push({
            status: 201,
            body: { app: "gameserver", signed_in: true, signed_in_with: "session", csrf: "csrf", idle_seconds: 60, lifetime_seconds: 3600 },
            id: "id-201",
        });
        await submit("0123456789abcdef0123456789abcdef");
        expect(session.state).toBe("signed-in");
        expect(session.app).toBe("gameserver");
    });

    it("takes a one-time owner link's token out of the address as soon as it reads it", async () => {
        if (page) unmount(page);
        const before = window.location.href;
        const routed: string[] = [];
        const follow = () => routed.push(window.location.hash);
        window.addEventListener("hashchange", follow);
        try {
            history.replaceState(null, "", `${window.location.pathname}${window.location.search}#claim?token=link-token-abc`);
            session.panel = true;
            session.needsOwner = true;
            page = mount(SignIn, { target: host });
            flushSync();
            expect(host.querySelector<HTMLInputElement>("#sign-in-token")?.value).toBe("link-token-abc");
            expect(window.location.hash).toBe("#overview");
            expect(window.location.href).not.toContain("link-token-abc");
            expect(routed).toEqual(["#overview"]);
        } finally {
            window.removeEventListener("hashchange", follow);
            history.replaceState(null, "", before);
        }
    });

    it("lets a password manager fill and paste into every field it asks a secret in", async () => {
        open();
        const secrets = Array.from(host.querySelectorAll<HTMLInputElement>("input"));
        expect(secrets.length, "the form asks for something").toBeGreaterThan(0);

        for (const field of secrets) {
            expect(field.autocomplete, `${field.id} names no autofill purpose`).not.toBe("");
            expect(field.autocomplete, `${field.id} tells a password manager to stay away`).not.toBe("off");
            expect(field.readOnly, `${field.id} cannot be typed into`).toBe(false);

            const clipboard = new DataTransfer();
            clipboard.setData("text/plain", "a-secret-from-a-manager");
            const paste = new ClipboardEvent("paste", { clipboardData: clipboard, bubbles: true, cancelable: true });
            field.focus();
            expect(field.dispatchEvent(paste), `${field.id} refuses a pasted value`).toBe(true);
            expect(paste.defaultPrevented, `${field.id} cancels a paste`).toBe(false);
        }
    });
});

describe("the second step of a panel sign-in", () => {
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
    const asked = { second_factor: true, methods: ["totp", "recovery_code"], expires_seconds: 300 };

    function fill(id: string, value: string) {
        const input = host.querySelector<HTMLInputElement>(id);
        if (!input) throw new Error(`no ${id} field`);
        input.value = value;
        input.dispatchEvent(new Event("input", { bubbles: true }));
        flushSync();
    }

    async function send() {
        host.querySelector("form")?.requestSubmit();
        await settle();
        flushSync();
    }

    function press(label: string) {
        const found = [...host.querySelectorAll<HTMLButtonElement>("button")].find((button) => button.textContent?.trim() === label);
        if (!found) throw new Error(`no ${label} button`);
        found.click();
        flushSync();
    }

    async function password() {
        fill("#sign-in-username", "merle");
        fill("#sign-in-password", "a long passphrase for merle");
        await send();
    }

    beforeEach(() => {
        if (page) unmount(page);
        session.panel = true;
        session.needsOwner = false;
        page = mount(SignIn, { target: host });
        flushSync();
    });

    it("a password that asks for a second factor shows the code step and the right code signs in", async () => {
        replies.push(
            { status: 200, body: asked, id: "id-asked" },
            { status: 200, body: { csrf: "csrf-after-code", user: operator }, id: "id-in" },
        );
        await password();
        expect(session.state).toBe("signed-out");
        expect(host.querySelector("#sign-in-password"), "the password is not asked for again").toBeNull();
        const code = host.querySelector<HTMLInputElement>("#sign-in-code");
        expect(code).not.toBeNull();
        expect(code?.autocomplete).toBe("one-time-code");
        expect(code?.inputMode).toBe("numeric");

        fill("#sign-in-code", "123 456");
        await send();
        expect(sent[1].url).toBe("api/panel/session/second-factor");
        expect(JSON.parse(sent[1].body)).toEqual({ code: "123456" });
        expect(session.state).toBe("signed-in");
        expect(session.csrf).toBe("csrf-after-code");
    });

    it("says a wrong code without asking the server about one that is not six digits", async () => {
        replies.push(
            { status: 200, body: asked, id: "id-asked" },
            {
                status: 401,
                body: { error: "second_factor_refused", message: "That code does not sign you in", request_id: "id-wrong" },
                id: "id-wrong",
            },
        );
        await password();
        fill("#sign-in-code", "12345");
        await send();
        expect(sent).toHaveLength(1);
        expect(host.querySelector("#sign-in-code-problem")?.textContent).toBe("Enter the six digits your authenticator app shows.");

        fill("#sign-in-code", "654321");
        await send();
        const alert = host.querySelector("[role=alert]");
        expect(alert?.textContent).toContain("That code does not sign you in.");
        expect(alert?.textContent).toContain("id-wrong");
        expect(session.state).toBe("signed-out");
    });

    it("a recovery code can be used instead of a code", async () => {
        replies.push(
            { status: 200, body: asked, id: "id-asked" },
            { status: 200, body: { csrf: "csrf-recovered", user: operator }, id: "id-in" },
        );
        await password();
        press("Use a recovery code instead");
        expect(host.querySelector("#sign-in-code")).toBeNull();
        fill("#sign-in-recovery-code", "7k2qm-xr4td");
        await send();
        expect(JSON.parse(sent[1].body)).toEqual({ recovery_code: "7K2QMXR4TD" });
        expect(session.state).toBe("signed-in");
    });

    it("goes back to the name and password when the sign-in has run out", async () => {
        replies.push(
            { status: 200, body: asked, id: "id-asked" },
            {
                status: 401,
                body: { error: "challenge_expired", message: "That sign-in has run out", request_id: "id-late" },
                id: "id-late",
            },
        );
        await password();
        fill("#sign-in-code", "123456");
        await send();
        expect(host.querySelector("#sign-in-password")).not.toBeNull();
        expect(host.querySelector<HTMLInputElement>("#sign-in-password")?.value, "the password is not kept for another try").toBe("");
        expect(host.textContent).toContain("That sign-in ran out. Enter your name and password again.");
    });
});
