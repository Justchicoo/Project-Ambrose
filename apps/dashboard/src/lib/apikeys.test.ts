/*
 * Project Ambrose by Imjustchico
 * Tests the API key text the pages share with the server: a minted key reads amb_ plus a twelve-character public id plus a dot plus a fifty-two character base32 secret, and nothing else passes as one.
 */

import { describe, expect, it } from "vitest";
import { isKeyText } from "./apikeys";

const good = `amb_${"A".repeat(12)}.${"B".repeat(52)}`;

describe("an API key", () => {
    it("reads amb_ plus a public id plus a base32 secret", () => {
        expect(isKeyText(good)).toBe(true);
        expect(isKeyText(`amb_${"234567".repeat(2)}.${"ABCD2345".repeat(7).slice(0, 52)}`)).toBe(true);
    });

    it("is nothing else", () => {
        expect(isKeyText("")).toBe(false);
        expect(isKeyText("bearer-token")).toBe(false);
        expect(isKeyText(`amb_${"A".repeat(11)}.${"B".repeat(52)}`)).toBe(false);
        expect(isKeyText(`amb_${"A".repeat(12)}${"B".repeat(52)}`)).toBe(false);
        expect(isKeyText(`amb_${"A".repeat(12)}.${"B".repeat(51)}`)).toBe(false);
        expect(isKeyText(`amb_${"a".repeat(12)}.${"B".repeat(52)}`)).toBe(false);
        expect(isKeyText(`amb_${"A".repeat(12)}.${"0".repeat(52)}`)).toBe(false);
    });
});
