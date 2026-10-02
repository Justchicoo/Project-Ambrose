/*
 * Project Ambrose by Imjustchico
 * Tests the launcher window in a real browser, in Chromium for WebView2 and in WebKit for WebKitGTK, against a channel the test answers as a launcher would: every screen passes the accessibility gate with every control carrying its label, and each is used from the keyboard alone. The ready screen shows the server, the client and the window size on its bar beside the server's status as a word, and Play is reached with Tab and pressed with Enter; the one primary button says what the launcher's state says, Setting up while the steps run as a numbered list with each state in words, Locate when no install was found, which opens settings at the installation field, Retry after a failure, and Playing once the game was started; Play cannot be pressed while the server is offline and the reason sits beside it; the progress strip names the current step and the counts; the failure names its cause, opens the log folder by asking the launcher, and copies diagnostics, showing them selected when the clipboard is refused; settings names the installation, has one Back, in the header, and fits a 1024 by 640 window without scrolling; and the skip link takes no room on screen until it is focused.
 */

import axe from "axe-core";
import { flushSync, mount, unmount } from "svelte";
import { afterEach, describe, expect, it } from "vitest";
import { page as browser, userEvent } from "vitest/browser";
import App from "./App.svelte";
import type { LauncherAnswer, LauncherChannel, LauncherRequest, SetupAnswer, SetupStep } from "./lib/channel";
import { LauncherState } from "./lib/launcher.svelte";

type Rig = {
    channel: LauncherChannel;
    started: LauncherRequest[];
    described: LauncherRequest[];
    again: LauncherRequest[];
    logs: number;
    answer: LauncherAnswer;
    setup: SetupAnswer;
};

const plan: LauncherAnswer = {
    schema: 1,
    ready: true,
    state: "play",
    install: "C:/Games/Wizard101",
    revision: "r806919.Wizard_1_610",
    program: "C:/Games/Wizard101/Bin/WizardGraphicalClient.exe",
    run_folder: "C:/Users/wiz/AppData/Local/ProjectAmbrose/client/r806919",
    log_file: "C:/Users/wiz/AppData/Local/ProjectAmbrose/client/r806919/client.log",
    host: "127.0.0.1",
    port: 12000,
    locale: "en-US",
    window: "1600x900",
    arguments: [],
    command: "WizardGraphicalClient.exe -L 127.0.0.1 12000",
};

const found: SetupStep = {
    id: "install",
    label: "Find your Wizard101 installation",
    state: "done",
    word: "Wizard101 r806919 at C:/Games/Wizard101",
};
const written: SetupStep = { id: "run-folder", label: "Ready the folder the game runs from", state: "done", word: "C:/run with 4 files" };
const reached: SetupStep = {
    id: "server",
    label: "Reach the login server",
    state: "done",
    word: "127.0.0.1:12000 is open, answered on try 1",
};

function setup(state: SetupAnswer["state"], steps: SetupStep[], status: "online" | "offline" | "unknown"): SetupAnswer {
    return { state, server: { status, address: "127.0.0.1:12000" }, steps };
}

const finished = setup("play", [found, written, reached], "online");

