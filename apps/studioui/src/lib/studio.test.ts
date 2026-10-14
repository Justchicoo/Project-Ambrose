/*
 * Project Ambrose by Imjustchico
 * Tests the Studio's channel as it sits on the host every surface shares: each question reaches its own path with what it was asked to send, one quest or cinematic goes by name, and a host that refuses is read as nothing found, an empty page or an empty summary, so the page never guards against a shape the channel could have prevented.
 */

import { describe, expect, it } from "vitest";
import type { Host, HostRequest } from "@ambrose/ui";
import { hostChannel } from "./studio";

function fakeHost(ok: boolean, body: unknown): { host: Host; asked: HostRequest[] } {
    const asked: HostRequest[] = [];
    return {
        asked,
        host: {
            kind: "http",
            call: async <T>(request: HostRequest) => {
                asked.push(request);
                return { ok, status: ok ? 200 : 404, body: body as T };
            },
        },
    };
}

describe("the Studio channel", () => {
    it("asks each path with what it was given", async () => {
        const { host, asked } = fakeHost(true, {
            total: 1,
            offset: 0,
            items: [{ name: "WC-COMMONS-MAIN-001", title: "Unicorn Way", level: 1, mainline: true, goals: 2, giver: 1 }],
        });
        const channel = hostChannel(host);
        const page = await channel.quests({ search: "unicorn", offset: 0, limit: 50 });
        await channel.quest("WC-COMMONS-MAIN-001");
        await channel.cinematics({ search: "Satyr" });
        await channel.cinematic("Cinematics/SatyrCinematic.xml");
        expect(page.items[0]?.title).toBe("Unicorn Way");
        expect(asked.map((request) => request.path)).toEqual([
            "/studio/quests",
            "/studio/quest",
            "/studio/cinematics",
            "/studio/cinematic",
        ]);
        expect(asked[0]?.body).toEqual({ search: "unicorn", offset: 0, limit: 50 });
        expect(asked[1]?.body).toEqual({ name: "WC-COMMONS-MAIN-001" });
        expect(asked[3]?.body).toEqual({ name: "Cinematics/SatyrCinematic.xml" });
    });

    it("reads a refusal as nothing found", async () => {
        const channel = hostChannel(fakeHost(false, { error: "no quest" }).host);
        expect(await channel.quest("missing")).toBeNull();
        expect(await channel.cinematic("missing")).toBeNull();
        expect(await channel.quests({})).toEqual({ total: 0, offset: 0, items: [] });
        expect((await channel.summary()).quests).toBe(0);
    });
});
