/*
 * Project Ambrose by Imjustchico
 * Checks that the generated stylesheet, the generated TypeScript and the generated C++ header carry the same value for every token, and that every documented pair still clears its ratio.
 */

import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { componentColors, contrastPairs, durationMs, semanticColors, seriesColors, terminalColors, themes } from "../tokens/tokens";

const css = readFileSync(fileURLToPath(new URL("../tokens/tokens.css", import.meta.url)), "utf8");
const header = readFileSync(fileURLToPath(new URL("../../../../src/common/Design/Tokens.h", import.meta.url)), "utf8");

function channel(value: number): number {
    const scaled = value / 255;
    return scaled <= 0.04045 ? scaled / 12.92 : ((scaled + 0.055) / 1.055) ** 2.4;
}

function luminance(hex: string): number {
    const red = channel(Number.parseInt(hex.slice(1, 3), 16));
    const green = channel(Number.parseInt(hex.slice(3, 5), 16));
    const blue = channel(Number.parseInt(hex.slice(5, 7), 16));
    return 0.2126 * red + 0.7152 * green + 0.0722 * blue;
}

function contrast(first: string, second: string): number {
    const one = luminance(first);
    const two = luminance(second);
    return (Math.max(one, two) + 0.05) / (Math.min(one, two) + 0.05);
}

function pick(path: string, theme: "dark" | "light"): string {
    const [group, , name] = path.split(".");
    if (group === "semantic") {
        return semanticColors[theme][name as keyof typeof semanticColors.dark];
    }
    if (group === "component") {
        return componentColors[theme][name as keyof typeof componentColors.dark];
    }
    return seriesColors[theme][Number.parseInt(name, 10) - 1];
}

describe("the generated token files", () => {
    it("names both themes", () => {
        expect(themes).toEqual(["dark", "light"]);
    });

    it("gives the stylesheet the same value as the TypeScript for every meaning", () => {
        for (const theme of themes) {
            for (const [name, value] of Object.entries(semanticColors[theme])) {
                expect(css).toContain(`--ambrose-color-${name}: ${value};`);
            }
        }
    });

    it("gives the C++ header the same value as the TypeScript for every meaning", () => {
        for (const theme of themes) {
            for (const [name, value] of Object.entries(semanticColors[theme])) {
                expect(header).toContain(`"${name}", "${value}"`);
            }
        }
    });

    it("gives the C++ header the same terminal indexes as the TypeScript", () => {
        for (const theme of themes) {
            for (const [name, entry] of Object.entries(terminalColors[theme])) {
                const line = header.split("\n").find((candidate: string) => candidate.includes(`"${name}", "${entry.hex}"`));
                expect(line, `${theme} ${name}`).toBeDefined();
                expect(line).toContain(`, ${entry.index256}, ${entry.index16}}`);
            }
        }
    });

    it("gives the C++ header the same durations as the TypeScript", () => {
        expect(header).toContain(`DurationFlipMs = ${durationMs.flip};`);
        expect(header).toContain(`DurationHoverMs = ${durationMs.hover};`);
        expect(header).toContain(`DurationPanelMs = ${durationMs.panel};`);
        expect(header).toContain(`DurationScreenMs = ${durationMs.screen};`);
    });

    it("deletes the stock palette before it writes ours", () => {
        expect(css.indexOf("--color-*: initial;")).toBeGreaterThanOrEqual(0);
        expect(css.indexOf("--color-*: initial;")).toBeLessThan(css.indexOf("--color-surface-page:"));
    });

    it("has every documented pair clear the ratio it needs", () => {
        expect(contrastPairs.length).toBeGreaterThan(50);
        for (const pair of contrastPairs) {
            const reached = contrast(pick(pair.foreground, pair.theme), pick(pair.background, pair.theme));
            expect(Math.round(reached * 100) / 100, `${pair.theme} ${pair.foreground} on ${pair.background}`).toBeGreaterThanOrEqual(
                pair.minimum,
            );
            expect(Math.abs(reached - pair.ratio)).toBeLessThan(0.01);
        }
    });

    it("collapses every duration to zero when the viewer asks for less motion", () => {
        for (const name of Object.keys(durationMs)) {
            expect(css).toContain(`--ambrose-duration-${name}: 0ms;`);
        }
        expect(css).toContain("@media (prefers-reduced-motion: reduce) {");
        expect(css).toContain('[data-motion="off"]');
    });
    it("carries the same value in every block that declares a meaning", () => {
        const blockOf = (marker: string): string => {
            const at = css.indexOf(marker);
            expect(at, marker).toBeGreaterThanOrEqual(0);
            const open = css.indexOf("{", at);
            let depth = 0;
            let index = open;
            for (; index < css.length; index += 1) {
                if (css[index] === "{") {
                    depth += 1;
                } else if (css[index] === "}") {
                    depth -= 1;
                    if (depth === 0) {
                        break;
                    }
                }
            }
            return css.slice(open, index);
        };
        const blocks: Array<[string, "dark" | "light"]> = [
            ["@theme {", "dark"],
            [":root {", "dark"],
            [':root[data-theme="light"] {', "light"],
            ["@media (prefers-color-scheme: light) {", "light"],
        ];
        let checked = 0;
        for (const [marker, theme] of blocks) {
            const body = blockOf(marker);
            const expected: Record<string, string> = {
                ...semanticColors[theme],
                ...componentColors[theme],
            };
            for (const found of body.matchAll(/--(?:ambrose-)?color-([a-z0-9-]+):\s*([^;]+);/g)) {
                const name = found[1];
                if (name in expected) {
                    const wanted = marker === "@theme {" ? `var(--ambrose-color-${name})` : expected[name];
                    expect(found[2].trim(), `${marker} ${name} in the ${theme} theme`).toBe(wanted);
                    checked += 1;
                }
            }
        }
        expect(checked).toBeGreaterThan(100);
    });
    it("gives every entry in the C++ header its theme's value", () => {
        const arrayOf = (name: string): string => {
            const at = header.indexOf(`${name} = {{`);
            expect(at, name).toBeGreaterThanOrEqual(0);
            const end = header.indexOf("}};", at);
            expect(end, name).toBeGreaterThan(at);
            return header.slice(at, end);
        };
        const arrays: Array<[string, "dark" | "light"]> = [
            ["DarkTokens", "dark"],
            ["LightTokens", "light"],
            ["DarkSeries", "dark"],
            ["LightSeries", "light"],
        ];
        let checked = 0;
        for (const [arrayName, theme] of arrays) {
            const body = arrayOf(arrayName);
            const expected: Record<string, string> = {
                ...semanticColors[theme],
                ...componentColors[theme],
            };
            for (const found of body.matchAll(/TerminalColor\{"([a-z0-9-]+)", "(#[0-9A-Fa-f]{6})"/g)) {
                const name = found[1];
                if (name in expected) {
                    expect(found[2], `${arrayName} ${name}`).toBe(expected[name]);
                    checked += 1;
                }
            }
        }
        expect(checked).toBeGreaterThan(30);
    });
});
