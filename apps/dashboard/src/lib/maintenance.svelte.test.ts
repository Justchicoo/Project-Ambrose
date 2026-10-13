/*
 * Project Ambrose by Imjustchico
 * Tests what the maintenance banner and card decide before they ask the server: the answer's shape, and the window's description naming both ends or nothing when the window is absent or half set.
 */

import { describe, expect, it } from "vitest";
import * as v from "valibot";
import { describeWindow } from "./maintenance.svelte";
import { MaintenanceAnswer, type MaintenanceState } from "./schemas";

function state(extra: Partial<MaintenanceState> = {}): MaintenanceState {
    return {
        active: false,
        reason: "",
        started_by: "",
        started_epoch_ms: 0,
        window_start_epoch_ms: null,
        window_end_epoch_ms: null,
        ...extra,
    };
}

describe("MaintenanceAnswer", () => {
    it("parses the panel's record", () => {
        const parsed = v.safeParse(MaintenanceAnswer, {
            active: true,
            reason: "Database upgrade",
            started_by: "op",
            started_epoch_ms: 1000,
            window_start_epoch_ms: 2000,
            window_end_epoch_ms: 3000,
            extra: "kept",
        });
        expect(parsed.success).toBe(true);
    });

    it("refuses a missing active flag", () => {
        const parsed = v.safeParse(MaintenanceAnswer, { reason: "x" });
        expect(parsed.success).toBe(false);
    });
});

describe("describeWindow", () => {
    it("names both ends when the window is set", () => {
        const text = describeWindow(state({ window_start_epoch_ms: 1000, window_end_epoch_ms: 2000 }));
        expect(text).not.toBeNull();
        expect(text).toContain("–");
    });

    it("says nothing when the window is absent or half set", () => {
        expect(describeWindow(state())).toBeNull();
        expect(describeWindow(state({ window_start_epoch_ms: 1000 }))).toBeNull();
        expect(describeWindow(state({ window_end_epoch_ms: 2000 }))).toBeNull();
    });
});
