/*
 * Project Ambrose by Imjustchico
 * Checks the motion rules: four durations and no more, and every one of them zero when the viewer asks for less motion, whichever way they ask.
 */

import { describe, expect, it } from "vitest";
import { durationMs, easing } from "../tokens/tokens";
import { motion, motionSettings } from "../motion/motion.svelte";

describe("the motion module", () => {
    it("has the four durations the design allows and no others", () => {
        expect(Object.keys(durationMs).sort()).toEqual(["flip", "hover", "panel", "screen"]);
        expect(durationMs).toEqual({ flip: 90, hover: 120, panel: 200, screen: 320 });
    });

    it("never overshoots, because every easing ends at its target", () => {
        for (const curve of Object.values(easing)) {
            expect(curve.startsWith("cubic-bezier(")).toBe(true);
            const points = curve
                .slice("cubic-bezier(".length, -1)
                .split(",")
                .map((part) => Number.parseFloat(part));
            expect(points).toHaveLength(4);
            expect(points[1]).toBeLessThanOrEqual(1);
            expect(points[3]).toBeLessThanOrEqual(1);
        }
    });

    it("offers exactly the three settings", () => {
        expect([...motionSettings]).toEqual(["system", "on", "off"]);
    });

    it("keeps the real durations when nothing asks for less", () => {
        motion.choose("system");
        motion.systemAsksForLess = false;
        expect(motion.off).toBe(false);
        expect(motion.durations()).toEqual(durationMs);
    });

    it("collapses every duration to zero when the system asks for less", () => {
        motion.choose("system");
        motion.systemAsksForLess = true;
        expect(motion.off).toBe(true);
        for (const value of Object.values(motion.durations())) {
            expect(value).toBe(0);
        }
    });

    it("collapses every duration to zero when the viewer turns motion off, whatever the system says", () => {
        motion.systemAsksForLess = false;
        motion.choose("off");
        expect(motion.off).toBe(true);
        expect(motion.duration("screen")).toBe(0);
    });

    it("keeps the real durations when the viewer turns motion on, whatever the system says", () => {
        motion.systemAsksForLess = true;
        motion.choose("on");
        expect(motion.off).toBe(false);
        expect(motion.duration("screen")).toBe(durationMs.screen);
        motion.choose("system");
        motion.systemAsksForLess = false;
    });
});
