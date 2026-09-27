/*
 * Project Ambrose by Imjustchico
 * Tests what the config page decides before it asks the server: each type refuses what the server would, with the bounds in the message, a value is sent in the form the server stores, a small or large number written as the server writes it and only the spaces the server trims removed, a reason is clipped by bytes and never splits a character, search and categories keep only live settings, a preset never carries a secret, reading a preset names what is wrong with a file, and a preset's diff marks every entry that changes, stays or would be refused, with the reason, and takes the server's own refusals over its guesses.
 */

import { describe, expect, it } from "vitest";
import type { Setting } from "./settings";
import { applyRefusals, categories, checkValue, clipBytes, diffPreset, makePreset, normalise, readPreset, textBytes } from "./settings";

function live(key: string, type: string, value: string, extra: Partial<Setting> = {}): Setting {
    return {
        key,
        value,
        layer: "config",
        file: "gameserver.conf",
        line: 1,
        default: value,
        default_file: null,
        secret: false,
        restart_reason: null,
        declared: true,
        origin: "gameserver.conf line 1",
        type: type as Setting["type"],
        declared_default: value,
        min: null,
        max: null,
        bounds: "",
        unit: "",
        category: "World",
        description: `${key} for the test`,
        apply: "live",
        lock: null,
        visibility: "normal",
        edit: "normal",
        persisted: null,
        revealed: false,
        ...extra,
    };
}

const drop = live("Rate.Drop.Item", "float", "1", {
    min: "0",
    max: "100",
    bounds: "from 0 to 100 times",
    unit: "times",
    category: "Rates",
});
const tick = live("World.UpdateInterval", "unsigned", "50", { min: "1", max: "10000", bounds: "from 1 to 10000 ms", unit: "ms" });
const warning = live("Login.AfkWarning", "integer", "1", { min: "-128", max: "127", bounds: "from -128 to 127", category: "Login" });
const flag = live("GM.LogCommands", "bool", "true", { category: "Commands" });
const prefix = live("GM.CommandPrefix", "string", ".", { max: "8", bounds: "at most 8 bytes", category: "Commands" });
const keys = live("Account.VerifierKeys", "string", "1:***", {
    secret: true,
    visibility: "secret",
    edit: "restricted",
    category: "Accounts",
});
const locked = live("Zone.UnloadDelay", "unsigned", "90", {
    lock: { layer: "override", origin: "a command-line override" },
    layer: "override",
    category: "Zones",
});
const file = { ...live("LogsDir", "string", "logs"), declared: false, type: undefined };

describe("checking a value before it is sent", () => {
    it("holds each type to its bounds and names them", () => {
        expect(checkValue(drop, "2.5")).toBeNull();
        expect(checkValue(drop, "101")).toContain("from 0 to 100 times");
        expect(checkValue(drop, "fast")).toContain("takes a number");
        expect(checkValue(drop, "1e400")).toContain("takes a number");
        expect(checkValue(tick, "0")).toContain("from 1 to 10000 ms");
        expect(checkValue(tick, "-5")).toContain("zero or more");
        expect(checkValue(tick, "12.5")).toContain("whole number");
        expect(checkValue(tick, "10000")).toBeNull();
        expect(checkValue(warning, "-128")).toBeNull();
        expect(checkValue(warning, "-129")).toContain("from -128 to 127");
        expect(checkValue(flag, "maybe")).toContain("true or false");
        expect(checkValue(flag, "Yes")).toBeNull();
        expect(checkValue(prefix, "!!!!!!!!!")).toContain("at most 8 bytes");
        expect(checkValue(prefix, "é".repeat(4))).toBeNull();
        expect(checkValue(prefix, "é".repeat(5))).toContain("the value given has 10");
        expect(checkValue(file, "x")).toContain("not a live setting");
    });

    it("sends a value in the form the server stores", () => {
        expect(normalise(flag, " Yes ")).toBe("true");
        expect(normalise(flag, "0")).toBe("false");
        expect(normalise(tick, "+0100")).toBe("100");
        expect(normalise(drop, "2.50")).toBe("2.5");
        expect(normalise(prefix, "  !  ")).toBe("!");
        expect(normalise(prefix, "  ! \t")).toBe("!");
        expect(normalise(prefix, "Welcome\u00a0")).toBe("Welcome\u00a0");
        expect(checkValue(prefix, "\u3000".repeat(3))).toContain("the value given has 9");
    });

    it("writes a small or large number the way the server does", () => {
        expect(normalise(drop, "0.00005")).toBe("5e-05");
        expect(normalise(drop, "0.0001")).toBe("0.0001");
        expect(normalise(drop, "1e-7")).toBe("1e-07");
        expect(normalise(drop, "1.25e-10")).toBe("1.25e-10");
        expect(normalise(drop, "1e16")).toBe("1e+16");
        expect(normalise(drop, "1e15")).toBe("1000000000000000");
        expect(normalise(drop, "1e100")).toBe("1e+100");
        expect(normalise(drop, "0")).toBe("0");
        expect(normalise(drop, "-0")).toBe("-0");
    });

    it("clips a reason by bytes without splitting a character", () => {
        const reason = "Revert change 3, which was for: " + "é".repeat(200);
        const clipped = clipBytes(reason, 255);
        expect(textBytes(clipped)).toBeLessThanOrEqual(255);
        expect(textBytes(clipped)).toBeGreaterThanOrEqual(254);
        expect(clipped.endsWith("é")).toBe(true);
        expect(clipBytes("😀😀", 5)).toBe("😀");
        expect(clipBytes("short", 255)).toBe("short");
    });
});

