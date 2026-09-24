/*
 * Project Ambrose by Imjustchico
 * The one typed call every surface makes to whatever hosts it, with an implementation for the Windows web view, for the one on every other desktop and for a browser talking to the panel over HTTP.
 */

export type HostKind = "webview2" | "webkit" | "http";

export type HostRequest = {
    path: string;
    method?: "GET" | "POST" | "PUT" | "DELETE";
    body?: unknown;
};

export type HostReply<T> = {
    ok: boolean;
    status: number;
    body: T;
};

export interface Host {
    readonly kind: HostKind;
    call<T>(request: HostRequest): Promise<HostReply<T>>;
}

type PendingCall = {
    resolve: (reply: HostReply<unknown>) => void;
    reject: (reason: Error) => void;
};

type WebView2Window = Window & {
    chrome?: {
        webview?: {
            postMessage(message: unknown): void;
            addEventListener(type: "message", listener: (event: { data: unknown }) => void): void;
        };
    };
};

type WebKitWindow = Window & {
    webkit?: {
        messageHandlers?: Record<string, { postMessage(message: unknown): void }>;
    };
    ambroseHostReply?: (message: unknown) => void;
};

const HANDLER = "ambrose";

export function hostKind(scope: Window | undefined = typeof window === "undefined" ? undefined : window): HostKind {
    const windows = scope as (WebView2Window & WebKitWindow) | undefined;
    if (windows?.chrome?.webview) {
        return "webview2";
    }
    if (windows?.webkit?.messageHandlers?.[HANDLER]) {
        return "webkit";
    }
    return "http";
}

class PostMessageHost implements Host {
    readonly kind: HostKind;
    private next = 0;
    private pending = new Map<number, PendingCall>();
    private send: (message: unknown) => void;

    constructor(kind: HostKind, send: (message: unknown) => void, listen: (receive: (message: unknown) => void) => void) {
        this.kind = kind;
        this.send = send;
        listen((message) => this.receive(message));
    }

    call<T>(request: HostRequest): Promise<HostReply<T>> {
        const id = ++this.next;
        return new Promise<HostReply<T>>((resolve, reject) => {
            this.pending.set(id, { resolve: resolve as (reply: HostReply<unknown>) => void, reject });
            this.send({ id, ...request, method: request.method ?? "GET" });
        });
    }

    private receive(message: unknown): void {
        const reply = message as { id?: number; ok?: boolean; status?: number; body?: unknown; error?: string };
        if (typeof reply?.id !== "number") {
            return;
        }
        const waiting = this.pending.get(reply.id);
        if (!waiting) {
            return;
        }
        this.pending.delete(reply.id);
        if (reply.error) {
            waiting.reject(new Error(reply.error));
            return;
        }
        waiting.resolve({ ok: reply.ok !== false, status: reply.status ?? 200, body: reply.body });
    }
}

class HttpHost implements Host {
    readonly kind = "http" as const;
    private base: string;

    constructor(base: string) {
        this.base = base.replace(/\/$/, "");
    }

    async call<T>(request: HostRequest): Promise<HostReply<T>> {
        const response = await fetch(`${this.base}${request.path}`, {
            method: request.method ?? "GET",
            headers: request.body === undefined ? undefined : { "content-type": "application/json" },
            body: request.body === undefined ? undefined : JSON.stringify(request.body),
            credentials: "same-origin",
        });
        const text = await response.text();
        const body = text.length === 0 ? undefined : (JSON.parse(text) as T);
        return { ok: response.ok, status: response.status, body: body as T };
    }
}

export function createHost(base = "/api", scope: Window | undefined = typeof window === "undefined" ? undefined : window): Host {
    const kind = hostKind(scope);
    if (kind === "webview2") {
        const view = (scope as WebView2Window).chrome!.webview!;
        return new PostMessageHost(
            kind,
            (message) => view.postMessage(message),
            (receive) => {
                view.addEventListener("message", (event) => receive(event.data));
            },
        );
    }
    if (kind === "webkit") {
        const handler = (scope as WebKitWindow).webkit!.messageHandlers![HANDLER];
        return new PostMessageHost(
            kind,
            (message) => handler.postMessage(message),
            (receive) => {
                (scope as WebKitWindow).ambroseHostReply = receive;
            },
        );
    }
    return new HttpHost(base);
}
