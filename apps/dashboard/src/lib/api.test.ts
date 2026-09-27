/*
 * Project Ambrose by Imjustchico
 * Tests the panel's API client against a stubbed fetch: a refusal becomes an error with its status, code, message, request id and fields, the CSRF token rides only on requests that change something, an answer of the wrong shape is refused, an unreachable server is named, signing in adopts the session and a 401 afterwards marks it ended, a password that asks for a second factor opens nothing until a code does, a 403 saying two-factor sign-in is required sends the session to enrollment, moving to another authenticator app names a code or recovery code from the app in use, and a 403 asking for a fresh check waits on the registered prompt and sends the request again exactly once, while a refused or cancelled check sends nothing more.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import {
    ApiError,
    answerSecondFactor,
    onStepUp,
    probeSession,
    readProblem,
    request,
    session,
    signIn,
    signInAsUser,
    turnOnTwoFactor,
} from "./api.svelte";
import { Status } from "./schemas";

type Sent = { url: string; init: RequestInit };

function answer(status: number, body: unknown, requestId = "req-1") {
    return new Response(body === undefined ? "" : JSON.stringify(body), {
        status,
        headers: { "Content-Type": "application/json", "X-Request-Id": requestId },
    });
}

let sent: Sent[] = [];
let replies: Response[] = [];

beforeEach(() => {
    sent = [];
    replies = [];
    vi.stubGlobal("fetch", async (url: string, init: RequestInit) => {
        sent.push({ url: String(url), init });
        const reply = replies.shift();
        if (!reply) throw new TypeError("no answer");
        return reply;
    });
    session.state = "checking";
    session.csrf = null;
    session.via = null;
    session.ended = false;
    session.panel = false;
    session.user = null;
    session.mustEnroll = false;
});

afterEach(() => {
    vi.unstubAllGlobals();
});

const sessionAnswer = {
    app: "gameserver",
    signed_in: true,
    signed_in_with: "session",
    csrf: "csrf-token",
    idle_seconds: 43200,
    lifetime_seconds: 604800,
};

describe("reading a refusal", () => {
    it("keeps the status, code, message, request id and fields", () => {
        const problem = readProblem(
            422,
            { error: "invalid", message: "Two things", request_id: "abc", fields: { token: "Enter it", extra: "No", odd: 3 } },
            "header",
        );
        expect(problem).toBeInstanceOf(ApiError);
        expect([problem.status, problem.code, problem.message, problem.requestId]).toEqual([422, "invalid", "Two things", "abc"]);
        expect(problem.fields).toEqual({ token: "Enter it", extra: "No" });
    });

    it("falls back to the header's request id and a plain message", () => {
        const problem = readProblem(502, null, "from-header");
        expect([problem.code, problem.message, problem.requestId]).toEqual(["http_502", "The server answered 502", "from-header"]);
    });
});

describe("sending requests", () => {
    it("sends paths relative to the page and the CSRF token only when something changes", async () => {
        session.csrf = "csrf-token";
        replies.push(answer(200, {}), answer(200, {}));
        await request("GET", "api/health", null);
        await request("POST", "api/echo", null, { a: 1 });
        expect(sent[0].url).toBe("api/health");
        expect((sent[0].init.headers as Record<string, string>)["X-CSRF-Token"]).toBeUndefined();
        expect((sent[1].init.headers as Record<string, string>)["X-CSRF-Token"]).toBe("csrf-token");
        expect(sent[1].init.credentials).toBe("same-origin");
        expect(sent[1].init.body).toBe('{"a":1}');
    });

    it("refuses an answer that is not the shape it reads", async () => {
        replies.push(answer(200, { schema: 1 }));
        await expect(request("GET", "api/status", Status)).rejects.toMatchObject({ code: "unexpected_answer" });
    });

    it("names a server it cannot reach", async () => {
        await expect(request("GET", "api/status", Status)).rejects.toMatchObject({ status: 0, code: "unreachable" });
    });
});

describe("the session", () => {
    it("adopts a session on sign-in and marks it ended when a later answer is 401", async () => {
        replies.push(answer(201, sessionAnswer), answer(401, { error: "unauthorized", message: "No" }));
        await signIn("0123456789abcdef0123456789abcdef");
        expect(sent[0].init.body).toBe('{"token":"0123456789abcdef0123456789abcdef"}');
        expect(session.state).toBe("signed-in");
        expect(session.csrf).toBe("csrf-token");
        await expect(request("GET", "api/status", Status)).rejects.toMatchObject({ status: 401 });
        expect(session.state).toBe("signed-out");
        expect(session.ended).toBe(true);
        expect(session.csrf).toBeNull();
    });

    it("reads a probe that finds no session as signed out and a failed probe as unreachable", async () => {
        replies.push(answer(200, { ...sessionAnswer, signed_in: false, signed_in_with: null, csrf: null }));
        await probeSession();
        expect(session.state).toBe("signed-out");
        expect(session.app).toBe("gameserver");
        replies.push(answer(200, sessionAnswer));
        await probeSession();
        expect(session.state).toBe("signed-in");
        expect(session.via).toBe("session");
        await probeSession();
        expect(session.state).toBe("unreachable");
    });
});

const operator = {
    id: 1,
    username: "merle",
    display_name: "merle",
    owner: true,
    role: "owner",
    permissions: ["panel.settings"],
    grants: {},
    must_change_password: false,
    two_factor: false,
    two_factor_required: false,
};

describe("two-factor sign-in", () => {
    it("opens nothing for a password that asks for a second factor until a code does", async () => {
        session.state = "signed-out";
        replies.push(
            answer(200, { second_factor: true, methods: ["totp", "recovery_code"], expires_seconds: 300 }),
            answer(200, { csrf: "csrf-after-code", user: { ...operator, two_factor: true } }),
        );
        await expect(signInAsUser("merle", "a long passphrase")).resolves.toBe("second-factor");
        expect(session.state).toBe("signed-out");
        expect(session.csrf).toBeNull();
        await answerSecondFactor({ code: "123456" });
        expect(sent[1].url).toBe("api/panel/session/second-factor");
        expect(sent[1].init.body).toBe('{"code":"123456"}');
        expect(session.state).toBe("signed-in");
        expect(session.csrf).toBe("csrf-after-code");
        expect(session.mustEnroll).toBe(false);
    });

    it("signs in at once when no second factor is asked for", async () => {
        session.state = "signed-out";
        replies.push(answer(200, { csrf: "csrf-token", user: operator }));
        await expect(signInAsUser("merle", "a long passphrase")).resolves.toBe("signed-in");
        expect(session.state).toBe("signed-in");
        expect(session.user?.username).toBe("merle");
    });

    it("sends a user the requirement covers straight to enrollment", async () => {
        replies.push(answer(200, { ...sessionAnswer, needs_owner: false, user: { ...operator, two_factor_required: true } }));
        await probeSession();
        expect(session.state).toBe("signed-in");
        expect(session.mustEnroll).toBe(true);
    });

    it("a 403 two_factor_required sends the session to enrollment", async () => {
        session.state = "signed-in";
        replies.push(answer(403, { error: "two_factor_required", message: "Turn it on" }));
        await expect(request("GET", "api/status", Status)).rejects.toMatchObject({ status: 403, code: "two_factor_required" });
        expect(session.mustEnroll).toBe(true);
        expect(session.state).toBe("signed-in");
    });
});

describe("turning two-factor sign-in on", () => {
    const issued = { recovery_codes: ["7K2QM-XR4TD"], recovery_codes_left: 10, user: operator };

    it("sends the password and the new app's code alone for a first app", async () => {
        session.state = "signed-in";
        replies.push(answer(200, issued));
        await turnOnTwoFactor("a long passphrase", "123456");
        expect(JSON.parse(String(sent[0].init.body))).toEqual({ password: "a long passphrase", code: "123456" });
    });

    it("names a code or a recovery code from the app in use when moving to another", async () => {
        session.state = "signed-in";
        replies.push(answer(200, issued), answer(200, issued));
        await turnOnTwoFactor("a long passphrase", "123456", { code: "654321" });
        await turnOnTwoFactor("a long passphrase", "123456", { recovery_code: "7K2QMXR4TD" });
        expect(JSON.parse(String(sent[0].init.body))).toEqual({ password: "a long passphrase", code: "123456", current_code: "654321" });
        expect(JSON.parse(String(sent[1].init.body))).toEqual({
            password: "a long passphrase",
            code: "123456",
            current_recovery_code: "7K2QMXR4TD",
        });
    });
});

describe("a fresh check before a danger action", () => {
    const asked = {
        error: "step_up_required",
        message: "Confirm it is you",
        permission: "panel.settings",
        methods: ["totp", "recovery_code"],
        window_seconds: 300,
    };

    it("a 403 step_up_required asks for a fresh check and retries once, and a refused check changes nothing", async () => {
        session.state = "signed-in";
        session.csrf = "csrf-token";
        const prompts: string[] = [];
        let confirm = true;
        const stop = onStepUp(async (question) => {
            prompts.push(question.permission);
            return confirm;
        });
        try {
            replies.push(answer(403, asked), answer(200, { saved: true }));
            await expect(request("PATCH", "api/panel/settings", null, { values: { "Panel.Name": "Elsewhere" } })).resolves.toEqual({
                saved: true,
            });
            expect(prompts).toEqual(["panel.settings"]);
            expect(sent).toHaveLength(2);
            expect(sent[1].init.body).toBe(sent[0].init.body);
            expect((sent[1].init.headers as Record<string, string>)["X-CSRF-Token"]).toBe("csrf-token");

            replies.push(answer(403, asked), answer(403, asked));
            await expect(request("PATCH", "api/panel/settings", null, { values: {} })).rejects.toMatchObject({
                status: 403,
                code: "step_up_required",
            });
            expect(sent).toHaveLength(4);
            expect(prompts).toHaveLength(2);

            confirm = false;
            replies.push(answer(403, asked));
            await expect(request("PATCH", "api/panel/settings", null, { values: {} })).rejects.toMatchObject({ code: "step_up_required" });
            expect(sent).toHaveLength(5);
            expect(session.state).toBe("signed-in");
        } finally {
            stop();
        }
    });

    it("passes the refusal on when no page asks the question", async () => {
        session.state = "signed-in";
        replies.push(answer(403, asked));
        await expect(request("PATCH", "api/panel/settings", null, {})).rejects.toMatchObject({ code: "step_up_required" });
        expect(sent).toHaveLength(1);
    });
});
