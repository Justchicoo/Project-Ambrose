/*
 * Project Ambrose by Imjustchico
 * The only way the panel program's screens speak to the program, and the shapes of what they hear back. Every call goes through the host bridge @ambrose/ui gives every surface, so the screens add no second way of reaching a host, and every answer is either what was asked for or a problem naming why. Nothing here carries a password: the program never asks for one, and a panel's own sign-in page is where one is typed.
 */

import type { Host } from "@ambrose/ui";

export type Trust = "loopback" | "public" | "pinned" | "plain-http";

export type PanelRow = {
    id: number;
    name: string;
    origin: string;
    trust: Trust;
    pin: string;
    warning: string;
    last_user: string;
    last_opened: number;
};

export type ThisComputer = { found: boolean; revision: string; word: string };

export type Listing = { this_computer: ThisComputer; panels: PanelRow[] };

export type ProbeState = { id: number; state: "answering" | "unreachable" | "certificate changed" | "not an Ambrose panel"; word: string };

export type Inspection = { origin: string; served: string; fingerprint?: string };

export type About = { version: string; revision: string; webview: string; supervisor_revision: string };

export type Problem = { error: string; message: string };

export type Answer<T> = { ok: true; body: T } | { ok: false; problem: Problem };

export const ThisComputerId = 0;

export const NoProgram: Problem = {
    error: "no_program",
    message: "these screens were opened without the panel program behind them, so there is nothing to ask",
};

export type PanelChannel = {
    list(): Promise<Answer<Listing>>;
    probe(): Promise<Answer<{ states: ProbeState[] }>>;
    inspect(address: string): Promise<Answer<Inspection>>;
    add(entry: { name: string; address: string; trust: Trust; pin?: string }): Promise<Answer<{ id: number; warning: string }>>;
    pair(line: string, name: string): Promise<Answer<{ id: number }>>;
    rename(id: number, name: string): Promise<Answer<object>>;
    forget(id: number): Promise<Answer<object>>;
    open(id: number): Promise<Answer<object>>;
    about(): Promise<Answer<About>>;
};

export function hostChannel(host: Host): PanelChannel {
    async function ask<T>(path: string, method: "GET" | "POST", body?: unknown): Promise<Answer<T>> {
        const reply = await host.call<T | Problem>({ path, method, body });
        if (reply.body === undefined || reply.body === null) return { ok: false, problem: NoProgram };
        if (!reply.ok) return { ok: false, problem: reply.body as Problem };
        return { ok: true, body: reply.body as T };
    }

    return {
        list: () => ask<Listing>("/panels", "GET"),
        probe: () => ask<{ states: ProbeState[] }>("/panels/probe", "POST", {}),
        inspect: (address) => ask<Inspection>("/panels/inspect", "POST", { address }),
        add: (entry) => ask<{ id: number; warning: string }>("/panels", "POST", entry),
        pair: (line, name) => ask<{ id: number }>("/panels/pair", "POST", { line, name }),
        rename: (id, name) => ask<object>("/panels/rename", "POST", { id, name }),
        forget: (id) => ask<object>("/panels/forget", "POST", { id }),
        open: (id) => ask<object>("/panels/open", "POST", { id }),
        about: () => ask<About>("/about", "GET"),
    };
}
