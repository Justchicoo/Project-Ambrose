/*
 * Project Ambrose by Imjustchico
 * Tests the panel's API client against a stubbed fetch: a refusal becomes an error with its status, code, message, request id and fields, the CSRF token rides only on requests that change something, an answer of the wrong shape is refused, an unreachable server is named, signing in adopts the session and a 401 afterwards marks it ended.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { ApiError, probeSession, readProblem, request, session, signIn } from "./api.svelte";
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
