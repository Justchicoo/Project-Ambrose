/*
 * Project Ambrose by Imjustchico
 * Tests the launcher window's own logic against a channel a test answers as a launcher would: which screen each answer leads to, what the window sends when the operator plays or changes a setting, that a refusal shows the reason the launcher gave rather than a shape of its own, that a first run still working through its steps holds the first-run screen until it is finished, that the account is remembered by name and the password never is, that a remembered name on its own is never sent as a login, and that nothing the operator changes is decided here rather than sent back to be decided.
 */

import { describe, expect, it } from "vitest";
import type { LauncherAnswer, LauncherChannel, LauncherRequest, SetupStep } from "./channel";
import { LauncherState } from "./launcher.svelte";

function plan(over: Partial<Record<string, unknown>> = {}): LauncherAnswer {
    return {
        schema: 1,
        ready: true,
        install: "C:/ProgramData/KingsIsle Entertainment/Wizard101",
        revision: "r806919",
        program: "C:/ProgramData/KingsIsle Entertainment/Wizard101/Bin/WizardGraphicalClient.exe",
        run_folder: "C:/Users/someone/AppData/Local/ProjectAmbrose/client",
        log_file: "C:/Users/someone/AppData/Local/ProjectAmbrose/client/launcher.log",
        host: "127.0.0.1",
        port: 12000,
        locale: "en-US",
        arguments: ["-L", "127.0.0.1", "12000", "-P", "0"],
        command: "WizardGraphicalClient.exe -L 127.0.0.1 12000 -P 0",
        ...over,
    } as LauncherAnswer;
}

function refusal(reason: string): LauncherAnswer {
    return { schema: 1, ready: false, reason };
}

type Rig = {
    channel: LauncherChannel;
    described: LauncherRequest[];
    started: LauncherRequest[];
    answer: LauncherAnswer;
    steps: SetupStep[] | null;
};

function rig(): Rig {
    const made: Rig = {
        described: [],
        started: [],
        answer: plan(),
        steps: null,
        channel: {
            describe: async (request) => {
                made.described.push(request);
                return made.answer;
            },
            start: async (request) => {
                made.started.push(request);
                return made.answer;
            },
            steps: async () => made.steps ?? [],
        },
    };
    return made;
}

function memory() {
    let held = "";
    return {
        get: () => held,
        set: (name: string) => {
            held = name;
        },
        peek: () => held,
    };
}

