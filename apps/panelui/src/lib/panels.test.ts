/*
 * Project Ambrose by Imjustchico
 * Tests the panel program's screens' logic against a channel a test answers as the program would: the list comes from the program, entries are probed only while the list is the screen shown and never when it holds none, an https address is pinned only after its fingerprint is confirmed, plain HTTP beyond this computer is sent with the opt-in named, a pairing needs no comparing, a refusal is shown as the program worded it, and the about screen shows what the program reports.
 */

import { describe, expect, it } from "vitest";
import type { Answer, Listing, PanelChannel, ProbeState, Trust } from "./channel";
import { PanelsState, ProbeEvery } from "./panels.svelte";

const empty: Listing = { this_computer: { found: true, revision: "9f8e7d6", word: "" }, panels: [] };
const one: Listing = {
    this_computer: { found: false, revision: "", word: "no supervisor answers" },
    panels: [
        {
            id: 3,
            name: "Realm one",
            origin: "https://panel.example.org:12080",
            trust: "pinned",
            pin: "AB:CD",
            warning: "",
            last_user: "",
            last_opened: 0,
        },
    ],
};

function ok<T>(body: T): Answer<T> {
    return { ok: true, body };
}

function rig(listing: Listing = empty) {
    const made = {
        listing,
        probes: 0,
        added: [] as { name: string; address: string; trust: Trust; pin?: string }[],
        paired: [] as string[],
        refuse: "",
        channel: undefined as unknown as PanelChannel,
    };
    made.channel = {
        list: async () => ok(made.listing),
        probe: async () => {
            made.probes++;
            const states: ProbeState[] = made.listing.panels.map((panel) => ({ id: panel.id, state: "answering", word: "answers" }));
            return ok({ states });
        },
        inspect: async (address) => ok({ origin: address, served: "AB:CD:EF:01 23:45:67:89", fingerprint: "AB:CD:EF:01:23:45:67:89" }),
        add: async (entry) => {
            if (made.refuse) return { ok: false, problem: { error: "address_refused", message: made.refuse } };
            made.added.push(entry);
            return ok({ id: 9, warning: "" });
        },
        pair: async (line) => {
            made.paired.push(line);
            return ok({ id: 10 });
        },
        rename: async () => ok({}),
        forget: async () => ok({}),
        open: async () => ok({}),
        about: async () => ok({ version: "0.1.0", revision: "abc1234", webview: "130.0", supervisor_revision: "9f8e7d6" }),
    };
    return made;
}

describe("the panel program's screens", () => {
    it("never probes when the list holds no panel, and probes only while the list is shown", async () => {
        const made = rig();
        const pauses: number[] = [];
        const state = new PanelsState(made.channel, async (milliseconds) => {
            pauses.push(milliseconds);
        });
        await state.watch(3);
        expect(made.probes).toBe(0);
        expect(pauses).toEqual([ProbeEvery, ProbeEvery, ProbeEvery]);

        made.listing = one;
        await state.refresh();
        await state.probeOnce();
        expect(made.probes).toBe(1);
        expect(state.states[3].state).toBe("answering");
        state.go("about");
        await state.probeOnce();
        expect(made.probes).toBe(1);
    });

    it("pins an https panel only after its fingerprint is confirmed", async () => {
        const made = rig();
        const state = new PanelsState(made.channel);
        state.go("add");
        await state.inspect("https://panel.example.org:12080");
        expect(state.inspection?.served).toBe("AB:CD:EF:01 23:45:67:89");

        expect(await state.add("Realm one", "https://panel.example.org:12080", false, false)).toBe(false);
        expect(made.added).toHaveLength(0);
        expect(state.problem).toContain("confirm");

        expect(await state.add("Realm one", "https://panel.example.org:12080", false, true)).toBe(true);
        expect(made.added[0]).toEqual({
            name: "Realm one",
            address: "https://panel.example.org:12080",
            trust: "pinned",
            pin: "AB:CD:EF:01:23:45:67:89",
        });
        expect(state.screen).toBe("list");
    });

    it("sends plain HTTP beyond this computer only with the opt-in, and shows a refusal as the program worded it", async () => {
        const made = rig();
        const state = new PanelsState(made.channel);
        made.refuse = "http://panel.example.org:12080 is plain HTTP beyond this computer";
        expect(await state.add("Plain", "http://panel.example.org:12080", false, false)).toBe(false);
        expect(state.problem).toBe(made.refuse);
        made.refuse = "";
        expect(await state.add("Plain", "http://panel.example.org:12080", true, false)).toBe(true);
        expect(made.added[0].trust).toBe("plain-http");
    });

    it("pairs from a line with no comparing, and the about screen shows what the program reports", async () => {
        const made = rig();
        const state = new PanelsState(made.channel);
        expect(await state.pair("https://panel.example.org:12080/#link?token=t&sha256=AB", "Realm one")).toBe(true);
        expect(made.paired).toHaveLength(1);
        await state.showAbout();
        expect(state.screen).toBe("about");
        expect(state.about?.supervisor_revision).toBe("9f8e7d6");
    });
});
