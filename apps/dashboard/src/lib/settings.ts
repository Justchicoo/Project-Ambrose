/*
 * Project Ambrose by Imjustchico
 * What the config page knows about a live setting before it asks the server: whether a value typed for it is of its type and within its bounds, the form the server stores it in, trimmed of only the spaces the server trims and a number written the way the server writes one, which settings a search and a category show, a reason clipped to the bytes the server keeps, and settings presets, a versioned JSON file of chosen keys and values that never carries a secret, read back with every entry checked against the app's own schema so the diff shown before an import names each problem; the server checks everything again, so this only keeps a value it would refuse from being sent.
 */

import type { SettingsAnswer } from "./schemas";

export type Setting = SettingsAnswer["settings"][number];
export type SettingType = "bool" | "integer" | "unsigned" | "float" | "string";

export const PresetFormat = "ambrose-settings-preset";
export const PresetVersion = 1;
const MaxTextBytes = 65535;
const MaxUnsigned = 18446744073709551615n;
const MinInteger = -9223372036854775808n;
const MaxInteger = 9223372036854775807n;

export type Preset = {
    format: string;
    version: number;
    app: string;
    created: string;
    settings: Record<string, string>;
};

export type PresetRow = {
    key: string;
    current: string | null;
    next: string;
    status: "change" | "same" | "refused";
    problem: string | null;
};

const truths = ["1", "true", "yes"];
const falsehoods = ["0", "false", "no"];

export function isLive(setting: Setting): boolean {
    return setting.declared === true && setting.type !== undefined;
}

export function isLocked(setting: Setting): boolean {
    return setting.lock !== null && setting.lock !== undefined;
}

function unitOf(setting: Setting): string {
    return setting.unit ? ` ${setting.unit}` : "";
}

function boundsPhrase(setting: Setting): string {
    return setting.bounds && setting.bounds !== "" ? setting.bounds : "";
}

function wholeNumber(text: string, signed: boolean): bigint | null {
    if (!(signed ? /^[+-]?\d+$/ : /^\+?\d+$/).test(text)) return null;
    try {
        return BigInt(text);
    } catch {
        return null;
    }
}

export function textBytes(text: string): number {
    return new TextEncoder().encode(text).length;
}

export function clipBytes(text: string, most: number): string {
    let clipped = "";
    let bytes = 0;
    for (const point of text) {
        const size = textBytes(point);
        if (bytes + size > most) break;
        clipped += point;
        bytes += size;
    }
    return clipped;
}

function trimLikeServer(text: string): string {
    return text.replace(/^[ \t\n\r\f\v]+|[ \t\n\r\f\v]+$/g, "");
}

function formatNumber(number: number): string {
    if (Object.is(number, -0)) return "-0";
    const [mantissa, power] = number.toExponential().split("e");
    const exponent = Number(power);
    if (exponent >= -4 && exponent < 16) return String(number);
    return `${mantissa}e${exponent < 0 ? "-" : "+"}${String(Math.abs(exponent)).padStart(2, "0")}`;
}

export function normalise(setting: Setting, text: string): string {
    const trimmed = trimLikeServer(text);
    if (setting.type === "bool") {
        const lowered = trimmed.toLowerCase();
        if (truths.includes(lowered)) return "true";
        if (falsehoods.includes(lowered)) return "false";
        return trimmed;
    }
    if (setting.type === "integer" || setting.type === "unsigned") {
        const whole = wholeNumber(trimmed, setting.type === "integer");
        return whole === null ? trimmed : whole.toString();
    }
    if (setting.type === "float") {
        const number = Number(trimmed);
        return trimmed !== "" && Number.isFinite(number) ? formatNumber(number) : trimmed;
    }
    return trimmed;
}

export function checkValue(setting: Setting, text: string): string | null {
    const trimmed = trimLikeServer(text);
    const bounds = boundsPhrase(setting);
    switch (setting.type as SettingType | undefined) {
        case "bool":
            return truths.includes(trimmed.toLowerCase()) || falsehoods.includes(trimmed.toLowerCase())
                ? null
                : `${setting.key} takes true or false`;
        case "integer":
        case "unsigned": {
            const signed = setting.type === "integer";
            const whole = wholeNumber(trimmed, signed);
            if (whole === null || (signed ? whole < MinInteger || whole > MaxInteger : whole > MaxUnsigned))
                return `${setting.key} takes ${signed ? "a whole number" : "a whole number of zero or more"}${bounds ? ` ${bounds}` : ""}`;
            if ((setting.min && whole < BigInt(setting.min)) || (setting.max && whole > BigInt(setting.max)))
                return `${setting.key} must be ${bounds}; ${whole.toString()}${unitOf(setting)} is outside that`;
            return null;
        }
        case "float": {
            const number = Number(trimmed);
            if (trimmed === "" || !Number.isFinite(number) || !/^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$/.test(trimmed))
                return `${setting.key} takes a number${bounds ? ` ${bounds}` : ""}`;
            if ((setting.min && number < Number(setting.min)) || (setting.max && number > Number(setting.max)))
                return `${setting.key} must be ${bounds}; ${trimmed}${unitOf(setting)} is outside that`;
            return null;
        }
        case "string": {
            const most = setting.max ? Number(setting.max) : MaxTextBytes;
            const bytes = textBytes(trimmed);
            return bytes > most ? `${setting.key} must be at most ${most} bytes; the value given has ${bytes}` : null;
        }
        default:
            return `${setting.key} is not a live setting, so it can only be changed in its config file`;
    }
}

