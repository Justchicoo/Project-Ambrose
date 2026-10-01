/*
 * Project Ambrose by Imjustchico
 * What the window is showing and why. The launcher decides the launch state and the page only holds it: the first run's steps come back with it, checking, setting up, locate, retry, play, launching or playing, together with the login server's status from the launcher's own attempt to reach it, and a plan or a refusal carries it too, so the one primary button the window draws always says what the launcher said. While the state is one that moves on by itself, checking, setting up or launching, the launcher is asked again every half second so each step shows as it happens, and asking stops as soon as the state settles; a plan is asked for only when the steps are done and none is held yet, since each asking reads the install again. Retry starts the steps over, carrying any setting the operator changed, so an installation named on the settings screen is looked for from the first step, and Play is sent only from the play state, once, however often it is pressed. Settings is a screen the operator opens and leaves, so it is the one thing the answer does not choose. The failure carries what a person needs to report it: diagnostics written from the launcher's own answers, the state, the cause, every step's word and the plan's facts with the password already hidden by the launcher, and the log folder is opened by asking the launcher, which answers with its own folder or why there is none. An answer that cannot be read at all is shown as a failure naming what went wrong, never left as a state that waits forever. The account is remembered by name only, never the password, because a launcher that remembers a password has become somewhere to steal one from, and a name on its own is never sent as a login, since a name with no key is a refusal and would turn a remembered name into a launcher that has stopped working. Where it is remembered is handed in rather than reached for, so the window uses the browser's own store and a test uses its own, and neither needs the other to exist. Nothing here decides what would be run: every value the operator changes is sent back and the launcher answers with a new plan, so the window and the terminal can never drift apart.
 */

import {
    isPlan,
    profileOf,
    stateOf,
    type LaunchState,
    type LauncherAnswer,
    type LauncherChannel,
    type LauncherPlan,
    type LauncherProfile,
    type LauncherRequest,
    type ServerReach,
    type SetupAnswer,
    type SetupStep,
} from "./channel";

export type Screen = "home" | "settings";

export const RememberedAccount = "ambrose.launcher.account";

export type Remembering = {
    get(): string;
    set(name: string): void;
};

export function browserMemory(): Remembering {
    return {
        get() {
            try {
                return window.localStorage.getItem(RememberedAccount) ?? "";
            } catch {
                return "";
            }
        },
        set(name: string) {
            try {
                if (name === "") window.localStorage.removeItem(RememberedAccount);
                else window.localStorage.setItem(RememberedAccount, name);
            } catch {
                return;
            }
        },
    };
}

export const FollowEvery = 500;

export const Moving: readonly LaunchState[] = ["checking", "setting-up", "launching"];
export const Failing: readonly LaunchState[] = ["locate", "retry"];
export const Started: readonly LaunchState[] = ["play", "launching", "playing"];

export type Pause = (milliseconds: number) => Promise<void>;

const wait: Pause = (milliseconds) => new Promise((resolve) => setTimeout(resolve, milliseconds));

export class LauncherState {
    screen = $state<Screen>("home");
    state = $state<LaunchState>("checking");
    server = $state<ServerReach>({ status: "unknown", address: "" });
    plan = $state<LauncherPlan | null>(null);
    reason = $state("");
    steps = $state<SetupStep[]>([]);
    account = $state("");
    password = $state("");
    starting = $state(false);
    logs = $state("");
    settings = $state<LauncherRequest>({});

    #channel: LauncherChannel;
    #memory: Remembering;
    #pause: Pause;

    constructor(channel: LauncherChannel, memory: Remembering = browserMemory(), pause: Pause = wait) {
        this.#channel = channel;
        this.#memory = memory;
        this.#pause = pause;
        this.account = memory.get();
    }

    get ready(): boolean {
        return this.plan !== null;
    }

    get failed(): boolean {
        return Failing.includes(this.state);
    }

    get moving(): boolean {
        return Moving.includes(this.state);
    }

    get done(): number {
        return this.steps.filter((step) => step.state === "done").length;
    }

    get current(): SetupStep | null {
        return (
            this.steps.find((step) => step.state === "wrong") ??
            this.steps.find((step) => step.state === "doing") ??
            this.steps.find((step) => step.state === "waiting") ??
            null
        );
    }

