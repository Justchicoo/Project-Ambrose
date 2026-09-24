/*
 * Project Ambrose by Imjustchico
 * Tests what a log line is read into, against lines the servers actually wrote: the time, level, category and message come apart, and each run is sorted into the kind whose colour says what it is, so an address, a setting, a name, a number and quoted text are told apart rather than all being marked as a value. Ordinary words and bare counts are left plain, a hyphenated word is a word, no line marks more than eight runs, and a line that is not a record is not pretended to be one.
 */

import { describe, expect, it } from "vitest";
import { readLine, readValues, type LinePart } from "./logline";

function kindsOf(message: string): Record<string, string[]> {
    const found: Record<string, string[]> = { number: [], text: [], name: [], address: [], setting: [], plain: [] };
    for (const part of readValues(message)) found[part.kind].push(part.text);
    return found;
}

function joined(parts: LinePart[]): string {
    return parts.map((part) => part.text).join("");
}

describe("reading a log line", () => {
    it("comes apart into the time, the level, the category and the message", () => {
        const record = readLine("2026-09-22_20:13:45.901 INFO  [server.admin] The admin API is listening on http://127.0.0.1:12012");
        expect(record).not.toBeNull();
        expect(record!.time).toBe("20:13:45.901");
        expect(record!.level).toBe("INFO");
        expect(record!.category).toBe("server.admin");
        expect(record!.message).toBe("The admin API is listening on http://127.0.0.1:12012");
        expect(joined(record!.parts)).toBe(record!.message);
    });

    it("is not pretended to be a record when it is not one", () => {
        expect(readLine("Started loginserver as process 10992")).toBeNull();
        expect(readLine("loginserver is ready: it printed its ready line")).toBeNull();
        expect(readLine("")).toBeNull();
    });

    it("marks a URL whole rather than splitting it at the slashes", () => {
        expect(kindsOf("The admin API is listening on http://127.0.0.1:12012").address).toContain("http://127.0.0.1:12012");
        expect(kindsOf("fetched https://example.test/a/b?c=1 today").address).toContain("https://example.test/a/b?c=1");
    });

    it("marks an address with its port", () => {
        expect(kindsOf("Listening on 127.0.0.1:12001 with 1 network thread(s)").address).toContain("127.0.0.1:12001");
        expect(kindsOf("bound 10.0.0.5 today").address).toContain("10.0.0.5");
    });

    it("marks a connection string whole rather than in pieces", () => {
        const found = kindsOf("Connected to ambrose@127.0.0.1:3307/ambrose_panel_login running 10.11.14-MariaDB-0ubuntu0.24.04.1");
        expect(found.address).toContain("ambrose@127.0.0.1:3307/ambrose_panel_login");
        expect(found.name).toContain("10.11.14-MariaDB-0ubuntu0.24.04.1");
    });

    it("marks a path, a digest and a quoted string", () => {
        expect(kindsOf("reading C:/Users/someone/panel.crt now").address).toContain("C:/Users/someone/panel.crt");
        expect(kindsOf("wrote /var/log/ambrose.log here").address).toContain("/var/log/ambrose.log");
        expect(kindsOf("Project Ambrose rev de63e83 (land-panel2) 2026-09-22").name).toContain("de63e83");
        expect(kindsOf('the option "Panel.Port" was read').setting).toContain('"Panel.Port"');
        expect(kindsOf('it said "nothing to do" once').text).toContain('"nothing to do"');
    });

    it("marks a number that carries a unit and leaves a bare count alone", () => {
        expect(kindsOf("took 184 ms and 11 MiB").number).toEqual(["184 ms", "11 MiB"]);
        expect(kindsOf("ticked 40 times over 2000 ms").number).toEqual(["2000 ms"]);
        expect(kindsOf("Opened database connection pool login: 1 async, 1 sync").number).toEqual([]);
        expect(kindsOf("with 1 network thread(s)").number).toEqual([]);
    });

    it("leaves ordinary words alone", () => {
        const found = kindsOf("The world has ticked and holds no session");
        expect(found.name).toEqual([]);
        expect(found.number).toEqual([]);
        expect(found.plain.join("")).toBe("The world has ticked and holds no session");
    });

    it("marks a dotted or underscored identifier but not a sentence", () => {
        expect(kindsOf("reloading server.loginserver now").name).toContain("server.loginserver");
        expect(kindsOf("the table ambrose_panel_login is up to date").name).toContain("ambrose_panel_login");
        expect(kindsOf("this is a plain sentence").name).toEqual([]);
    });

    it("tells a configuration option from an identifier by its capitals", () => {
        const found = kindsOf("set Account.VerifierKeys and Account.VerifierActiveKey to encrypt them at rest");
        expect(found.setting).toEqual(["Account.VerifierKeys", "Account.VerifierActiveKey"]);
        expect(kindsOf("reloading server.loginserver now").setting).toEqual([]);
        expect(kindsOf("TypeDumpPath is not set, and Setup.Mode is off").setting).toContain("Setup.Mode");
    });

    it("leaves a hyphenated word a word", () => {
        const found = kindsOf("An AI-built Wizard101 server: loginserver");
        expect(found.name).toEqual([]);
        expect(found.setting).toEqual([]);
        expect(found.plain.join("")).toBe("An AI-built Wizard101 server: loginserver");
    });

    it("marks no more than eight runs on one line", () => {
        const many = "1 ms 2 ms 3 ms 4 ms 5 ms 6 ms 7 ms 8 ms 9 ms 10 ms";
        const found = readValues(many);
        expect(found.filter((part) => part.kind !== "plain")).toHaveLength(8);
        expect(joined(found)).toBe(many);
    });

    it("keeps every character of the message whatever it marks", () => {
        const lines = [
            "The admin API is listening on http://127.0.0.1:12012",
            "Connected to ambrose@127.0.0.1:3307/ambrose_panel_login running 10.11.14-MariaDB-0ubuntu0.24.04.1",
            "No Wizard101 install is in use, so client messages are logged by service and order only",
            "Opened database connection pool characters: 1 async, 1 sync",
        ];
        for (const line of lines) expect(joined(readValues(line))).toBe(line);
    });
});
