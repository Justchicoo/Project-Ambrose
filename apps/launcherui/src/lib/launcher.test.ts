/*
 * Project Ambrose by Imjustchico
 * Tests the launcher window's own logic against a channel a test answers as a launcher would: the launch state the window draws is the one the launcher answered, from the steps, a plan or a refusal, and never one the page worked out; the install and window size a plan carries reach the window; a refusal shows the reason the launcher gave; a first run still working through its steps is followed step by step until the state settles, and a launch is followed until the launcher says the game is playing; a failed step names what went wrong and holds Locate or Retry; retrying starts the steps over carrying any changed setting; Play is sent only from the play state and only once at a time; the diagnostics carry the state, the cause, every step and the plan's facts; opening the log folder asks the launcher and shows its answer; the profile is filled only from the plan and the remembered name; the account is remembered by name and the password never is, and a remembered name on its own is never sent as a login; an answer that cannot be read is a failure rather than a wait; and nothing the operator changes is decided here rather than sent back to be decided.
 */

import { describe, expect, it } from "vitest";
import type { LaunchState, LauncherAnswer, LauncherChannel, LauncherRequest, SetupAnswer, SetupStep } from "./channel";
import { LauncherState } from "./launcher.svelte";

function plan(over: Partial<Record<string, unknown>> = {}): LauncherAnswer {
    return {
        schema: 1,
        ready: true,
        state: "play",
        install: "C:/ProgramData/KingsIsle Entertainment/Wizard101",
        revision: "r806919",
        program: "C:/ProgramData/KingsIsle Entertainment/Wizard101/Bin/WizardGraphicalClient.exe",
        run_folder: "C:/Users/someone/AppData/Local/ProjectAmbrose/client",
        log_file: "C:/Users/someone/AppData/Local/ProjectAmbrose/client/launcher.log",
        host: "127.0.0.1",
        port: 12000,
        locale: "en-US",
        window: "1280x720",
        arguments: ["-L", "127.0.0.1", "12000", "-P", "0"],
        command: "WizardGraphicalClient.exe -L 127.0.0.1 12000 -P 0 -U tester ********",
        ...over,
    } as LauncherAnswer;
}

function refusal(reason: string, state?: LaunchState): LauncherAnswer {
    return { schema: 1, ready: false, state, reason };
}

const found: SetupStep = {
    id: "install",
    label: "Find your Wizard101 installation",
    state: "done",
    word: "Wizard101 r806919 at C:/Games/Wizard101",
};
const written: SetupStep = { id: "run-folder", label: "Ready the folder the game runs from", state: "done", word: "C:/run with 4 files" };
const open: SetupStep = {
    id: "server",
    label: "Reach the login server",
    state: "done",
    word: "127.0.0.1:12000 is open, answered on try 1",
};

function setup(state: LaunchState, steps: SetupStep[], status: "online" | "offline" | "unknown" = "unknown"): SetupAnswer {
    return { state, server: { status, address: "127.0.0.1:12000" }, steps };
}

const finished = setup("play", [found, written, open], "online");

type Rig = {
    channel: LauncherChannel;
    described: LauncherRequest[];
    started: LauncherRequest[];
    again: LauncherRequest[];
    answer: LauncherAnswer;
    setup: SetupAnswer | null;
};