    get profile(): LauncherProfile {
        return profileOf(this.plan, this.account);
    }

    get guarantee(): string {
        return "Ambrose reads your own installation and writes nothing into it. It never starts KingsIsle's launcher or patcher.";
    }

    get diagnostics(): string {
        const where = this.server.address === "" ? "" : ` at ${this.server.address}`;
        const lines = [`State: ${this.state}`, `Server: ${this.server.status}${where}`];
        if (this.reason !== "") lines.push(`Cause: ${this.reason}`);
        this.steps.forEach((step, index) => lines.push(`Step ${index + 1} ${step.id} (${step.state}): ${step.label}. ${step.word}`));
        if (this.plan) {
            lines.push(
                `Installation: ${this.plan.install}`,
                `Client: ${this.plan.revision}`,
                `Run folder: ${this.plan.run_folder}`,
                `Client log: ${this.plan.log_file}`,
                `Window: ${this.plan.window}`,
                `Language: ${this.plan.locale}`,
                `Command: ${this.plan.command}`,
            );
        }
        return lines.join("\n");
    }

    async look(): Promise<void> {
        if (this.#channel.steps) {
            const setup = await this.#channel.steps();
            if (setup.steps.length > 0) {
                this.show(setup);
                if (!Started.includes(this.state)) return;
                if (this.plan !== null) return;
            }
        }
        this.take(await this.#channel.describe(this.settings));
    }

    async run(): Promise<void> {
        try {
            await this.look();
            while (this.moving) {
                await this.#pause(FollowEvery);
                await this.look();
            }
        } catch (error) {
            this.fail(error);
        }
    }

    fail(error: unknown): void {
        this.state = "retry";
        this.plan = null;
        this.reason = `The launcher's answer could not be read: ${error instanceof Error ? error.message : String(error)}`;
    }

    async again(): Promise<void> {
        this.state = "checking";
        this.reason = "";
        this.logs = "";
        this.plan = null;
        if (this.#channel.again) {
            try {
                const setup = await this.#channel.again(this.settings);
                if (setup.steps.length > 0) {
                    this.show(setup);
                    if (this.failed) return;
                }
            } catch (error) {
                this.fail(error);
                return;
            }
        }
        await this.run();
    }

    show(setup: SetupAnswer): void {
        this.steps = setup.steps;
        this.server = setup.server;
        this.state = setup.state;
        const wrong = setup.steps.find((step) => step.state === "wrong");
        this.reason = wrong ? wrong.word : "";
        if (wrong) this.plan = null;
    }

    async apply(settings: LauncherRequest): Promise<void> {
        this.settings = { ...this.settings, ...settings };
        this.screen = "home";
        if (this.#channel.again) {
            await this.again();
            return;
        }
        this.take(await this.#channel.describe(this.settings));
    }

    async play(): Promise<void> {
        if (this.starting || this.state !== "play") return;
        this.starting = true;
        try {
            this.#memory.set(this.account);
            const request: LauncherRequest = { ...this.settings };
            if (this.account !== "" && this.password !== "") {
                request.user = { user_id: this.account, key: this.password, name: this.account };
            }
            this.take(await this.#channel.start(request));
        } catch (error) {
            this.fail(error);
        } finally {
            this.starting = false;
            this.password = "";
        }
        if (this.moving) await this.run();
    }

    async openLogs(): Promise<void> {
        if (!this.#channel.openLogs) {
            this.logs = "This window cannot open folders.";
            return;
        }
        const answer = await this.#channel.openLogs();
        this.logs = answer.opened
            ? `Opened ${answer.folder ?? "the log folder"}`
            : (answer.reason ?? "The log folder could not be opened.");
    }

    open(screen: Screen): void {
        this.screen = screen;
    }

    close(): void {
        this.screen = "home";
    }

    take(answer: LauncherAnswer): void {
        this.state = stateOf(answer);
        if (isPlan(answer)) {
            this.plan = answer;
            this.reason = "";
            return;
        }
        this.plan = null;
        this.reason = answer.reason;
    }
}
