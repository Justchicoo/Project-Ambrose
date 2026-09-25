/*
 * Project Ambrose by Imjustchico
 * What the window is showing and why. It asks the launcher what it found, and which screen follows is decided by the answer rather than by the page: a plan means ready to play, a refusal means the failure screen with the reason the launcher gave, and a first run that is still working through its steps means the first-run screen until it is done. Settings are a screen the operator opens and leaves, so it is the one state the answer does not choose. The account is remembered by name only, never the password, because a launcher that remembers a password has become somewhere to steal one from, and a name on its own is never sent as a login, since a name with no key is a refusal and would turn a remembered name into a launcher that has stopped working. Where it is remembered is handed in rather than reached for, so the window uses the browser's own store and a test uses its own, and neither needs the other to exist. Nothing here decides what would be run: every value the operator changes is sent back and the launcher answers with a new plan, so the window and the terminal can never drift apart.
 */

import { isPlan, type LauncherAnswer, type LauncherChannel, type LauncherPlan, type LauncherRequest, type SetupStep } from "./channel";

export type Screen = "reading" | "ready" | "first-run" | "settings" | "failed";

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

export class LauncherState {
    screen = $state<Screen>("reading");
    plan = $state<LauncherPlan | null>(null);
    reason = $state("");
    steps = $state<SetupStep[]>([]);
    account = $state("");
    password = $state("");
    starting = $state(false);
    settings = $state<LauncherRequest>({});

    #channel: LauncherChannel;
    #memory: Remembering;

    constructor(channel: LauncherChannel, memory: Remembering = browserMemory()) {
        this.#channel = channel;
        this.#memory = memory;
        this.account = memory.get();
    }

    get ready(): boolean {
        return this.plan !== null;
    }

    get guarantee(): string {
        return "Ambrose reads your own installation and writes nothing into it. It never starts KingsIsle's launcher or patcher.";
    }

    async look(): Promise<void> {
        if (this.#channel.steps) {
            const steps = await this.#channel.steps();
            this.steps = steps;
            if (steps.some((step) => step.state === "doing" || step.state === "waiting")) {
                this.screen = "first-run";
                return;
            }
            const wrong = steps.find((step) => step.state === "wrong");
            if (wrong) {
                this.reason = wrong.word;
                this.screen = "failed";
                return;
            }
        }
        this.take(await this.#channel.describe(this.settings));
    }

    async apply(settings: LauncherRequest): Promise<void> {
        this.settings = { ...this.settings, ...settings };
        this.take(await this.#channel.describe(this.settings));
    }

    async play(): Promise<void> {
        if (this.starting) return;
        this.starting = true;
        try {
            this.#memory.set(this.account);
            const request: LauncherRequest = { ...this.settings };
            if (this.account !== "" && this.password !== "") {
                request.user = { user_id: this.account, key: this.password, name: this.account };
            }
            this.take(await this.#channel.start(request));
        } finally {
            this.starting = false;
            this.password = "";
        }
    }

    open(screen: Screen): void {
        this.screen = screen;
    }

    close(): void {
        this.screen = this.plan === null ? "failed" : "ready";
    }

    take(answer: LauncherAnswer): void {
        if (isPlan(answer)) {
            this.plan = answer;
            this.reason = "";
            this.screen = "ready";
            return;
        }
        this.plan = null;
        this.reason = answer.reason;
        this.screen = "failed";
    }
}
