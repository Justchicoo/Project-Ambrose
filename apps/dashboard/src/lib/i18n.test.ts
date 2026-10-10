/*
 * Project Ambrose by Imjustchico
 * Tests for 17.66 dashboard localization: every t() key used in the dashboard exists in the default catalog so a string added without a key fails the suite, a locale missing a key falls back key by key instead of rendering blank with the report naming it, switching locale re-renders translated text while values stay unchanged, and absolute times name the operator's time zone.
 */

import { readdirSync, readFileSync, statSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { beforeEach, describe, expect, it } from "vitest";
import en from "../../locales/en.json";
import {
    chooseLocale,
    clearUserChoice,
    formatDateTime,
    formatDuration,
    formatNumber,
    i18n,
    missingKeys,
    normalizeLocale,
    setInstallationDefault,
    t,
} from "./i18n.svelte.js";

const srcDir = join(dirname(fileURLToPath(import.meta.url)), "..");

function sourceFiles(dir: string): string[] {
    const found: string[] = [];
    for (const entry of readdirSync(dir)) {
        const full = join(dir, entry);
        if (statSync(full).isDirectory()) {
            if (entry === "components" && dir.endsWith("lib")) {
                found.push(...sourceFiles(full));
                continue;
            }
            found.push(...sourceFiles(full));
        } else if (/\.(svelte|ts)$/.test(entry) && !entry.endsWith(".test.ts")) {
            found.push(full);
        }
    }
    return found;
}

function usedKeys(): Set<string> {
    const keys = new Set<string>();
    const pattern = /\bt\(\s*["']([^"']+)["']/g;
    for (const file of sourceFiles(srcDir)) {
        const text = readFileSync(file, "utf8");
        let match: RegExpExecArray | null;
        pattern.lastIndex = 0;
        while ((match = pattern.exec(text)) !== null) keys.add(match[1]);
    }
    return keys;
}

beforeEach(() => {
    chooseLocale("en");
});

describe("the default catalog", () => {
    it("holds every key the dashboard renders through t()", () => {
        const missing = [...usedKeys()].filter((key) => !(key in en));
        expect(missing, `keys used but missing from locales/en.json: ${missing.join(", ")}`).toEqual([]);
    });

    it("has no key twice and no empty value", () => {
        for (const [key, value] of Object.entries(en)) {
            expect(value.trim().length, `empty value for ${key}`).toBeGreaterThan(0);
        }
    });
});

describe("locale resolution", () => {
    it("normalizes BCP 47 tags to the catalog", () => {
        expect(normalizeLocale("en-US")).toBe("en");
        expect(normalizeLocale("ES")).toBe("es");
        expect(normalizeLocale("es-419")).toBe("es");
        expect(normalizeLocale("xx")).toBe("en");
    });

    it("takes the installation default when the user chose nothing", () => {
        clearUserChoice();
        setInstallationDefault("es");
        expect(i18n.locale).toBe("es");
        expect(t("nav.settings")).toBe("Ajustes");
        setInstallationDefault("en");
    });

    it("keeps the user's choice ahead of the installation default", () => {
        chooseLocale("es");
        setInstallationDefault("en");
        expect(i18n.locale).toBe("es");
    });
});

describe("switching locale", () => {
    it("renders the navigation and settings strings in it", () => {
        chooseLocale("es");
        expect(t("nav.overview")).toBe("Resumen");
        expect(t("nav.settings")).toBe("Ajustes");
        expect(t("settings.title")).toBe("Ajustes del panel");
        expect(t("settings.save")).toBe("Guardar cambios");
        chooseLocale("en");
        expect(t("nav.overview")).toBe("Overview");
        expect(t("settings.save")).toBe("Save changes");
    });

    it("leaves values unchanged while the chrome translates", () => {
        chooseLocale("es");
        expect(t("settings.sentTo", { to: "ops@example.com" })).toBe("Se envió un correo de prueba a ops@example.com.");
        expect(t("denied.detail", { page: "Resumen", permission: "status.read" })).toContain("status.read");
    });
});

describe("fallback", () => {
    it("falls back key by key to the default text, never blank", () => {
        chooseLocale("es");
        expect(t("nav.errors")).toBe(en["nav.errors" as keyof typeof en]);
        expect(t("settings.sendTest")).toBe(en["settings.sendTest" as keyof typeof en]);
        expect(t("nav.errors").length).toBeGreaterThan(0);
    });

    it("reports the keys a locale is missing", () => {
        const missing = missingKeys("es");
        expect(missing).toContain("nav.errors");
        expect(missing).toContain("settings.sendTest");
        expect(missingKeys("en")).toEqual([]);
    });

    it("returns the key itself when nothing holds it", () => {
        expect(t("no.such.key.anywhere")).toBe("no.such.key.anywhere");
    });
});

describe("browser locale formatting", () => {
    it("names the time zone beside absolute times", () => {
        const rendered = formatDateTime(new Date("2026-10-10T08:00:00Z"));
        const withoutZone = new Intl.DateTimeFormat(i18n.locale, { dateStyle: "medium", timeStyle: "short" }).format(
            new Date("2026-10-10T08:00:00Z"),
        );
        expect(rendered.length).toBeGreaterThan(withoutZone.length);
        expect(rendered).not.toBe(withoutZone);
    });

    it("formats numbers and durations in the operator's locale", () => {
        chooseLocale("es");
        expect(formatNumber(1234567)).not.toBe(formatNumber(1234567).replace(/[^0-9]/g, ""));
        expect(formatDuration(90)).toContain("minuto");
        chooseLocale("en");
        expect(formatDuration(90)).toContain("minute");
        expect(formatDuration(3600)).toContain("hour");
    });
});