describe("the list", () => {
    it("groups only live settings by category and searches keys, values and descriptions", () => {
        const all = [drop, tick, flag, file, keys];
        expect(categories(all, "").map((group) => group.name)).toEqual(["Accounts", "Commands", "Rates", "World"]);
        expect(categories(all, "drop").flatMap((group) => group.settings.map((setting) => setting.key))).toEqual(["Rate.Drop.Item"]);
        expect(categories(all, "for the test").length).toBe(4);
        expect(categories(all, "***")).toEqual([]);
    });
});

describe("settings presets", () => {
    it("never carries a secret or an option only the config file holds", () => {
        const preset = makePreset(
            "gameserver",
            [drop, tick, keys, file],
            ["World.UpdateInterval", "Account.VerifierKeys", "LogsDir", "Rate.Drop.Item"],
            new Date(0),
        );
        expect(preset.format).toBe("ambrose-settings-preset");
        expect(preset.version).toBe(1);
        expect(preset.settings).toEqual({ "Rate.Drop.Item": "1", "World.UpdateInterval": "50" });
    });

    it("names what is wrong with a file that is not a preset", () => {
        expect(readPreset("not json")).toEqual({ error: expect.stringContaining("not JSON") });
        expect(readPreset("[]")).toEqual({ error: expect.stringContaining("JSON object") });
        expect(readPreset('{"format":"other","version":1,"settings":{}}')).toEqual({
            error: expect.stringContaining("not a settings preset"),
        });
        expect(readPreset('{"format":"ambrose-settings-preset","version":2,"settings":{}}')).toEqual({
            error: expect.stringContaining("newer panel"),
        });
        expect(readPreset('{"format":"ambrose-settings-preset","version":1,"settings":{"A.B":[1]}}')).toEqual({
            error: expect.stringContaining("A.B"),
        });
        const read = readPreset(
            '{"format":"ambrose-settings-preset","version":1,"app":"gameserver","settings":{"Rate.Drop.Item":2,"GM.LogCommands":false}}',
        );
        expect(read).toEqual({ preset: expect.objectContaining({ settings: { "Rate.Drop.Item": "2", "GM.LogCommands": "false" } }) });
    });

    it("reads back a preset exported from an app with a locked key, which already holds its value", () => {
        const preset = makePreset("gameserver", [drop, locked], ["Rate.Drop.Item", "Zone.UnloadDelay"], new Date(0));
        expect(preset.settings).toEqual({ "Rate.Drop.Item": "1", "Zone.UnloadDelay": "90" });
        const rows = diffPreset(preset, [drop, locked]);
        expect(rows.map((row) => [row.key, row.status])).toEqual([
            ["Rate.Drop.Item", "same"],
            ["Zone.UnloadDelay", "same"],
        ]);
    });

    it("marks what changes, what stays and what would be refused, with why", () => {
        const read = readPreset(
            JSON.stringify({
                format: "ambrose-settings-preset",
                version: 1,
                settings: {
                    "Rate.Drop.Item": "250",
                    "World.UpdateInterval": "50",
                    "GM.LogCommands": "no",
                    "Zone.UnloadDelay": "30",
                    "Account.VerifierKeys": "x",
                    "No.Such": "1",
                },
            }),
        );
        if (!("preset" in read)) throw new Error(read.error);
        const rows = diffPreset(read.preset, [drop, tick, flag, locked, keys]);
        const byKey = Object.fromEntries(rows.map((row) => [row.key, row]));
        expect(byKey["Rate.Drop.Item"].status).toBe("refused");
        expect(byKey["Rate.Drop.Item"].problem).toContain("from 0 to 100 times");
        expect(byKey["World.UpdateInterval"].status).toBe("same");
        expect(byKey["GM.LogCommands"]).toEqual(expect.objectContaining({ status: "change", current: "true", next: "false" }));
        expect(byKey["Zone.UnloadDelay"].problem).toContain("command-line override");
        expect(byKey["Account.VerifierKeys"].problem).toContain("secret");
        expect(byKey["No.Such"].problem).toContain("no live setting");

        const checked = applyRefusals(rows, [{ key: "GM.LogCommands", message: "a check the server holds refused it" }]);
        expect(checked.find((row) => row.key === "GM.LogCommands")).toEqual(
            expect.objectContaining({ status: "refused", problem: "a check the server holds refused it" }),
        );
    });
});
