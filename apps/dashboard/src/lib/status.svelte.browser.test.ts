/*
 * Project Ambrose by Imjustchico
 * Tests that starting the watcher from inside an effect does not make what the first poll reads a dependency of that effect, because when it did, assigning the app list invalidated the effect, the cleanup aborted the request in flight, the request queued behind it was handed a signal aborted before it began, and the panel reported that the server had stopped answering while the server was answering fine.
 */

import { flushSync } from "svelte";
import { afterEach, describe, expect, it, vi } from "vitest";
import { live, stop, watch } from "./status.svelte";

afterEach(() => {
    stop();
    vi.unstubAllGlobals();
    live.apps = [];
    live.capabilities = null;
    live.status = null;
    live.attempt = 0;
    live.connection = "live";
});

describe("watching from inside an effect", () => {
    it("does not take what the first poll reads as a dependency of the caller", () => {
        vi.stubGlobal("fetch", () => new Promise(() => {}));
        let runs = 0;
        const dispose = $effect.root(() => {
            $effect(() => {
                runs += 1;
                watch();
                return () => stop();
            });
        });
        flushSync();
        expect(runs).toBe(1);

        live.apps = [{ name: "loginserver", role: "login", realm: "", revision: "abc1234", supervision: null } as never];
        flushSync();
        expect(runs).toBe(1);

        live.capabilities = null;
        live.status = null;
        flushSync();
        expect(runs).toBe(1);
        dispose();
    });
});
