/*
 * Project Ambrose by Imjustchico
 * Tests the launcher window in a real browser, in Chromium for WebView2 and in WebKit for WebKitGTK, against a channel the test answers as a launcher would: every screen, ready to play, the first run, settings and the failure, passes the accessibility gate with every control carrying its label, and each is used from the keyboard alone, reaching Play, settings and back, a changed setting and Look again with Tab and Enter and no pointer.
 */

import axe from "axe-core";
import { flushSync, mount, unmount } from "svelte";
import { afterEach, describe, expect, it } from "vitest";
import { userEvent } from "vitest/browser";
import App from "./App.svelte";
import type { LauncherAnswer, LauncherChannel, LauncherRequest, SetupStep } from "./lib/channel";
import { LauncherState } from "./lib/launcher.svelte";

type Rig = { channel: LauncherChannel; started: LauncherRequest[]; described: LauncherRequest[]; restarted: number; answer: LauncherAnswer; steps: SetupStep[] };

const plan: LauncherAnswer = {
    schema: 1,
    ready: true,
    install: "C:/Games/Wizard101",
    revision: "r806919.Wizard_1_610",
    program: "C:/Games/Wizard101/Bin/WizardGraphicalClient.exe",
    run_folder: "C:/Users/wiz/AppData/Local/ProjectAmbrose/client/r806919",
    log_file: "C:/Users/wiz/AppData/Local/ProjectAmbrose/client/r806919/client.log",
    host: "127.0.0.1",
    port: 12000,
    locale: "en-US",
    arguments: [],
    command: "WizardGraphicalClient.exe -L 127.0.0.1 12000",
};

function rig(): Rig {
    const made: Rig = {
        started: [],
        described: [],
        restarted: 0,
        answer: plan,
        steps: [],
        channel: {
            describe: async (request) => {
                made.described.push(request);
                return made.answer;
            },
            start: async (request) => {
                made.started.push(request);
                return made.answer;
            },
            steps: async () => made.steps,
            again: async () => {
                made.restarted++;
                return made.steps;
            },
        },
    };
    return made;
}

const memory = { get: () => "", set: () => undefined };

let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

function settle() {
    return new Promise((done) => setTimeout(done, 30));
}

async function open(made: Rig, then?: (state: LauncherState) => void, pause: (milliseconds: number) => Promise<void> = async () => undefined): Promise<LauncherState> {
    host = document.createElement("div");
    document.body.appendChild(host);
    const launcher = new LauncherState(made.channel, memory, pause);
    page = mount(App, { target: host, props: { launcher } });
    await settle();
    then?.(launcher);
    flushSync();
    await settle();
    return launcher;
}

async function gate(): Promise<void> {
    const results = await axe.run(host, { resultTypes: ["violations"] });
    const found = results.violations.map((violation) => `${violation.id}: ${violation.nodes.map((node) => node.target.join(" ")).join(", ")}`);
    expect(found).toEqual([]);
    for (const control of host.querySelectorAll<HTMLElement>("button, input, a[href], select, textarea")) {
        const name = control.getAttribute("aria-label") ?? control.textContent?.trim() ?? "";
        const labelled = name !== "" || (control.id !== "" && host.querySelector(`label[for="${control.id}"]`) !== null);
        expect(labelled, control.outerHTML).toBe(true);
    }
}

async function tabTo(name: string): Promise<void> {
    for (let step = 0; step < 30; step++) {
        const focused = document.activeElement as HTMLElement | null;
        const label = focused?.getAttribute("aria-label") ?? focused?.textContent?.trim() ?? "";
        const labelledBy = focused?.id ? (host.querySelector(`label[for="${focused.id}"]`)?.textContent?.trim() ?? "") : "";
        if (focused && host.contains(focused) && (label === name || labelledBy === name)) return;
        await userEvent.keyboard("{Tab}");
    }
    throw new Error(`Tab never reached ${name}`);
}

afterEach(() => {
    if (page) unmount(page);
    page = null;
    host?.remove();
});

describe("the launcher window from the keyboard alone", () => {
    it("reaches Play with Tab and starts it with Enter, and the ready screen passes the gate", async () => {
        const made = rig();
        const launcher = await open(made);
        expect(launcher.screen).toBe("ready");
        await gate();

        await tabTo("Play");
        await userEvent.keyboard("{Enter}");
        await settle();
        expect(made.started).toHaveLength(1);
    });

    it("opens settings, changes one, and goes back, all from the keyboard, and settings passes the gate", async () => {
        const made = rig();
        const launcher = await open(made);

        await tabTo("Settings");
        await userEvent.keyboard("{Enter}");
        await settle();
        flushSync();
        expect(launcher.screen).toBe("settings");
        await gate();

        await tabTo("Port");
        await userEvent.keyboard("12001");
        await tabTo("Use these");
        await userEvent.keyboard("{Enter}");
        await settle();
        expect(made.described.at(-1)).toEqual({ port: "12001" });
        expect(launcher.screen).toBe("ready");

        await tabTo("Settings");
        await userEvent.keyboard("{Enter}");
        await settle();
        flushSync();
        await tabTo("Back");
        await userEvent.keyboard("{Enter}");
        await settle();
        expect(launcher.screen).toBe("ready");
    });

    it("shows the first run with every step labelled, and it passes the gate", async () => {
        const made = rig();
        made.steps = [
            { id: "install", label: "Find your Wizard101 installation", state: "done", word: "Wizard101 r806919 at C:/Games/Wizard101" },
            { id: "server", label: "Reach the login server", state: "doing", word: "Waiting for 127.0.0.1:12000 to open, try 3 of 30" },
        ];
        const launcher = await open(made, undefined, () => new Promise<void>(() => undefined));
        expect(launcher.screen).toBe("first-run");
        expect(host.textContent).toContain("Reach the login server");
        expect(host.textContent).toContain("try 3 of 30");
        await gate();
    });

    it("names what went wrong, and Look again is reached and pressed from the keyboard", async () => {
        const made = rig();
        made.steps = [{ id: "server", label: "Reach the login server", state: "wrong", word: "Nothing answered at 127.0.0.1:12000 after 30 tries. Start the Ambrose servers, then look again" }];
        const launcher = await open(made);
        expect(launcher.screen).toBe("failed");
        expect(host.textContent).toContain("Start the Ambrose servers");
        await gate();

        made.steps = [];
        await tabTo("Look again");
        await userEvent.keyboard("{Enter}");
        await settle();
        expect(made.restarted).toBe(1);
        expect(launcher.screen).toBe("ready");
    });
});
