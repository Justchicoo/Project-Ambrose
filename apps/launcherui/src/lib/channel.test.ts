/*
 * Project Ambrose by Imjustchico
 * Tests the channel as it sits on the host every surface shares: each question reaches its own path with what it was asked to send, a host that refuses is read as a refusal the window can show rather than as an answer, a host that replies with nothing is treated the same way, and the steps of a first run come back as a list even when there are none, so the page never has to guard against a shape the channel could have prevented.
 */

import { describe, expect, it } from "vitest";
import type { Host, HostRequest } from "@ambrose/ui";
import { hostChannel, NoLauncher, type LauncherAnswer } from "./channel";

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
            body: { steps: [{ id: "install", label: "Find the installation", state: "done", word: "Found" }] },
        }));
        expect(await hostChannel(withSteps.host).steps?.()).toHaveLength(1);
        expect(withSteps.asked[0]).toMatchObject({ path: "/launcher/steps", method: "GET" });

        const withNone = fakeHost(() => ({ ok: true, status: 200, body: {} }));
        expect(await hostChannel(withNone.host).steps?.(), "no steps is an empty list, never undefined").toEqual([]);

        const refusing = fakeHost(() => ({ ok: false, status: 500, body: null }));
        expect(await hostChannel(refusing.host).steps?.()).toEqual([]);
    });
});