function rig(): Rig {
    const made: Rig = {
        described: [],
        started: [],
        again: [],
        answer: plan(),
        setup: null,
        channel: {
            describe: async (request) => {
                made.described.push(request);
                return made.answer;
            },
            start: async (request) => {
                made.started.push(request);
                return made.answer;
            },
            steps: async () => made.setup ?? setup("checking", []),
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
    it("draws play when the launcher's steps are done and it has a plan", async () => {
        const made = rig();
        made.setup = finished;
        const state = new LauncherState(made.channel);

        await state.look();

        expect(state.state).toBe("play");
        expect(state.server.status).toBe("online");
        expect(state.ready).toBe(true);
        expect(state.plan?.revision).toBe("r806919");
        expect(state.plan?.window, "the window size is the launcher's own, as it will start the client").toBe("1280x720");
        expect(state.plan?.install).toBe("C:/ProgramData/KingsIsle Entertainment/Wizard101");
        expect(state.reason).toBe("");
    });

    it("draws the state a refusal carries, with the reason the launcher gave", async () => {
        const made = rig();
        made.answer = refusal("no Wizard101 installation was found", "locate");
        const state = new LauncherState(made.channel);

        await state.look();

        expect(state.state).toBe("locate");
        expect(state.failed).toBe(true);
        expect(state.reason).toBe("no Wizard101 installation was found");
        expect(state.plan).toBeNull();
    });

    it("reads a refusal that names no state as retry and a plan that names none as play", async () => {
        const made = rig();
        made.answer = refusal("the launcher did not answer");
        const state = new LauncherState(made.channel);
        await state.look();
        expect(state.state).toBe("retry");

        made.answer = plan({ state: undefined });
        await state.look();
        expect(state.state).toBe("play");
    });

    it("holds the setting-up state while a step is still going and asks for no plan", async () => {
        const made = rig();
        made.setup = setup("setting-up", [found, { ...written, state: "doing", word: "Writing the configuration the game reads" }]);
        const state = new LauncherState(made.channel);

        await state.look();

        expect(state.state).toBe("setting-up");
        expect(state.moving).toBe(true);
        expect(state.done).toBe(1);
        expect(state.current?.id).toBe("run-folder");
        expect(made.described, "there is nothing to describe while the setup is still working").toHaveLength(0);
    });

    it("follows the first run step by step until the server is open, then draws play", async () => {
        const made = rig();
        const answers: SetupAnswer[] = [
            setup("setting-up", [found, { ...written, state: "doing" }, { ...open, state: "waiting", word: "Waits for the run folder" }]),
            setup(
                "setting-up",
                [found, written, { ...open, state: "doing", word: "Waiting for 127.0.0.1:12000 to open, try 1 of 30" }],
                "offline",
            ),
            finished,
        ];
        const seen: string[] = [];
        let asked = 0;
        made.channel.steps = async () => answers[Math.min(asked++, answers.length - 1)];
        const pauses: number[] = [];
        const state = new LauncherState(made.channel, memory(), async (milliseconds) => {
            pauses.push(milliseconds);
            seen.push(`${state.state}:${state.server.status}:${state.steps.map((step) => step.state).join(",")}`);
        });

        await state.run();

        expect(seen).toEqual(["setting-up:unknown:done,doing,waiting", "setting-up:offline:done,done,doing"]);
        expect(pauses).toEqual([500, 500]);
        expect(state.state).toBe("play");
        expect(state.server.status).toBe("online");
        expect(made.described).toHaveLength(1);
    });

    it("stops following when a step goes wrong and draws retry with what it said to do", async () => {
        const made = rig();
        let asked = 0;
        made.channel.steps = async () =>
            asked++ === 0
                ? setup("setting-up", [found, written, { ...open, state: "doing", word: "try 1 of 30" }], "offline")
                : setup(
                      "retry",
                      [
                          found,
                          written,
                          {
                              ...open,
                              state: "wrong",
                              word: "Nothing answered at 127.0.0.1:12000 after 30 tries. Start the Ambrose servers, then look again",
                          },
                      ],
                      "offline",
                  );
        const state = new LauncherState(made.channel, memory(), async () => undefined);

        await state.run();

        expect(state.state).toBe("retry");
        expect(state.reason).toContain("Start the Ambrose servers");
        expect(state.current?.id).toBe("server");
        expect(made.described).toHaveLength(0);
    });

    it("starts the steps over on retry, carrying the settings the operator changed", async () => {
        const made = rig();
        made.channel.again = async (request) => {
            made.again.push(request);
            return setup("setting-up", [found, { ...written, state: "doing" }]);
        };
        made.setup = finished;
        const state = new LauncherState(made.channel, memory(), async () => undefined);

        await state.apply({ client_dir: "D:/Games/Wizard101" });

        expect(made.again).toEqual([{ client_dir: "D:/Games/Wizard101" }]);
        expect(state.state).toBe("play");
        expect(made.described.at(-1)).toMatchObject({ client_dir: "D:/Games/Wizard101" });
    });

    it("plays only from the play state and follows the launch until the launcher says playing", async () => {
        const made = rig();
        made.setup = setup("retry", [{ ...found, state: "wrong", word: "no install" }]);
        const state = new LauncherState(made.channel, memory(), async () => undefined);
        await state.look();
        await state.play();
        expect(made.started, "Retry is not Play, so nothing is started").toHaveLength(0);

        made.setup = finished;
        await state.again();
        made.answer = plan({ state: "launching" });
        let polled = 0;
        made.channel.steps = async () =>
            polled++ === 0 ? setup("launching", finished.steps, "online") : setup("playing", finished.steps, "online");
        await state.play();

        expect(made.started).toHaveLength(1);
        expect(state.state).toBe("playing");
    });

    it("writes diagnostics from the launcher's own answers", async () => {
        const made = rig();
        made.setup = setup("retry", [found, written, { ...open, state: "wrong", word: "Nothing answered" }], "offline");
        const state = new LauncherState(made.channel);
        await state.look();

        expect(state.diagnostics).toContain("State: retry");
        expect(state.diagnostics).toContain("Server: offline at 127.0.0.1:12000");
        expect(state.diagnostics).toContain("Cause: Nothing answered");
        expect(state.diagnostics).toContain("Step 3 server (wrong): Reach the login server. Nothing answered");

        made.setup = finished;
        await state.again();
        expect(state.diagnostics).toContain("Installation: C:/ProgramData/KingsIsle Entertainment/Wizard101");
        expect(state.diagnostics, "the launcher already hid the password, and nothing here puts one back").not.toContain("a-secret");
    });

    it("asks the launcher to open its own log folder and shows what it answered", async () => {
        const made = rig();
        const state = new LauncherState(made.channel);
        await state.openLogs();
        expect(state.logs).toBe("This window cannot open folders.");

        made.channel.openLogs = async () => ({ opened: true, folder: "C:/run" });
        await state.openLogs();
        expect(state.logs).toBe("Opened C:/run");

        made.channel.openLogs = async () => ({ opened: false, reason: "not made yet" });
        await state.openLogs();
        expect(state.logs).toBe("not made yet");
    });

    it("fills the profile only from the plan and the remembered name", async () => {
        const made = rig();
        const held = memory();
        const state = new LauncherState(made.channel, held);
        expect(state.profile).toEqual({ server: null, account: null });

        made.setup = finished;
        state.account = "wolf";
        await state.look();
        expect(state.profile).toEqual({
            server: { id: "127.0.0.1:12000", name: "127.0.0.1", host: "127.0.0.1", port: 12000 },
            account: "wolf",
        });
    });

    it("sends the account with the play request and forgets the password afterwards", async () => {
        const made = rig();
        made.setup = finished;
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
        made.setup = finished;
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
        made.setup = finished;
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
        expect(state.state).toBe("play");
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

    it("opens settings and leaves it without changing the state the launcher answered", async () => {
        const made = rig();
        const state = new LauncherState(made.channel);
        await state.look();

        state.open("settings");
        expect(state.screen).toBe("settings");
        state.close();
        expect(state.screen).toBe("home");
        expect(state.state).toBe("play");

        made.answer = refusal("no install", "locate");
        await state.look();
        state.open("settings");
        state.close();
        expect(state.state, "leaving settings goes back to what the launcher said, not to a screen that would lie").toBe("locate");
    });

    it("shows an answer it cannot read as a failure rather than checking forever", async () => {
        const made = rig();
        made.channel.steps = async () => {
            throw new SyntaxError("Unexpected token '<'");
        };
        const state = new LauncherState(made.channel, memory(), async () => undefined);

        await state.run();

        expect(state.state).toBe("retry");
        expect(state.reason).toContain("Unexpected token");
    });

    it("says so when the window was opened with no launcher behind it", async () => {
        const state = new LauncherState({
            describe: async () => refusal("the launcher did not answer", "retry"),
            start: async () => refusal("the launcher did not answer", "retry"),
        });

        await state.look();

        expect(state.state).toBe("retry");
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
