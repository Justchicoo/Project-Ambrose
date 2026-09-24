/*
 * Project Ambrose by Imjustchico
 * The panel's one way to reach the API of the host that served it: relative requests the browser resolves against the page's own address, with the session's CSRF token on anything that changes something, every answer checked against its shape, every refusal turned into an error carrying its status, code, message, request id and field problems, and the browser session itself, probed on load where finding none is an answer rather than an error, opened by trading the admin token once on an app's own listener or by a panel user's name and password on the panel's, closed on request, and marked ended when an answer says so.
 */

import * as v from "valibot";
import { PanelSessionAnswer, PanelSignedIn, SessionAnswer, type PanelUser } from "./schemas";

export type Fields = Record<string, string>;

export class ApiError extends Error {
    readonly status: number;
    readonly code: string;
    readonly requestId: string;
    readonly fields: Fields;

    constructor(status: number, code: string, message: string, requestId: string, fields: Fields = {}) {
        super(message);
        this.name = "ApiError";
        this.status = status;
        this.code = code;
        this.requestId = requestId;
        this.fields = fields;
    }
}

export type SessionState = {
    state: "checking" | "signed-out" | "signed-in" | "unreachable";
    app: string;
    csrf: string | null;
    via: "session" | "token" | null;
    ended: boolean;
    panel: boolean;
    needsOwner: boolean;
    user: PanelUser | null;
};

export const session = $state<SessionState>({
    state: "checking",
    app: "",
    csrf: null,
    via: null,
    ended: false,
    panel: false,
    needsOwner: false,
    user: null,
});

const changing = new Set(["POST", "PUT", "PATCH", "DELETE"]);

function readJson(text: string): unknown {
    if (text === "") return null;
    try {
        return JSON.parse(text);
    } catch {
        return null;
    }
}

export function readProblem(status: number, body: unknown, headerId: string): ApiError {
    const record = body !== null && typeof body === "object" ? (body as Record<string, unknown>) : {};
    const code = typeof record.error === "string" ? record.error : `http_${status}`;
    const message = typeof record.message === "string" ? record.message : `The server answered ${status}`;
    const requestId = typeof record.request_id === "string" ? record.request_id : headerId;
    const fields: Fields = {};
    if (record.fields !== null && typeof record.fields === "object")
        for (const [field, problem] of Object.entries(record.fields as Record<string, unknown>))
            if (typeof problem === "string") fields[field] = problem;
    return new ApiError(status, code, message, requestId, fields);
}

export async function request<Schema extends v.GenericSchema>(
    method: string,
    path: string,
    schema: Schema | null,
    body?: unknown,
    signal?: AbortSignal,
): Promise<v.InferOutput<Schema>> {
    const headers: Record<string, string> = { Accept: "application/json" };
    if (body !== undefined) headers["Content-Type"] = "application/json";
    if (changing.has(method) && session.csrf) headers["X-CSRF-Token"] = session.csrf;

    let response: Response;
    try {
        response = await fetch(path, {
            method,
            headers,
            body: body === undefined ? undefined : JSON.stringify(body),
            credentials: "same-origin",
            cache: "no-store",
            signal,
        });
    } catch (failure) {
        if (failure instanceof DOMException && failure.name === "AbortError") throw failure;
        throw new ApiError(0, "unreachable", "The panel could not reach the server that served it", "");
    }

    const headerId = response.headers.get("X-Request-Id") ?? "";
    const parsed = readJson(await response.text());
    if (!response.ok) {
        const problem = readProblem(response.status, parsed, headerId);
        if (response.status === 401 && session.state === "signed-in") {
            session.state = "signed-out";
            session.csrf = null;
            session.ended = true;
        }
        throw problem;
    }
    if (schema === null) return parsed as v.InferOutput<Schema>;
    const checked = v.safeParse(schema, parsed);
    if (!checked.success)
        throw new ApiError(
            response.status,
            "unexpected_answer",
            `The server's answer to ${path} is not the shape this panel reads`,
            headerId,
        );
    return checked.output;
}

function adopt(answer: SessionAnswer) {
    session.app = answer.app;
    if (!answer.signed_in) {
        session.state = "signed-out";
        session.csrf = null;
        session.via = null;
        return;
    }
    session.state = "signed-in";
    session.csrf = answer.csrf;
    session.via = answer.signed_in_with;
    session.ended = false;
}

function adoptPanel(answer: v.InferOutput<typeof PanelSessionAnswer>) {
    session.app = answer.app;
    session.panel = answer.needs_owner !== undefined;
    session.needsOwner = answer.needs_owner ?? false;
    session.user = answer.user ?? null;
    if (!answer.signed_in) {
        session.state = "signed-out";
        session.csrf = null;
        session.via = null;
        return;
    }
    session.state = "signed-in";
    session.csrf = answer.csrf;
    session.via = answer.signed_in_with ?? "session";
    session.ended = false;
}

export async function probeSession() {
    try {
        adoptPanel(await request("GET", "api/session", PanelSessionAnswer));
    } catch {
        session.state = "unreachable";
    }
}

export async function signIn(token: string) {
    adopt(await request("POST", "api/session", SessionAnswer, { token }));
}

export async function signInAsUser(username: string, password: string) {
    const answer = await request("POST", "api/panel/session", PanelSignedIn, { username, password });
    session.state = "signed-in";
    session.csrf = answer.csrf;
    session.via = "session";
    session.ended = false;
    session.needsOwner = false;
    session.user = answer.user;
}

export async function claimOwner(token: string, username: string, password: string) {
    const answer = await request("POST", "api/panel/claim", PanelSignedIn, { token, username, password });
    session.state = "signed-in";
    session.csrf = answer.csrf;
    session.via = "session";
    session.ended = false;
    session.needsOwner = false;
    session.user = answer.user;
}

export async function signOut() {
    await request("DELETE", session.panel ? "api/panel/session" : "api/session", null);
    session.user = null;
    session.state = "signed-out";
    session.csrf = null;
    session.via = null;
    session.ended = false;
}
