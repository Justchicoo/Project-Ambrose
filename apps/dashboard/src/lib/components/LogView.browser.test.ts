/*
 * Project Ambrose by Imjustchico
 * Tests captured output in a real browser with the panel's own stylesheet loaded, because a class name that no rule matches type-checks and formats cleanly and still paints nothing: a record comes out as four columns, the level word and the runs that carry a value each take a colour of their own, a line that is not a record is left whole, and no part of a message is dropped on the way to the screen.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it } from "vitest";
import LogView from "./LogView.svelte";
import "../../app.css";

const lines = [
    { seq: 1, stream: "stdout", text: "2026-09-22_20:13:45.901 INFO  [server.admin] The admin API is listening on http://127.0.0.1:12012" },
    { seq: 2, stream: "stdout", text: "2026-09-22_20:13:45.902 WARN  [server.loading] Loading took 184 ms" },
    { seq: 3, stream: "stdout", text: "2026-09-22_20:13:46.010 ERROR [server.database] Connection to ambrose@127.0.0.1:3307 failed" },
    {
        seq: 5,
        stream: "stdout",
        text: '2026-09-22_20:13:46.100 INFO  [accounts] set Account.VerifierKeys on ambrose_panel_login saying "ready"',
    },
    { seq: 4, stream: "supervisor", text: "Started loginserver as process 10992" },
];

let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

function colourOf(text: string): string {
    for (const span of host.querySelectorAll("span")) {
        if (span.textContent === text) return getComputedStyle(span).color;
    }
    throw new Error(`no span holds ${JSON.stringify(text)}`);
}

beforeEach(() => {
    host = document.createElement("div");
    document.body.append(host);
    document.documentElement.dataset.theme = "dark";
    page = mount(LogView, { target: host, props: { lines } });
    flushSync();
});

afterEach(() => {
    if (page) unmount(page);
    page = null;
    host.remove();
});

describe("captured output on screen", () => {
    it("drops the date and keeps the time, the level and the category apart", () => {
        const text = host.textContent ?? "";
        expect(text).not.toContain("2026-09-22");
        expect(colourOf("20:13:45.901")).toBeTruthy();
        expect(colourOf("INFO")).toBeTruthy();
        expect(colourOf("[server.admin]")).toBeTruthy();
    });

    it("paints the level words apart from each other and from the message", () => {
        const info = colourOf("INFO");
        const warn = colourOf("WARN");
        const error = colourOf("ERROR");
        expect(new Set([info, warn, error]).size).toBe(3);
        expect(info).not.toBe(colourOf("The admin API is listening on "));
    });

    it("paints a value apart from the words around it", () => {
        const words = colourOf("The admin API is listening on ");
        expect(colourOf("http://127.0.0.1:12012")).not.toBe(words);
        expect(colourOf("184 ms")).not.toBe(words);
        expect(colourOf("ambrose@127.0.0.1:3307")).not.toBe(words);
    });

    it("gives every kind of value a colour of its own, which is the point of the ramp", () => {
        const kinds = {
            address: colourOf("http://127.0.0.1:12012"),
            number: colourOf("184 ms"),
            setting: colourOf("Account.VerifierKeys"),
            name: colourOf("ambrose_panel_login"),
            text: colourOf('"ready"'),
        };
        expect(new Set(Object.values(kinds)).size).toBe(5);
        expect(colourOf("ambrose@127.0.0.1:3307")).toBe(kinds.address);
    });

    it("leaves a line that is not a record whole and in its own colour", () => {
        const supervisor = [...host.querySelectorAll("div")].find(
            (box) => box.textContent?.trim() === "Started loginserver as process 10992",
        );
        expect(supervisor).toBeDefined();
        expect(getComputedStyle(supervisor!).color).not.toBe(colourOf("The admin API is listening on "));
    });

    it("drops no part of a message", () => {
        const text = host.textContent ?? "";
        expect(text).toContain("The admin API is listening on http://127.0.0.1:12012");
        expect(text).toContain("Connection to ambrose@127.0.0.1:3307 failed");
    });
});
