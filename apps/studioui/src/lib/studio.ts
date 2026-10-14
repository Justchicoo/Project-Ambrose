/*
 * Project Ambrose by Imjustchico
 * The only way the Studio window speaks to the Studio, and the shapes of what it hears back: the summary of what was loaded, pages of quests and cinematics found by a search, one quest with its goals, dialogs, requirements and results, and one cinematic decoded from the install with the quest results that play it. The channel is an interface so the page's logic can be driven by a test that answers as the Studio would, and the real one is built over the host @ambrose/ui gives every surface.
 */

import type { Host } from "@ambrose/ui";

export type Summary = {
    quests: number;
    goals: number;
    results: number;
    cinematics: number;
    linked_cinematics: number;
    install: boolean;
    types: boolean;
};

export type Page<T> = { total: number; offset: number; items: T[] };

export type QuestItem = { name: string; title: string; level: number; mainline: boolean; goals: number; giver: number };

export type CinematicItem = { name: string; used_by: number };

export type Goal = {
    name: string;
    type: string;
    title: string;
    title_key: string;
    destination_zone: string;
    persona: string;
    tally: number;
    school: string;
    requirements: unknown;
};

export type DialogLine = {
    owner: string;
    goal: string;
    tag: string;
    position: number;
    actor: number;
    text: string;
    key: string;
    event: string;
    animation: string;
};

export type Result = { goal: string; position: number; class: string; on_start: boolean; data: unknown; cinematics: string[] };

export type Quest = {
    name: string;
    title: string;
    title_key: string;
    info: string;
    level: number;
    repeat: number;
    mainline: boolean;
    hidden: boolean;
    pet_only: boolean;
    activity_type: number;
    giver: number;
    requirement_list: string;
    requirements: unknown;
    prequests: string[];
    followups: string[];
    goals: Goal[];
    dialogs: DialogLine[];
    results: Result[];
};

export type CinematicUse = { quest: string; goal: string; class: string; on_start: boolean };

export type Cinematic = { name: string; bytes: number; used_by: CinematicUse[]; data: unknown; unread: string };

export type Search = { search?: string; offset?: number; limit?: number };

export interface StudioChannel {
    summary(): Promise<Summary>;
    quests(search: Search): Promise<Page<QuestItem>>;
    quest(name: string): Promise<Quest | null>;
    cinematics(search: Search): Promise<Page<CinematicItem>>;
    cinematic(name: string): Promise<Cinematic | null>;
}

export function hostChannel(host: Host): StudioChannel {
    const ask = async <T>(path: string, body: unknown): Promise<T | null> => {
        const reply = await host.call<T>({ path, method: "POST", body });
        return reply.ok ? reply.body : null;
    };
    const empty = <T>(): Page<T> => ({ total: 0, offset: 0, items: [] });
    return {
        summary: async () =>
            (await ask<Summary>("/studio/summary", {})) ?? {
                quests: 0,
                goals: 0,
                results: 0,
                cinematics: 0,
                linked_cinematics: 0,
                install: false,
                types: false,
            },
        quests: async (search) => (await ask<Page<QuestItem>>("/studio/quests", search)) ?? empty<QuestItem>(),
        quest: (name) => ask<Quest>("/studio/quest", { name }),
        cinematics: async (search) => (await ask<Page<CinematicItem>>("/studio/cinematics", search)) ?? empty<CinematicItem>(),
        cinematic: (name) => ask<Cinematic>("/studio/cinematic", { name }),
    };
}
