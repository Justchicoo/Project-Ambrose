/*
 * Project Ambrose by Imjustchico
 * Tests reading the one-time links a page is opened from: an owner claim, a password link and a sign-in link each give their page and token, only a sign-in link carries a pin, any other address gives nothing, and without a window there is nothing to take.
 */

import { describe, expect, it } from "vitest";
import { arrivedWith, readLink, takeArrival } from "./links";

describe("reading a link from an address", () => {
    it("reads the page, token and pin a link carries and nothing from any other address", () => {
        expect(readLink("#link?token=abc-DEF_123&sha256=AA:BB:CC")).toEqual({ page: "link", token: "abc-DEF_123", pin: "AA:BB:CC" });
        expect(readLink("#/link?token=abc")).toEqual({ page: "link", token: "abc", pin: "" });
        expect(readLink("#claim?token=owner-token")).toEqual({ page: "claim", token: "owner-token", pin: "" });
        expect(readLink("#password?token=reset-token&sha256=AA")).toEqual({ page: "password", token: "reset-token", pin: "" });
        for (const other of [
            "",
            "#",
            "#overview",
            "#link",
            "#link?",
            "#link?sha256=AA",
            "#link?token=",
            "#settings?token=abc",
            "#files?root=logs&token=abc",
            "#linked?token=abc",
        ])
            expect(readLink(other), other).toBeNull();
    });

    it("takes nothing where there is no window to take it from", () => {
        expect(takeArrival()).toBeNull();
        expect(arrivedWith.link).toBeNull();
    });
});