function rig(): Rig {
    const made: Rig = {
        started: [],
        described: [],
        again: [],
        logs: 0,
        answer: plan,
        setup: finished,
        channel: {
            describe: async (request) => {
                made.described.push(request);
                return made.answer;
            },
            start: async (request) => {
                made.started.push(request);
                return made.answer;
            },
            steps: async () => made.setup,
            again: async (request) => {
                made.again.push(request);
                return made.setup;
            },
            openLogs: async () => {
                made.logs++;
                return { opened: true, folder: "C:/Users/wiz/AppData/Local/ProjectAmbrose/client/r806919" };
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

async function open(
    made: Rig,
    then?: (state: LauncherState) => void,
    pause: (milliseconds: number) => Promise<void> = async () => undefined,
): Promise<LauncherState> {
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
    const found = results.violations.map(
        (violation) => `${violation.id}: ${violation.nodes.map((node) => node.target.join(" ")).join(", ")}`,
    );
    expect(found).toEqual([]);
    for (const control of host.querySelectorAll<HTMLElement>("button, input, a[href], select, textarea")) {
        const name = control.getAttribute("aria-label") ?? control.textContent?.trim() ?? "";
        const labelled = name !== "" || (control.id !== "" && host.querySelector(`label[for="${control.id}"]`) !== null);
        expect(labelled, control.outerHTML).toBe(true);
    }
}

async function tabTo(name: string): Promise<void> {
    for (let step = 0; step < 40; step++) {
        const focused = document.activeElement as HTMLElement | null;
        const label = focused?.getAttribute("aria-label") ?? focused?.textContent?.trim() ?? "";
        const labelledBy = focused?.id ? (host.querySelector(`label[for="${focused.id}"]`)?.textContent?.trim() ?? "") : "";
        if (focused && host.contains(focused) && (label === name || labelledBy === name)) return;
        await userEvent.keyboard("{Tab}");
    }
    throw new Error(`Tab never reached ${name}`);
}

function button(name: string): HTMLButtonElement | undefined {
    return [...host.querySelectorAll<HTMLButtonElement>("button")].find((control) => control.textContent?.trim() === name);
}

afterEach(async () => {
    if (page) unmount(page);
    page = null;
    host?.remove();
    await browser.viewport(1280, 800);
});

describe("the launcher window from the keyboard alone", () => {
    it("reaches Play with Tab and starts it with Enter, and the ready screen passes the gate", async () => {
        const made = rig();
        const launcher = await open(made);
        expect(launcher.state).toBe("play");
        expect(host.textContent).toContain("Ready to play");
        expect(host.textContent).toContain("127.0.0.1:12000");
        expect(host.textContent).toContain("r806919.Wizard_1_610");
        expect(host.textContent, "the bar shows the window size the client starts at").toContain("1600x900");
        expect(host.querySelector('[role="status"]')?.textContent?.trim(), "the server's status is a word beside the dot").toBe("Online");
        expect(host.textContent, "the strip counts the steps").toContain("Ready · 3 of 3");
        expect(host.querySelector("svg[aria-hidden='true']"), "the hero art is decoration the reader skips").not.toBeNull();
        await gate();

        await tabTo("Play");
        await userEvent.keyboard("{Enter}");
        await settle();
        expect(made.started).toHaveLength(1);
    });

    it("keeps Play from being pressed while the server is offline and says why", async () => {
        const made = rig();
        made.setup = setup("play", [found, written, reached], "offline");
        await open(made);
        expect(button("Play")?.disabled).toBe(true);
        expect(host.textContent).toContain("Offline");
        expect(host.textContent).toContain("The login server is not answering");
        expect(button("Play")?.getAttribute("aria-describedby")).toBe("launcher-why");
        await gate();
    });

    it("opens settings, changes one, and goes back, all from the keyboard, and settings passes the gate", async () => {
        const made = rig();
        const launcher = await open(made);

        await tabTo("Settings");
        await userEvent.keyboard("{Enter}");
        await settle();
        flushSync();
        expect(launcher.screen).toBe("settings");
        const installation = [...host.querySelectorAll("dt")].find((term) => term.textContent?.trim() === "Installation");
        expect(installation?.nextElementSibling?.textContent?.trim(), "settings names the install the launcher found").toBe(
            "C:/Games/Wizard101",
        );
        expect(host.querySelector<HTMLInputElement>("#launcher-size")?.placeholder).toBe("1600x900");
        expect(
            [...host.querySelectorAll("button")].filter((control) => control.textContent?.trim() === "Back"),
            "settings has one Back, in the header",
        ).toHaveLength(1);
        await gate();

        await tabTo("Port");
        await userEvent.keyboard("12001");
        await tabTo("Use these");
        await userEvent.keyboard("{Enter}");
        await settle();
        expect(made.again.at(-1), "saving looks again from the first step with the new value").toEqual({ port: "12001" });
        expect(made.described.at(-1)).toEqual({ port: "12001" });
        expect(launcher.screen).toBe("home");

        await tabTo("Settings");
        await userEvent.keyboard("{Enter}");
        await settle();
        flushSync();
        await tabTo("Back");
        await userEvent.keyboard("{Enter}");
        await settle();
        expect(launcher.screen).toBe("home");
    });

    it("fits settings and the ready screen in a 1024 by 640 window without scrolling", async () => {
        await browser.viewport(1024, 600);
        const made = rig();
        const launcher = await open(made);
        const hero = host.querySelector("main section") as HTMLElement;
        expect(document.documentElement.scrollHeight).toBeLessThanOrEqual(window.innerHeight);
        expect(hero.scrollHeight).toBeLessThanOrEqual(hero.clientHeight);

        launcher.open("settings");
        flushSync();
        await settle();
        expect(document.documentElement.scrollHeight, "settings fits without scrolling").toBeLessThanOrEqual(window.innerHeight);
        expect(hero.scrollHeight).toBeLessThanOrEqual(hero.clientHeight);
    });

    it("shows the first run as numbered steps with their states in words, and it passes the gate", async () => {
        const made = rig();
        made.setup = setup(
            "setting-up",
            [
                found,
                { ...written, state: "doing", word: "Writing the configuration the game reads" },
                { ...reached, state: "waiting", word: "Waits for the run folder" },
            ],
            "unknown",
        );
        const launcher = await open(made, undefined, () => new Promise<void>(() => undefined));
        expect(launcher.state).toBe("setting-up");
        const steps = host.querySelector("ol[aria-label='First run']");
        expect(steps?.querySelectorAll("li")).toHaveLength(3);
        expect(steps?.textContent).toContain("Step 1:");
        expect(steps?.textContent).toContain("Done");
        expect(steps?.textContent).toContain("Active");
        expect(steps?.textContent).toContain("Waiting");
        expect(host.textContent, "the strip names the current step and the counts").toContain(
            "Setting up · 1 of 3: Ready the folder the game runs from",
        );
        expect(host.textContent).toContain("33%");
        expect(host.textContent, "the copy says the checks run every time, not once").not.toContain("This happens once");
        expect(button("Setting up")?.disabled).toBe(true);
        await gate();
    });

    it("names what went wrong, opens the log folder, copies diagnostics, and Retry is pressed from the keyboard", async () => {
        const made = rig();
        made.setup = setup(
            "retry",
            [
                found,
                written,
                {
                    ...reached,
                    state: "wrong",
                    word: "Nothing answered at 127.0.0.1:12000 after 30 tries. Start the Ambrose servers, then look again",
                },
            ],
            "offline",
        );
        const launcher = await open(made);
        expect(launcher.state).toBe("retry");
        expect(host.textContent).toContain("The login server did not answer");
        expect(host.textContent).toContain("Start the Ambrose servers");
        expect(host.textContent).toContain("Offline");
        expect(host.textContent).toContain("Failed · step 3 of 3: Reach the login server");
        await gate();

        await tabTo("Open log folder");
        await userEvent.keyboard("{Enter}");
        await settle();
        expect(made.logs).toBe(1);
        expect(host.textContent).toContain("Opened C:/Users/wiz/AppData/Local/ProjectAmbrose/client/r806919");

        const clipboard = Object.getOwnPropertyDescriptor(Navigator.prototype, "clipboard");
        Object.defineProperty(navigator, "clipboard", {
            configurable: true,
            value: { writeText: () => Promise.reject(new Error("refused")) },
        });
        try {
            await tabTo("Copy diagnostics");
            await userEvent.keyboard("{Enter}");
            await settle();
            flushSync();
            const box = host.querySelector<HTMLTextAreaElement>("#launcher-diagnostics");
            expect(box, "a refused clipboard shows the diagnostics to copy by hand").not.toBeNull();
            expect(box?.value).toContain("State: retry");
            expect(document.activeElement).toBe(box);
            expect(box?.selectionEnd).toBe(box?.value.length);
            await gate();
        } finally {
            delete (navigator as unknown as Record<string, unknown>).clipboard;
            if (clipboard) Object.defineProperty(Navigator.prototype, "clipboard", clipboard);
        }

        made.setup = finished;
        await tabTo("Retry");
        await userEvent.keyboard("{Enter}");
        await settle();
        expect(made.again).toHaveLength(1);
        expect(launcher.state).toBe("play");
    });

    it("offers Locate when no install was found, which opens settings at the installation field", async () => {
        const made = rig();
        made.setup = setup(
            "locate",
            [
                {
                    ...found,
                    state: "wrong",
                    word: "No Wizard101 installation was found. Name the installation with ClientDir in launcher.conf or with --client, then look again",
                },
                { ...written, state: "waiting" },
                { ...reached, state: "waiting" },
            ],
            "unknown",
        );
        const launcher = await open(made);
        expect(host.textContent).toContain("Your game installation was not found");
        expect(host.textContent).toContain("No installation found");
        await gate();

        await tabTo("Locate");
        await userEvent.keyboard("{Enter}");
        await settle();
        flushSync();
        expect(launcher.screen).toBe("settings");
        expect(document.activeElement?.id).toBe("launcher-client");
    });

    it("says the game was started and offers to check again once it is playing", async () => {
        const made = rig();
        made.setup = setup("playing", [found, written, reached], "online");
        made.answer = { ...(plan as Extract<LauncherAnswer, { ready: true }>), state: "playing" };
        await open(made);
        expect(host.textContent).toContain("The game was started");
        expect(button("Playing")?.disabled).toBe(true);
        expect(button("Check again")).toBeDefined();
        await gate();
    });

    it("keeps the skip link off the screen until it is focused", async () => {
        const made = rig();
        await open(made);
        const skip = [...document.querySelectorAll<HTMLAnchorElement>("a[href='#ambrose-main']")].find((link) => host.contains(link));
        expect(skip).toBeDefined();
        const hidden = skip!.getBoundingClientRect();
        expect(hidden.width * hidden.height, "an unfocused skip link takes no visible room").toBeLessThanOrEqual(1);
        skip!.focus();
        flushSync();
        const shown = skip!.getBoundingClientRect();
        expect(shown.width).toBeGreaterThan(20);
        expect(shown.top).toBeGreaterThanOrEqual(0);
    });
});