describe("the launcher window", () => {
    it("shows ready to play when the launcher has a plan", async () => {
        const made = rig();
        made.steps = [{ id: "install", label: "Find the installation", state: "done", word: "Found" }];
        const state = new LauncherState(made.channel);

        await state.look();

        expect(state.screen).toBe("ready");
        expect(state.ready).toBe(true);
        expect(state.plan?.revision).toBe("r806919");
        expect(state.plan?.command).toContain("WizardGraphicalClient.exe");
        expect(state.reason).toBe("");
    });

    it("shows the failure screen with the reason the launcher gave", async () => {
        const made = rig();
        made.answer = refusal("no Wizard101 installation was found");
        const state = new LauncherState(made.channel);

        await state.look();

        expect(state.screen).toBe("failed");
        expect(state.ready).toBe(false);
        expect(state.reason).toBe("no Wizard101 installation was found");
        expect(state.plan).toBeNull();
    });

    it("holds the first run screen while a step is still going", async () => {
        const made = rig();
        made.steps = [
            { id: "install", label: "Find the installation", state: "done", word: "Found" },
            { id: "dump", label: "Build the type dump", state: "doing", word: "2198 classes so far" },
        ];
        const state = new LauncherState(made.channel);

        await state.look();

        expect(state.screen).toBe("first-run");
        expect(state.steps).toHaveLength(2);
        expect(state.steps[1].word).toBe("2198 classes so far");
        expect(made.described, "there is nothing to describe while the setup is still working").toHaveLength(0);
    });

    it("names the step that went wrong rather than saying only that something did", async () => {
        const made = rig();
        made.steps = [{ id: "dump", label: "Build the type dump", state: "wrong", word: "the client program could not be read" }];
        const state = new LauncherState(made.channel);

        await state.look();

        expect(state.screen).toBe("failed");
        expect(state.reason).toBe("the client program could not be read");
    });

    it("sends the account with the play request and forgets the password afterwards", async () => {
        const made = rig();
        const state = new LauncherState(made.channel);
        await state.look();

        state.account = "wolf";
        state.password = "a-secret";
        await state.play();

        expect(made.started).toHaveLength(1);
        expect(made.started[0].user).toEqual({ user_id: "wolf", key: "a-secret", name: "wolf" });
        expect(state.password, "a launcher that keeps a password becomes somewhere to steal one from").toBe("");
        expect(state.starting).toBe(false);
    });

    it("remembers the account by name and never the password", async () => {
        const made = rig();
        const held = memory();
        const first = new LauncherState(made.channel, held);
        await first.look();
        first.account = "wolf";
        first.password = "a-secret";
        await first.play();

        const second = new LauncherState(made.channel, held);
        expect(second.account, "the next time the window opens, the name is already there").toBe("wolf");
        expect(second.password).toBe("");
        expect(held.peek()).toBe("wolf");
        expect(held.peek(), "nothing that was remembered may be the password").not.toContain("a-secret");
    });

    it("does not send a remembered name on its own as a login", async () => {
        const made = rig();
        const held = memory();
        held.set("wolf");
        const state = new LauncherState(made.channel, held);
        await state.look();

        await state.play();

        expect(made.started).toHaveLength(1);
        expect(
            made.started[0].user,
            "a name with no key is refused by the launcher, so a remembered name alone must not be sent as one",
        ).toBeUndefined();
        expect(state.screen).toBe("ready");
    });

    it("sends a changed setting back to be decided rather than deciding it", async () => {
        const made = rig();
        const state = new LauncherState(made.channel);
        await state.look();

        made.answer = plan({ host: "10.0.0.4", port: 12010 });
        await state.apply({ host: "10.0.0.4", port: "12010" });

        expect(made.described.at(-1)).toMatchObject({ host: "10.0.0.4", port: "12010" });
        expect(state.plan?.host, "the window shows what the launcher answered, not what it typed").toBe("10.0.0.4");
        expect(state.plan?.port).toBe(12010);
    });

    it("keeps settings across changes so one does not undo another", async () => {
        const made = rig();
        const state = new LauncherState(made.channel);
        await state.look();

        await state.apply({ host: "10.0.0.4" });
        await state.apply({ locale: "de-DE" });

        expect(made.described.at(-1)).toMatchObject({ host: "10.0.0.4", locale: "de-DE" });
    });

    it("refuses to start twice at once", async () => {
        const made = rig();
        let settle: (answer: LauncherAnswer) => void = () => {};
        made.channel.start = (request) => {
            made.started.push(request);
            return new Promise<LauncherAnswer>((resolve) => {
                settle = resolve;
            });
        };
        const state = new LauncherState(made.channel);
        await state.look();

        const first = state.play();
        expect(state.starting).toBe(true);
        await state.play();
        expect(made.started, "a second Play while the first is still going must not start the client twice").toHaveLength(1);

        settle(plan());
        await first;
        expect(state.starting).toBe(false);
    });

    it("leaves settings back to where the operator came from", async () => {
        const made = rig();
        const state = new LauncherState(made.channel);
        await state.look();

        state.open("settings");
        expect(state.screen).toBe("settings");
        state.close();
        expect(state.screen).toBe("ready");

        made.answer = refusal("no install");
        await state.look();
        state.open("settings");
        state.close();
        expect(state.screen, "with nothing to play, leaving settings goes back to the failure and not to a screen that would lie").toBe(
            "failed",
        );
    });

    it("says so when the window was opened with no launcher behind it", async () => {
        const state = new LauncherState({
            describe: async () => refusal("the launcher did not answer"),
            start: async () => refusal("the launcher did not answer"),
        });

        await state.look();

        expect(state.screen).toBe("failed");
        expect(state.reason).toBe("the launcher did not answer");
    });
});

describe("the guarantee the window carries", () => {
    it("says what the launcher will not do", () => {
        const state = new LauncherState(rig().channel);
        expect(state.guarantee).toContain("writes nothing into it");
        expect(state.guarantee).toContain("never starts KingsIsle's launcher");
    });
});