export function matches(setting: Setting, search: string): boolean {
    const text = search.trim().toLowerCase();
    if (text === "") return true;
    return (
        setting.key.toLowerCase().includes(text) ||
        (!setting.secret && setting.value.toLowerCase().includes(text)) ||
        (setting.description ?? "").toLowerCase().includes(text) ||
        (setting.category ?? "").toLowerCase().includes(text)
    );
}

export function categories(settings: Setting[], search: string): { name: string; settings: Setting[] }[] {
    const groups: { name: string; settings: Setting[] }[] = [];
    for (const setting of settings) {
        if (!isLive(setting) || !matches(setting, search)) continue;
        const name = setting.category && setting.category !== "" ? setting.category : "Other";
        const held = groups.find((group) => group.name === name);
        if (held) held.settings.push(setting);
        else groups.push({ name, settings: [setting] });
    }
    groups.sort((left, right) => left.name.localeCompare(right.name));
    return groups;
}

export function makePreset(app: string, settings: Setting[], keys: string[], created: Date): Preset {
    const chosen: Record<string, string> = {};
    for (const key of [...keys].sort()) {
        const setting = settings.find((entry) => entry.key === key);
        if (!setting || !isLive(setting) || setting.secret) continue;
        chosen[key] = setting.value;
    }
    return { format: PresetFormat, version: PresetVersion, app, created: created.toISOString(), settings: chosen };
}

export function readPreset(text: string): { preset: Preset } | { error: string } {
    let parsed: unknown;
    try {
        parsed = JSON.parse(text);
    } catch {
        return { error: "The file is not JSON, so it is not a settings preset" };
    }
    if (typeof parsed !== "object" || parsed === null || Array.isArray(parsed)) return { error: "A settings preset is a JSON object" };
    const record = parsed as Record<string, unknown>;
    if (record.format !== PresetFormat) return { error: `The file is not a settings preset: its format is not ${PresetFormat}` };
    if (typeof record.version !== "number" || !Number.isInteger(record.version) || record.version < 1)
        return { error: "The preset names no version this panel can read" };
    if (record.version > PresetVersion)
        return { error: `The preset is version ${record.version}, made by a newer panel; this one reads up to version ${PresetVersion}` };
    if (typeof record.settings !== "object" || record.settings === null || Array.isArray(record.settings))
        return { error: "The preset holds no settings object" };
    const settings: Record<string, string> = {};
    for (const [key, value] of Object.entries(record.settings as Record<string, unknown>)) {
        if (typeof value === "string") settings[key] = value;
        else if (typeof value === "number" || typeof value === "boolean") settings[key] = String(value);
        else return { error: `The preset gives ${key} a value that is neither text, a number nor true or false` };
    }
    return {
        preset: {
            format: PresetFormat,
            version: record.version,
            app: typeof record.app === "string" ? record.app : "",
            created: typeof record.created === "string" ? record.created : "",
            settings,
        },
    };
}

export function diffPreset(preset: Preset, settings: Setting[]): PresetRow[] {
    const rows: PresetRow[] = [];
    for (const [key, next] of Object.entries(preset.settings)) {
        const setting = settings.find((entry) => entry.key === key);
        const refuse = (problem: string): PresetRow => ({ key, current: setting ? setting.value : null, next, status: "refused", problem });
        if (!setting || !isLive(setting)) {
            rows.push(refuse(`This app has no live setting named ${key}`));
            continue;
        }
        if (setting.secret) {
            rows.push(refuse(`${key} is a secret, and a preset never carries one`));
            continue;
        }
        if (isLocked(setting)) {
            const held = checkValue(setting, next) === null && normalise(setting, next) === setting.value;
            if (held) rows.push({ key, current: setting.value, next: setting.value, status: "same", problem: null });
            else
                rows.push(
                    refuse(
                        `${key} is set by ${setting.lock?.origin ?? setting.origin ?? "a layer above live settings"}, so it cannot be changed live`,
                    ),
                );
            continue;
        }
        const problem = checkValue(setting, next);
        if (problem) {
            rows.push(refuse(problem));
            continue;
        }
        const normalised = normalise(setting, next);
        rows.push({
            key,
            current: setting.value,
            next: normalised,
            status: normalised === setting.value ? "same" : "change",
            problem: null,
        });
    }
    rows.sort((left, right) => left.key.localeCompare(right.key));
    return rows;
}

export function applyRefusals(rows: PresetRow[], problems: { key: string; message: string }[]): PresetRow[] {
    return rows.map((row) => {
        const refused = problems.find((problem) => problem.key === row.key);
        return refused ? { ...row, status: "refused", problem: refused.message } : row;
    });
}

export function layerName(layer: string): string {
    const names: Record<string, string> = {
        declared: "Declared default",
        default: "Shipped default",
        module_default: "Module default",
        config: "This app's .conf",
        module_config: "A conf.d file",
        live: "Set live",
        environment: "Environment variable",
        override: "Command line",
    };
    return names[layer] ?? layer;
}

export function applyPhrase(setting: Setting): string {
    switch (setting.apply) {
        case "live":
            return "Takes hold at once";
        case "next_use":
            return "Takes hold from the next connection or operation that reads it";
        case "restart":
            return `Takes hold after a restart, because ${setting.restart_reason ?? "the app reads it only as it starts"}`;
        default:
            return "";
    }
}
