/*
 * Project Ambrose by Imjustchico
 * Tests that the restart-required list the reload page shows is the one doc/OPERATIONS.md gives, every change and its reason word for word and in the same order, so neither can change without the other.
 */

import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";
import { restartCases } from "./restarts";

function documented(): { change: string; reason: string }[] {
    const text = readFileSync(new URL("../../../../doc/OPERATIONS.md", import.meta.url), "utf8");
    const section = text.split(/^## /m).find((part) => part.startsWith("Restart-required changes"));
    if (!section) throw new Error("doc/OPERATIONS.md has no Restart-required changes section");
    return section
        .split(/\r?\n/)
        .filter((line) => line.startsWith("| ") && !line.startsWith("| Change ") && !line.startsWith("| ---"))
        .map((line) => {
            const [change, reason] = line
                .split("|")
                .slice(1, -1)
                .map((cell) => cell.trim());
            return { change, reason };
        });
}

describe("the restart-required list", () => {
    it("is the table doc/OPERATIONS.md gives, word for word", () => {
        const table = documented();
        expect(table.length).toBe(5);
        expect(restartCases).toEqual(table);
    });
});
