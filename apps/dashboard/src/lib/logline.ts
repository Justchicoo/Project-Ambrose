/*
 * Project Ambrose by Imjustchico
 * Reads one captured line into the parts doc/DESIGN.md colours: the time, the level word, the category and the message, and inside the message the runs that carry a value rather than a word, sorted into the kinds the value ramp names. An address is anything that says where, which is a URL, a host and port, a path and a connection target; a setting is a configuration option, told from an identifier by its capitals; a name is an app, a database, a category, a version or a digest; a number is a quantity carrying a unit; and quoted text is itself, unless what is quoted is a setting. Ordinary words are left alone, a hyphenated word such as AI-built is a word and not an identifier, a bare count with no unit is never a value, and no line marks more than eight runs, both rules straight from the design, because a line holding twenty numbers is a rainbow.
 */

export type ValueKind = "number" | "text" | "name" | "address" | "setting" | "plain";

export type LinePart = { text: string; kind: ValueKind };

export type LogRecord = {
    time: string;
    level: string;
    category: string;
    message: string;
    parts: LinePart[];
};

const RECORD = /^(?:\d{4}-\d{2}-\d{2}[_ ])?(\d{2}:\d{2}:\d{2}(?:\.\d{1,3})?)\s+([A-Z]+)\s+\[([^\]]+)\]\s?(.*)$/;

const UNITS = "ms|us|ns|s|min|h|d|MiB|GiB|KiB|TiB|MB|GB|KB|B|%";

const SETTING = /^[A-Z][A-Za-z0-9]*(?:\.[A-Za-z0-9]+)+$|^[A-Z][a-z0-9]+(?:[A-Z][a-z0-9]*)+$/;

const RUNS: { kind: ValueKind; pattern: RegExp }[] = [
    { kind: "text", pattern: /"[^"]*"/y },
    { kind: "address", pattern: /[A-Za-z][A-Za-z0-9+.-]*:\/\/[^\s,;]+/y },
    { kind: "address", pattern: /[A-Za-z0-9_.+-]+@[^\s,;]+/y },
    { kind: "address", pattern: /(?:[A-Za-z]:[\\/]|\/)[^\s,;]+/y },
    { kind: "address", pattern: /\d{1,3}(?:\.\d{1,3}){3}(?::\d+)?/y },
    { kind: "address", pattern: /\[[0-9a-fA-F:]+\](?::\d+)?/y },
    { kind: "number", pattern: new RegExp(String.raw`\d[\d.,]*\s?(?:${UNITS})(?![A-Za-z0-9])`, "y") },
    { kind: "name", pattern: /\d+(?:\.\d+){2,}[A-Za-z0-9.-]*/y },
    { kind: "name", pattern: /[0-9a-f]{7,}(?![A-Za-z0-9])/y },
    { kind: "setting", pattern: /[A-Z][A-Za-z0-9]*(?:\.[A-Za-z0-9]+)+(?![A-Za-z0-9._])/y },
    { kind: "name", pattern: /[A-Za-z][A-Za-z0-9]*(?:[._][A-Za-z0-9]+)+/y },
];

const MaxRuns = 8;

function startsARun(text: string, at: number): boolean {
    const before = at === 0 ? "" : text[at - 1];
    return before === "" || !/[A-Za-z0-9_]/.test(before);
}

function quotedKind(run: string): ValueKind {
    return SETTING.test(run.slice(1, -1)) ? "setting" : "text";
}

export function readValues(message: string): LinePart[] {
    const parts: LinePart[] = [];
    let plain = "";
    let at = 0;
    let marked = 0;

    const keepPlain = () => {
        if (plain !== "") {
            parts.push({ text: plain, kind: "plain" });
            plain = "";
        }
    };

    while (at < message.length) {
        let taken: LinePart | null = null;
        if (marked < MaxRuns && startsARun(message, at)) {
            for (const run of RUNS) {
                run.pattern.lastIndex = at;
                const match = run.pattern.exec(message);
                if (match && match[0].length > 0) {
                    taken = { text: match[0], kind: run.kind === "text" ? quotedKind(match[0]) : run.kind };
                    break;
                }
            }
        }
        if (taken) {
            keepPlain();
            parts.push(taken);
            at += taken.text.length;
            marked += 1;
        } else {
            plain += message[at];
            at += 1;
        }
    }
    keepPlain();
    return parts;
}

export function readLine(text: string): LogRecord | null {
    const found = RECORD.exec(text);
    if (!found) return null;
    return { time: found[1], level: found[2], category: found[3], message: found[4], parts: readValues(found[4]) };
}
