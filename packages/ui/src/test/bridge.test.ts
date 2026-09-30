/*
 * Project Ambrose by Imjustchico
 * Checks the host bridge: which host a page is in, that a native host is chosen only on a desktop program's own origin, that a reply reaches the call that asked for it, and that a reply for a call nobody is waiting on is ignored.
 */

import { describe, expect, it, vi } from "vitest";
import { createHost, hostKind } from "../bridge/bridge";

type Listener = (event: { data: unknown }) => void;

const launcherOrigin = { protocol: "https:", hostname: "launcher.ambrose", port: "" };
const webkitOrigin = { protocol: "ambrose:", hostname: "launcher.ambrose", port: "" };

function windowsHost(location: object = launcherOrigin) {
    const sent: unknown[] = [];
    let listener: Listener | undefined;
    const scope = {
        location,
        chrome: {
            webview: {
                postMessage: (message: unknown) => sent.push(message),
                addEventListener: (_type: "message", handler: Listener) => {
                    listener = handler;
                },
            },
        },
    } as unknown as Window;
    return { scope, sent, reply: (data: unknown) => listener?.({ data }) };
}

function webkitHost(location: object = webkitOrigin) {
    const sent: unknown[] = [];
    const scope = {
        location,
        webkit: { messageHandlers: { ambrose: { postMessage: (message: unknown) => sent.push(message) } } },
    } as unknown as Window & { ambroseHostReply?: (message: unknown) => void };
    return { scope, sent };
}

describe("the host bridge", () => {
    it("knows a plain browser when no host is there", () => {
        expect(hostKind({} as Window)).toBe("http");
        expect(hostKind(undefined)).toBe("http");
    });

    it("knows the Windows web view", () => {
        expect(hostKind(windowsHost().scope)).toBe("webview2");
    });

    it("knows the web view every other desktop uses", () => {
        expect(hostKind(webkitHost().scope)).toBe("webkit");
    });

    it("answers http on any origin but a desktop program's own, even where a web view's channel is there", () => {
        const remote = [
            { protocol: "https:", hostname: "panel.example.org", port: "8443" },
            { protocol: "http:", hostname: "127.0.0.1", port: "12021" },
            { protocol: "http:", hostname: "launcher.ambrose", port: "" },
            { protocol: "https:", hostname: "launcher.ambrose", port: "8443" },
            { protocol: "https:", hostname: "launcher.ambrose.example.org", port: "" },
        ];
        for (const location of remote) {
            expect(hostKind(windowsHost(location).scope)).toBe("http");
            expect(hostKind(webkitHost(location).scope)).toBe("http");
        }
        expect(hostKind(windowsHost({ protocol: "https:", hostname: "panel.ambrose", port: "" }).scope)).toBe("webview2");
    });

    it("sends a call and resolves it with the reply that carries its number", async () => {
        const host = windowsHost();
        const bridge = createHost("/api", host.scope);
        const pending = bridge.call<{ players: number }>({ path: "/status" });
        expect(host.sent).toHaveLength(1);
        const sent = host.sent[0] as { id: number; path: string; method: string };
        expect(sent.path).toBe("/status");
        expect(sent.method).toBe("GET");
        host.reply({ id: sent.id, ok: true, status: 200, body: { players: 128 } });
        await expect(pending).resolves.toEqual({ ok: true, status: 200, body: { players: 128 } });
    });

    it("rejects a call the host answered with an error", async () => {
        const host = windowsHost();
        const bridge = createHost("/api", host.scope);
        const pending = bridge.call({ path: "/power", method: "POST", body: { action: "stop" } });
        const sent = host.sent[0] as { id: number };
        host.reply({ id: sent.id, error: "the supervisor refused" });
        await expect(pending).rejects.toThrow("the supervisor refused");
    });

    it("ignores a reply for a call nobody is waiting on", async () => {
        const host = windowsHost();
        const bridge = createHost("/api", host.scope);
        const pending = bridge.call({ path: "/status" });
        const sent = host.sent[0] as { id: number };
        host.reply({ id: sent.id + 99, ok: true, status: 200, body: null });
        host.reply({ ok: true, status: 200, body: null });
        host.reply({ id: sent.id, ok: true, status: 200, body: "done" });
        await expect(pending).resolves.toEqual({ ok: true, status: 200, body: "done" });
    });

    it("talks to the panel over HTTP when nothing hosts the page", async () => {
        const fetcher = vi.fn(async () => new Response(JSON.stringify({ ok: true }), { status: 200 }));
        vi.stubGlobal("fetch", fetcher);
        const bridge = createHost("/api/", {} as Window);
        const reply = await bridge.call<{ ok: boolean }>({ path: "/status" });
        expect(fetcher).toHaveBeenCalledWith("/api/status", expect.objectContaining({ method: "GET", credentials: "same-origin" }));
        expect(reply.body).toEqual({ ok: true });
        vi.unstubAllGlobals();
    });
});
