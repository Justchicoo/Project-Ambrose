/*
 * Project Ambrose by Imjustchico
 * Tests the channel as it sits on the host every surface shares: each question reaches its own path with what it was asked to send, a host that refuses is read as a refusal the window can show rather than as an answer, a host that replies with nothing is treated the same way, the steps of a first run come back as a list even when there are none, with the launch state and the server's status the launcher answered and nothing the launcher did not say read as a state, looking again carries the operator's settings, opening the log folder sends no folder at all, and a refusal or plan that names no state is read as retry or play, so the page never has to guard against a shape the channel could have prevented.
 */

import { describe, expect, it } from "vitest";
import type { Host, HostRequest } from "@ambrose/ui";
import { hostChannel, NoLauncher, profileOf, stateOf, type LauncherAnswer, type LauncherPlan } from "./channel";

function fakeHost(answer: (request: HostRequest) => { ok: boolean; status: number; body: unknown }): {
    host: Host;
    asked: HostRequest[];
} {
    const asked: HostRequest[] = [];
    return {
        asked,
        host: {
            kind: "http",
            call: async <T>(request: HostRequest) => {
                asked.push(request);
                const made = answer(request);
                return { ok: made.ok, status: made.status, body: made.body as T };
            },
        },
    };
}

const plan: LauncherAnswer = {
    schema: 1,
    ready: true,
    install: "C:/Wizard101",
    revision: "r806919",
    program: "C:/Wizard101/Bin/WizardGraphicalClient.exe",
    run_folder: "C:/run",
    log_file: "C:/run/launcher.log",
    host: "127.0.0.1",
    port: 12000,
    locale: "en-US",
    window: "1280x720",
    arguments: [],
    command: "WizardGraphicalClient.exe",
};

describe("the launcher channel over the shared host", () => {
    it("asks each question at its own path with what it was given", async () => {
        const { host, asked } = fakeHost(() => ({ ok: true, status: 200, body: plan }));
        const channel = hostChannel(host);

        await channel.describe({ host: "10.0.0.4" });
        await channel.start({ character: "Wolf" });

        expect(asked[0]).toMatchObject({ path: "/launcher/describe", method: "POST", body: { host: "10.0.0.4" } });
        expect(asked[1]).toMatchObject({ path: "/launcher/start", method: "POST", body: { character: "Wolf" } });
    });

    it("gives back what the launcher answered", async () => {
        const { host } = fakeHost(() => ({ ok: true, status: 200, body: plan }));
        const answer = await hostChannel(host).describe({});
        expect(answer.ready).toBe(true);
        expect(answer).toMatchObject({ revision: "r806919" });
    });

    it("reads a host that refuses as a refusal the window can show", async () => {
        const { host } = fakeHost(() => ({ ok: false, status: 503, body: null }));
        const answer = await hostChannel(host).describe({});
        expect(answer.ready).toBe(false);
        expect(answer).toMatchObject({ reason: NoLauncher });
    });

    it("reads a host that answers with nothing the same way", async () => {
        const { host } = fakeHost(() => ({ ok: true, status: 200, body: null }));
        const answer = await hostChannel(host).describe({});
        expect(answer.ready).toBe(false);
        expect(answer).toMatchObject({ reason: NoLauncher });
    });

    it("always gives the steps back as a list", async () => {
        const withSteps = fakeHost(() => ({
            ok: true,
            status: 200,
            body: {
                state: "setting-up",
                server: { status: "offline", address: "127.0.0.1:12000" },
                steps: [{ id: "install", label: "Find the installation", state: "done", word: "Found" }],
            },
        }));
        const answered = await hostChannel(withSteps.host).steps?.();
        expect(answered?.steps).toHaveLength(1);
        expect(answered?.state).toBe("setting-up");
        expect(answered?.server).toEqual({ status: "offline", address: "127.0.0.1:12000" });
        expect(withSteps.asked[0]).toMatchObject({ path: "/launcher/steps", method: "GET" });

        const withNone = fakeHost(() => ({ ok: true, status: 200, body: {} }));
        expect((await hostChannel(withNone.host).steps?.())?.steps, "no steps is an empty list, never undefined").toEqual([]);

        const refusing = fakeHost(() => ({ ok: false, status: 500, body: null }));
        expect((await hostChannel(refusing.host).steps?.())?.steps).toEqual([]);

        const strange = fakeHost(() => ({ ok: true, status: 200, body: { state: "dancing", server: { status: "maybe" }, steps: [] } }));
        const read = await hostChannel(strange.host).steps?.();
        expect(read?.state, "a state the launcher does not send is not read as one").toBe("checking");
        expect(read?.server).toEqual({ status: "unknown", address: "" });
    });

    it("carries the settings when looking again and no folder when opening the logs", async () => {
        const { host, asked } = fakeHost((request) =>
            request.path === "/launcher/logs/open"
                ? { ok: true, status: 200, body: { opened: true, folder: "C:/run" } }
                : { ok: true, status: 200, body: { state: "checking", server: { status: "unknown", address: "" }, steps: [] } },
        );
        const channel = hostChannel(host);

        await channel.again?.({ client_dir: "D:/Games/Wizard101" });
        expect(asked[0]).toMatchObject({ path: "/launcher/steps/again", method: "POST", body: { client_dir: "D:/Games/Wizard101" } });

        expect(await channel.openLogs?.()).toEqual({ opened: true, folder: "C:/run" });
        expect(asked[1]).toMatchObject({ path: "/launcher/logs/open", method: "POST" });
        expect(asked[1].body, "the page never names the folder; the launcher opens its own").toBeUndefined();

        const refusing = fakeHost(() => ({ ok: false, status: 500, body: null }));
        expect(await hostChannel(refusing.host).openLogs?.()).toEqual({ opened: false, reason: NoLauncher });
    });

    it("reads the state an answer names, and retry or play when it names none", () => {
        expect(stateOf({ schema: 1, ready: false, reason: "x", state: "locate" })).toBe("locate");
        expect(stateOf({ schema: 1, ready: false, reason: "x" })).toBe("retry");
        expect(stateOf(plan)).toBe("play");
        expect(stateOf({ ...(plan as LauncherPlan), state: "playing" })).toBe("playing");
    });

    it("shapes the profile the header shows from the plan and the remembered name only", () => {
        expect(profileOf(null, "")).toEqual({ server: null, account: null });
        expect(profileOf(plan as LauncherPlan, "wolf")).toEqual({
            server: { id: "127.0.0.1:12000", name: "127.0.0.1", host: "127.0.0.1", port: 12000 },
            account: "wolf",
        });
    });
});
