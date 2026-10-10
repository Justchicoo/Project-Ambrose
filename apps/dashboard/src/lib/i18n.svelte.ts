/*
 * Project Ambrose by Imjustchico
 * The dashboard's own words in the operator's language: one catalog per locale with key-by-key fallback to the default, the installation default from 17.35's Panel.Locale, a per-user choice remembered in this browser until 17.38 owns the account page, dates numbers and durations through the browser's own Intl with the zone named beside every absolute time, and a report of the keys each locale still misses.
 */

import en from "../../locales/en.json";
import es from "../../locales/es.json";

export const defaultLocale = "en";

const catalogs: Record<string, Record<string, string>> = { en, es };

export const availableLocales = [
    { tag: "en", name: "English" },
    { tag: "es", name: "Español" },
] as const;

const storageKey = "ambrose.panel.locale";

export function normalizeLocale(tag: string): string {
    const lower = tag.trim().toLowerCase();
    if (catalogs[lower]) return lower;
    const language = lower.split(/[-_]/)[0];
    if (catalogs[language]) return language;
    return defaultLocale;
}

function remembered(): string | null {
    try {
        const saved = window.localStorage.getItem(storageKey);
        if (saved) return normalizeLocale(saved);
    } catch {
        return null;
    }
    return null;
}

let installationDefault = defaultLocale;
let userChoice: string | null = remembered();

export const i18n = $state({ locale: userChoice ?? installationDefault });

export function setInstallationDefault(tag: string): void {
    installationDefault = normalizeLocale(tag);
    if (userChoice === null) i18n.locale = installationDefault;
}

export function chooseLocale(tag: string): void {
    const next = normalizeLocale(tag);
    userChoice = next;
    i18n.locale = next;
    try {
        window.localStorage.setItem(storageKey, next);
    } catch {
        return;
    }
}

export function clearUserChoice(): void {
    userChoice = null;
    try {
        window.localStorage.removeItem(storageKey);
    } catch {
        return;
    }
    i18n.locale = installationDefault;
}

export function t(key: string, vars?: Record<string, string | number>): string {
    const table = catalogs[i18n.locale] ?? {};
    const fallback = catalogs[defaultLocale] ?? {};
    let text = table[key] ?? fallback[key] ?? key;
    if (vars) {
        for (const [name, value] of Object.entries(vars)) text = text.replaceAll(`{${name}}`, String(value));
    }
    return text;
}

export function missingKeys(tag: string): string[] {
    const locale = normalizeLocale(tag);
    if (locale === defaultLocale) return [];
    const table = catalogs[locale] ?? {};
    return Object.keys(catalogs[defaultLocale]).filter((key) => !(key in table));
}

export function localeDisplayName(tag: string): string {
    return t(`locale.${normalizeLocale(tag)}`);
}

export function formatDateTime(value: number | Date): string {
    return new Intl.DateTimeFormat(i18n.locale, {
        year: "numeric",
        month: "short",
        day: "numeric",
        hour: "numeric",
        minute: "2-digit",
        timeZoneName: "short",
    }).format(value);
}

export function formatDate(value: number | Date): string {
    return new Intl.DateTimeFormat(i18n.locale, { dateStyle: "medium" }).format(value);
}

export function formatNumber(value: number): string {
    return new Intl.NumberFormat(i18n.locale).format(value);
}

export function formatDuration(seconds: number): string {
    const whole = Math.max(0, Math.floor(seconds));
    const days = Math.floor(whole / 86400);
    const hours = Math.floor((whole % 86400) / 3600);
    const minutes = Math.floor((whole % 3600) / 60);
    const rest = whole % 60;
    const unit = (n: number, one: string, many: string) => `${formatNumber(n)} ${n === 1 ? one : many}`;
    if (days > 0) return `${unit(days, t("unit.day.one"), t("unit.day.many"))} ${unit(hours, t("unit.hour.one"), t("unit.hour.many"))}`;
    if (hours > 0)
        return `${unit(hours, t("unit.hour.one"), t("unit.hour.many"))} ${unit(minutes, t("unit.minute.one"), t("unit.minute.many"))}`;
    if (minutes > 0)
        return `${unit(minutes, t("unit.minute.one"), t("unit.minute.many"))} ${unit(rest, t("unit.second.one"), t("unit.second.many"))}`;
    return unit(rest, t("unit.second.one"), t("unit.second.many"));
}
