/*
 * Project Ambrose by Imjustchico
 * What the panel program's screens are showing and why. The list is asked for when the program opens and every entry is probed every ten seconds only while the list is the screen shown, since a probe is a connection to every panel in it. Adding a panel over https first shows the certificate the address serves, grouped for comparing with the one the supervisor printed, and pins it only once the operator confirms they match; a pairing line needs no comparing, because it carries the pin. Plain HTTP beyond this computer is added only when the operator takes the opt-in, and the warning the program returns is shown with the entry. Every refusal is shown as the program worded it.
 */

import type { About, Inspection, Listing, PanelChannel, ProbeState, Trust } from "./channel";

export type Screen = "list" | "add" | "pair" | "about";

export const ProbeEvery = 10000;

export type Pause = (milliseconds: number) => Promise<void>;

const wait: Pause = (milliseconds) => new Promise((resolve) => setTimeout(resolve, milliseconds));

export class PanelsState {
    screen = $state<Screen>("list");
    listing = $state<Listing | null>(null);
    states = $state<Record<number, ProbeState>>({});
    problem = $state("");
    inspection = $state<Inspection | null>(null);
    about = $state<About | null>(null);
    busy = $state(false);

    #channel: PanelChannel;
    #pause: Pause;

    constructor(channel: PanelChannel, pause: Pause = wait) {
        this.#channel = channel;
        this.#pause = pause;
    }

    async refresh(): Promise<void> {
        const answer = await this.#channel.list();
        if (!answer.ok) {
            this.problem = answer.problem.message;
            return;
        }
        this.listing = answer.body;
        this.problem = "";
    }

    async probeOnce(): Promise<void> {
        if (this.screen !== "list" || !this.listing || this.listing.panels.length === 0) return;
        const answer = await this.#channel.probe();
        if (!answer.ok) return;
        const next: Record<number, ProbeState> = {};
        for (const state of answer.body.states) next[state.id] = state;
        this.states = next;
    }

    async watch(rounds = Number.POSITIVE_INFINITY): Promise<void> {
        await this.refresh();
        for (let round = 0; round < rounds; round++) {
            await this.probeOnce();
            await this.#pause(ProbeEvery);
        }
    }

    go(screen: Screen): void {
        this.screen = screen;
        this.problem = "";
        this.inspection = null;
    }

    async inspect(address: string): Promise<void> {
        this.inspection = null;
        const answer = await this.#channel.inspect(address);
        if (!answer.ok) {
            this.problem = answer.problem.message;
            return;
        }
        this.problem = "";
        this.inspection = answer.body;
    }

    async add(name: string, address: string, plainOptIn: boolean, confirmed: boolean): Promise<boolean> {
        let trust: Trust = plainOptIn ? "plain-http" : "public";
        let pin: string | undefined;
        if (this.inspection?.served) {
            if (!confirmed) {
                this.problem =
                    "Compare the fingerprint with the one the supervisor printed and confirm they match before this panel is added";
                return false;
            }
            trust = "pinned";
            pin = this.inspection.fingerprint ?? this.inspection.served;
        }
        const answer = await this.#channel.add({ name, address, trust, pin });
        if (!answer.ok) {
            this.problem = answer.problem.message;
            return false;
        }
        await this.refresh();
        this.go("list");
        return true;
    }

    async pair(line: string, name: string): Promise<boolean> {
        const answer = await this.#channel.pair(line, name);
        if (!answer.ok) {
            this.problem = answer.problem.message;
            return false;
        }
        await this.refresh();
        this.go("list");
        return true;
    }

    async open(id: number): Promise<void> {
        this.busy = true;
        try {
            const answer = await this.#channel.open(id);
            this.problem = answer.ok ? "" : answer.problem.message;
            if (answer.ok) await this.refresh();
        } finally {
            this.busy = false;
        }
    }

    async rename(id: number, name: string): Promise<void> {
        const answer = await this.#channel.rename(id, name);
        this.problem = answer.ok ? "" : answer.problem.message;
        if (answer.ok) await this.refresh();
    }

    async forget(id: number): Promise<void> {
        const answer = await this.#channel.forget(id);
        this.problem = answer.ok ? "" : answer.problem.message;
        if (answer.ok) await this.refresh();
    }

    async showAbout(): Promise<void> {
        this.go("about");
        const answer = await this.#channel.about();
        if (answer.ok) this.about = answer.body;
        else this.problem = answer.problem.message;
    }
}
