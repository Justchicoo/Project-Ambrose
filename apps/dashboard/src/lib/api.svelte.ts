/*
 * Project Ambrose by Imjustchico
 * The panel's one way to reach the API of the host that served it: relative requests the browser resolves against the page's own address, with the session's CSRF token on anything that changes something, every answer checked against its shape, every refusal turned into an error carrying its status, code, message, request id and field problems, and the browser session itself, probed on load where finding none is an answer rather than an error, opened by trading the admin token once on an app's own listener or by a panel user's name and password on the panel's, followed by a code or a recovery code when the operator has two-factor sign-in, closed on request, and marked ended when an answer says so. A refusal saying two-factor sign-in is required sends the session to enrollment, and one asking for a fresh check of who the operator is waits on the prompt the page registered and, once that check is made, sends the same request again exactly once, so a check that is refused or cancelled changes nothing. The operator's own two-factor calls live here too; turning it on hands the recovery codes and the operator's new state back to the caller, which adopts that state only once the codes have been shown, moving to another authenticator app sends a current code or recovery code from the app in use beside the new app's code, and nothing here ever keeps a code.
 */

import * as v from "valibot";
import {
    PanelSessionAnswer,
    PanelSignedIn,
    PanelSignInAnswer,
    RecoveryCodesIssued,
    SessionAnswer,
    StepUpAnswer,
    StepUpAsked,
    TwoFactorSetup,
    TwoFactorState,
    TwoFactorTurnedOff,
    type PanelUser,
} from "./schemas";

export type Fields = Record<string, string>;

export class ApiError extends Error {
    readonly status: number;
    readonly code: string;
    readonly requestId: string;
    readonly fields: Fields;
    readonly body: unknown;

    constructor(status: number, code: string, message: string, requestId: string, fields: Fields = {}, body: unknown = null) {
        super(message);
        this.name = "ApiError";
        this.status = status;
        this.code = code;
        this.requestId = requestId;
        this.fields = fields;
        this.body = body;
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
    mustEnroll: boolean;
};

export type SecondFactor = { code: string } | { recovery_code: string };
export type Confirmation = SecondFactor | { password: string };
export type SignInStep = "signed-in" | "second-factor";
export type StepUpPrompt = (asked: StepUpAsked) => Promise<boolean>;

export const session = $state<SessionState>({
    state: "checking",
    app: "",
    csrf: null,
    via: null,
    ended: false,
    panel: false,
    needsOwner: false,
    user: null,
    mustEnroll: false,
});

let stepUpPrompt: StepUpPrompt | null = null;

export function onStepUp(prompt: StepUpPrompt): () => void {
    stepUpPrompt = prompt;
    return () => {
        if (stepUpPrompt === prompt) stepUpPrompt = null;
    };
}

function mustEnroll(user: PanelUser | null): boolean {
    return user !== null && user.two_factor_required && !user.two_factor;
}

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
    const message =
        typeof record.message === "string"
            ? record.message
            : typeof record.reason === "string" && record.reason !== ""
              ? record.reason
              : `The server answered ${status}`;
    const requestId = typeof record.request_id === "string" ? record.request_id : headerId;
    const fields: Fields = {};
    if (record.fields !== null && typeof record.fields === "object")
        for (const [field, problem] of Object.entries(record.fields as Record<string, unknown>))
            if (typeof problem === "string") fields[field] = problem;
    return new ApiError(status, code, message, requestId, fields, body);
}

async function send<Schema extends v.GenericSchema>(
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
            session.mustEnroll = false;
        }
        if (response.status === 403 && problem.code === "two_factor_required" && session.state === "signed-in") session.mustEnroll = true;
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

export async function request<Schema extends v.GenericSchema>(
    method: string,
    path: string,
    schema: Schema | null,
    body?: unknown,
    signal?: AbortSignal,
): Promise<v.InferOutput<Schema>> {
    try {
        return await send(method, path, schema, body, signal);
    } catch (failure) {
        const prompt = stepUpPrompt;
        if (!(failure instanceof ApiError) || failure.status !== 403 || failure.code !== "step_up_required" || prompt === null)
            throw failure;
        const asked = v.safeParse(StepUpAsked, failure.body);
        if (!asked.success || signal?.aborted) throw failure;
        if (!(await prompt(asked.output))) throw failure;
        return send(method, path, schema, body, signal);
    }
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
    session.mustEnroll = answer.signed_in && mustEnroll(session.user);
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

function adoptSignedIn(answer: v.InferOutput<typeof PanelSignedIn>) {
    session.state = "signed-in";
    session.csrf = answer.csrf;
    session.via = "session";
    session.ended = false;
    session.needsOwner = false;
    session.user = answer.user;
    session.mustEnroll = mustEnroll(answer.user);
}

export function adoptUser(user: PanelUser) {
    session.user = user;
    session.mustEnroll = mustEnroll(user);
}

export async function signInAsUser(username: string, password: string): Promise<SignInStep> {
    const answer = await request("POST", "api/panel/session", PanelSignInAnswer, { username, password });
    const signedIn = v.safeParse(PanelSignedIn, answer);
    if (!signedIn.success) return "second-factor";
    adoptSignedIn(signedIn.output);
    return "signed-in";
}

export async function answerSecondFactor(factor: SecondFactor) {
    adoptSignedIn(await request("POST", "api/panel/session/second-factor", PanelSignedIn, factor));
}

export async function claimOwner(token: string, username: string, password: string) {
    adoptSignedIn(await request("POST", "api/panel/claim", PanelSignedIn, { token, username, password }));
}

export async function signOut() {
    await request("DELETE", session.panel ? "api/panel/session" : "api/session", null);
    session.user = null;
    session.state = "signed-out";
    session.csrf = null;
    session.via = null;
    session.ended = false;
    session.mustEnroll = false;
}

export function twoFactorState(signal?: AbortSignal) {
    return request("GET", "api/panel/me/two-factor", TwoFactorState, undefined, signal);
}

export function setUpTwoFactor(replace = false) {
    return request("POST", "api/panel/me/two-factor/setup", TwoFactorSetup, { replace });
}

export function turnOnTwoFactor(password: string, code: string, current: SecondFactor | null = null) {
    const held =
        current === null ? {} : "code" in current ? { current_code: current.code } : { current_recovery_code: current.recovery_code };
    return request("POST", "api/panel/me/two-factor/enable", RecoveryCodesIssued, { password, code, ...held });
}

export async function turnOffTwoFactor(password: string, factor: SecondFactor) {
    const answer = await request("POST", "api/panel/me/two-factor/disable", TwoFactorTurnedOff, { password, ...factor });
    if (answer.user) adoptUser(answer.user);
}

export function newRecoveryCodes(password: string, factor: SecondFactor) {
    return request("POST", "api/panel/me/two-factor/recovery-codes", RecoveryCodesIssued, { password, ...factor });
}

export function stepUp(confirmation: Confirmation, purpose = "") {
    return request("POST", "api/panel/step-up", StepUpAnswer, purpose === "" ? confirmation : { ...confirmation, for: purpose });
}
