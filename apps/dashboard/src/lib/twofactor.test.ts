/*
 * Project Ambrose by Imjustchico
 * Tests the two-factor rules the pages share with the server: a setup secret grouped in fours for typing, a code read as six digits with its spaces and dashes ignored and nothing else accepted, a recovery code read in any case with or without its dash and with the letters people confuse for digits read as those digits, and the saved text naming the account and carrying every code grouped.
 */

import { describe, expect, it } from "vitest";
import { groupRecoveryCode, groupSecret, readCode, readRecoveryCode, recoveryCodesText } from "./twofactor";

describe("the setup secret", () => {
    it("is grouped in fours for typing", () => {
        expect(groupSecret("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ")).toBe("GEZD GNBV GY3T QOJQ GEZD GNBV GY3T QOJQ");
        expect(groupSecret("mzxw6ytboi")).toBe("MZXW 6YTB OI");
        expect(groupSecret("MZXW6YQ=")).toBe("MZXW 6YQ");
    });
});

describe("a typed code", () => {
    it("is six digits once its spaces and dashes are gone", () => {
        expect(readCode("287082")).toBe("287082");
        expect(readCode(" 287 082 ")).toBe("287082");
        expect(readCode("287-082")).toBe("287082");
    });

    it("is nothing else", () => {
        expect(readCode("28708")).toBeNull();
        expect(readCode("2870821")).toBeNull();
        expect(readCode("28708a")).toBeNull();
        expect(readCode("")).toBeNull();
    });
});

describe("a typed recovery code", () => {
    it("is read in any case, with or without its dash", () => {
        expect(readRecoveryCode("7K2QM-XR4TD")).toBe("7K2QMXR4TD");
        expect(readRecoveryCode("7k2qmxr4td")).toBe("7K2QMXR4TD");
        expect(readRecoveryCode(" 7k2qm - xr4td ")).toBe("7K2QMXR4TD");
    });

    it("reads the letters people confuse for digits as those digits", () => {
        expect(readRecoveryCode("1O000-00000")).toBe("1000000000");
        expect(readRecoveryCode("ILl00-00000")).toBe("1110000000");
    });

    it("is nothing when it is not ten characters of the alphabet", () => {
        expect(readRecoveryCode("7K2QM-XR4T")).toBeNull();
        expect(readRecoveryCode("7K2QM-XR4TDD")).toBeNull();
        expect(readRecoveryCode("7K2QM-XR4TU")).toBeNull();
        expect(readRecoveryCode("")).toBeNull();
    });

    it("is shown in two groups of five", () => {
        expect(groupRecoveryCode("7k2qmxr4td")).toBe("7K2QM-XR4TD");
        expect(groupRecoveryCode("7K2QM-XR4TD")).toBe("7K2QM-XR4TD");
    });
});

describe("the saved codes", () => {
    it("name the account and carry every code grouped", () => {
        const text = recoveryCodesText(["7K2QMXR4TD", "ABCDE-FGHJK"], "merle", "Ambrose", new Date(Date.UTC(2026, 8, 27, 12, 0, 0)));
        expect(text).toContain("Recovery codes for merle on Ambrose");
        expect(text).toContain("2026-09-27T12:00:00.000Z");
        expect(text).toContain("\n7K2QM-XR4TD\nABCDE-FGHJK\n");
    });
});
