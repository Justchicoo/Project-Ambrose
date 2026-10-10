/*
 * Project Ambrose by Imjustchico
 * Tests what the operations calendar decides before it asks the server: the answer's shape, week and month starts landing on Monday and the first in the operator's zone, the range naming an ongoing entry, and the conflict reason found for either entry of the pair.
 */

import { describe, expect, it } from "vitest";
import * as v from "valibot";
import {
    addDaysMs,
    conflictReasonFor,
    formatRange,
    sourceLabel,
    startOfMonthMs,
    startOfWeekMs,
} from "./opsCalendar.svelte";
import { OpsCalendarAnswer } from "./schemas";

describe("OpsCalendarAnswer", () => {
    it("parses the panel's answer", () => {
        const parsed = v.safeParse(OpsCalendarAnswer, {
            events: [
                {
                    id: "installation-maintenance",
                    source: "installation_maintenance",
                    color: "#ef4444",
                    title: "Installation maintenance",
                    detail: "db upgrade",
                    start_epoch_ms: 2000,
                    end_epoch_ms: 3000,
                    disruptive: true,
                    url: "#/overview",
                },
            ],
            sources: [
                { source: "schedules", color: "#3b82f6", available: false, unavailable_reason: "17.15 has not landed" },
                { source: "installation_maintenance", color: "#ef4444", available: true },
            ],
            conflicts: [{ event_a_id: "a", event_b_id: "b", reason: "a runs across b" }],
            server_utc_offset_minutes: -240,
        });
        expect(parsed.success).toBe(true);
    });

    it("refuses an event without times", () => {
        const parsed = v.safeParse(OpsCalendarAnswer, {
            events: [{ id: "x", source: "schedules" }],
            sources: [],
            conflicts: [],
            server_utc_offset_minutes: 0,
        });
        expect(parsed.success).toBe(false);
    });
});

describe("week and month starts", () => {
    const zone = "America/Santo_Domingo";
    // 2026-10-10 is a Saturday.
    const saturday = Date.UTC(2026, 9, 10, 12, 0, 0);

    it("starts the week on Monday", () => {
        const monday = startOfWeekMs(saturday, zone);
        expect(new Intl.DateTimeFormat("en-US", { timeZone: zone, weekday: "short" }).format(monday)).toBe("Mon");
        expect(monday).toBeLessThanOrEqual(saturday);
        expect(saturday - monday).toBeLessThan(7 * 24 * 60 * 60 * 1000);
    });

    it("starts the month on the first", () => {
        const first = startOfMonthMs(saturday, zone);
        expect(new Intl.DateTimeFormat("en-US", { timeZone: zone, day: "numeric" }).format(first)).toBe("1");
        expect(first).toBeLessThanOrEqual(saturday);
    });

    it("adds days", () => {
        expect(addDaysMs(saturday, 7) - saturday).toBe(7 * 24 * 60 * 60 * 1000);
    });
});

describe("ranges and conflicts", () => {
    it("names an ongoing entry", () => {
        expect(
            formatRange({
                id: "x",
                source: "installation_maintenance",
                color: "#ef4444",
                title: "t",
                detail: "",
                start_epoch_ms: 1000,
                end_epoch_ms: 0,
                disruptive: true,
                url: "",
            }),
        ).toContain("ongoing");
    });

    it("finds the conflict reason for either entry", () => {
        const conflicts = [{ event_a_id: "a", event_b_id: "b", reason: "a runs across b" }];
        expect(conflictReasonFor("a", conflicts)).toBe("a runs across b");
        expect(conflictReasonFor("b", conflicts)).toBe("a runs across b");
        expect(conflictReasonFor("c", conflicts)).toBeNull();
    });

    it("labels the sources", () => {
        expect(sourceLabel("installation_maintenance")).toBe("Installation maintenance");
        expect(sourceLabel("schedules")).toBe("Schedules");
    });
});
