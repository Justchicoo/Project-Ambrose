/*
 * Project Ambrose by Imjustchico
 * Tests the live picture's pieces that decide what a reader sees: the ring keeps the newest fifteen minutes in order with gaps where no reading came, a sample goes stale past three intervals, the retry delay grows and then holds, and figures are written in the units the panel uses.
 */

import { describe, expect, it } from "vitest";
import { formatAge, formatBytes, formatUptime } from "./format";
import { FreshnessMs, IntervalMs, isStale, retryDelay, Ring, WindowSeconds } from "./status.svelte";

describe("the ring", () => {
    it("keeps readings oldest first with gaps as nulls", () => {
        const ring = new Ring(4);
        ring.push(1, 10);
        ring.push(2, null);
        ring.push(3, 30);
        expect(ring.read()).toEqual({ times: [1, 2, 3], values: [10, null, 30] });
    });

    it("drops the oldest reading once full", () => {
        const ring = new Ring(3);
        for (let time = 1; time <= 5; time += 1) ring.push(time, time * 10);
        expect(ring.read()).toEqual({ times: [3, 4, 5], values: [30, 40, 50] });
        expect(ring.length).toBe(3);
        ring.clear();
        expect(ring.read()).toEqual({ times: [], values: [] });
    });

    it("holds fifteen minutes at one reading a second", () => {
        expect(WindowSeconds).toBe(900);
        expect(IntervalMs).toBe(1000);
    });
});

describe("freshness and retries", () => {
    it("calls a sample stale past three intervals and one never read stale", () => {
        expect(FreshnessMs).toBe(3 * IntervalMs);
        expect(isStale(0, 5000)).toBe(true);
        expect(isStale(10_000, 10_000 + FreshnessMs)).toBe(false);
        expect(isStale(10_000, 10_000 + FreshnessMs + 1)).toBe(true);
    });

    it("waits longer after each failed attempt and then holds", () => {
        expect([1, 2, 3, 4, 5, 6, 50].map(retryDelay)).toEqual([1000, 2000, 4000, 8000, 10000, 10000, 10000]);
        expect(retryDelay(0)).toBe(1000);
    });
});

describe("writing figures", () => {
    it("writes uptime in its two largest units", () => {
        expect([12, 185, 7_380, 187_200].map(formatUptime)).toEqual(["12 s", "3 min 5 s", "2 h 3 min", "2 d 4 h"]);
    });

    it("writes bytes in binary units", () => {
        expect([512, 1536, 190_840_832, 5 * 1024 ** 3].map(formatBytes)).toEqual(["512 B", "1.5 KiB", "182 MiB", "5.0 GiB"]);
    });

    it("writes an age in the unit that reads fastest", () => {
        expect([400, 14_000, 125_000, 7_300_000].map(formatAge)).toEqual(["0 s", "14 s", "2 min", "2 h"]);
    });
});
