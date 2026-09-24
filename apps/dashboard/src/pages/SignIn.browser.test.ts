/*
 * Project Ambrose by Imjustchico
 * Tests the sign-in page in a real browser against a stubbed server: a 422 marks the field it names beside the input and shows its request id, a field the form does not have is listed with the id, a wrong token is said above the form with its id, and the right token signs the browser in.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { session } from "$lib/api.svelte";
import SignIn from "./SignIn.svelte";

type Reply = { status: number; body: unknown; id: string };

let replies: Reply[] = [];
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
    session.state = "signed-out";
    session.csrf = null;
    session.ended = false;
    session.panel = false;
    session.needsOwner = false;
    session.user = null;
    vi.stubGlobal("fetch", async () => {
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
});
