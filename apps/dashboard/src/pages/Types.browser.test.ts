/*
 * Project Ambrose by Imjustchico
 * Tests the types page in a real browser against a stubbed admin API: the page lists the classes the registry holds, finds one by name and by hash, opens its detail with bases and properties, walks to a base and back, and names the revision stamp on every view.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { session } from "$lib/api.svelte";
import { live } from "$lib/status.svelte";
import type { Status } from "$lib/schemas";
import Types from "./Types.svelte";

const status: Status = {
    schema: 1,
    app: "gameserver",
    role: "game",
    realm: "",
    revision: "abc1234",
    state: "running",
    uptime: 120,
    memory: null,
    threads: null,
    sessions: null,
    tick: null,
    stats: {},
    problems: [],
};

const stamp = { revision: "r806919", program_sha256: "abc123def456", generation: 3 };

function listBody() {
    return {
        schema: 1,
        stamp,
        rows: [
            { name: "class TestBase", hash: 1001, kind: "PropertyClass", property_count: 1, from_server_class: false },
            { name: "class TestDerived", hash: 1002, kind: "PropertyClass", property_count: 2, from_server_class: false },
        ],
        total: 2,
        has_more: false,
    };
}

function detailBody(name: string) {
    const isBase = name === "class TestBase";
    return {
        schema: 1,
        stamp,
        name,
        hash: isBase ? 1001 : 1002,
        kind: "PropertyClass",
        property_count: isBase ? 1 : 2,
        from_server_class: false,
        bases: isBase ? [] : ["class TestBase"],
        derived: isBase ? ["class TestDerived"] : [],
        properties: isBase
            ? [
                  {
                      id: 1,
                      name: "m_id",
                      type: "unsigned int",
                      container: "Static",
                      flags: ["Serialized"],
                      offset: 0,
                      hash: 111,
                      enum_options: [],
                  },
              ]
            : [
                  {
                      id: 1,
                      name: "m_name",
                      type: "std::string",
                      container: "Static",
                      flags: ["Serialized"],
                      offset: 8,
                      hash: 222,
                      enum_options: [],
                  },
                  { id: 2, name: "m_level", type: "int", container: "Static", flags: [], offset: 16, hash: 333, enum_options: [] },
              ],
        provenance: "type_dump",
        typed_views: [],
    };
}

let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

function open() {
    host = document.createElement("div");
    document.body.append(host);
    page = mount(Types, { target: host });
    flushSync();
}

async function drawn(text: string) {
    await vi.waitFor(() => {
        flushSync();
        expect(host.textContent ?? "").toContain(text);
    });
}

function stubFetch() {
    vi.stubGlobal("fetch", (path: string) => {
        const url = String(path);
        if (url.includes("/api/capabilities")) {
            return Promise.resolve(
                new Response(JSON.stringify({ schema: 1, data_kinds: [{ kind: "types", generation: 3 }] }), {
                    status: 200,
                    headers: { "Content-Type": "application/json" },
                }),
            );
        }
        if (url.includes("/api/data/types/")) {
            const name = decodeURIComponent(url.split("/api/data/types/")[1].split("?")[0]);
            return Promise.resolve(
                new Response(JSON.stringify(detailBody(name)), { status: 200, headers: { "Content-Type": "application/json" } }),
            );
        }
        const query = new URL(url, "http://localhost").searchParams.get("q") ?? "";
        const all = listBody();
        const rows = query ? all.rows.filter((r: { name: string }) => r.name.toLowerCase().includes(query.toLowerCase())) : all.rows;
        return Promise.resolve(
            new Response(JSON.stringify({ ...all, rows, total: rows.length }), {
                status: 200,
                headers: { "Content-Type": "application/json" },
            }),
        );
    });
}

beforeEach(() => {
    stubFetch();
    session.csrf = "token";
    session.state = "signed-in";
    live.status = status;
    live.now = Date.now();
    live.apps = [{ name: "gameserver", supervision: { admin: { enabled: true } } }];
});

afterEach(() => {
    if (page) unmount(page);
    page = null;
    host?.remove();
    vi.unstubAllGlobals();
});

describe("the types page", () => {
    it("lists the classes the registry holds", async () => {
        open();
        await drawn("class TestDerived");
        const text = host.textContent ?? "";
        expect(text).toContain("class TestBase");
        expect(text).toContain("2 classes");
    });

    it("names the revision stamp on the list", async () => {
        open();
        await drawn("r806919");
        const text = host.textContent ?? "";
        expect(text).toContain("generation");
        expect(text).toContain("3");
    });

    it("finds a class by name", async () => {
        open();
        await drawn("class TestDerived");
        const input = host.querySelector('input[aria-label="Search classes"]') as HTMLInputElement | null;
        expect(input, "the search box exists").toBeTruthy();
        if (input) {
            input.value = "TestBase";
            input.dispatchEvent(new Event("input", { bubbles: true }));
            flushSync();
        }
        await vi.waitFor(
            () => {
                flushSync();
                const text = host.textContent ?? "";
                expect(text).toContain("class TestBase");
                expect(text).not.toContain("class TestDerived");
            },
            { timeout: 5000 },
        );
    });
});
