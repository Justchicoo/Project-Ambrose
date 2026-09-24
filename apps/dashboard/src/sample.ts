/*
 * Project Ambrose by Imjustchico
 * Made-up figures the pages show until 17.06 reads them from the admin API: three apps, a realm, players and their count over the last hour, day and week, accounts, activity, log lines and panel users, shaped like the answers the real routes give.
 */

export type AppState = "healthy" | "waiting" | "wrong" | "unknown";

export type SampleApp = {
    name: string;
    role: string;
    realm: string;
    address: string;
    port: number;
    state: AppState;
    word: string;
    uptime: string;
    revision: string;
    sessions: number | null;
    players: number | null;
    limit: number | null;
    tickAverage: number | null;
    tickMax: number | null;
    age: string;
    stale: boolean;
    badges: { tone: "healthy" | "waiting" | "wrong" | "unknown"; text: string }[];
    problems: { code: string; message: string; fix: string; fixLabel: string }[];
    ticks: number[];
};

export const apps: SampleApp[] = [
    {
        name: "loginserver",
        role: "login",
        realm: "",
        address: "127.0.0.1",
        port: 12000,
        state: "healthy",
        word: "Online",
        uptime: "2 d 4 h",
        revision: "573f769",
        sessions: 14,
        players: null,
        limit: null,
        tickAverage: null,
        tickMax: null,
        age: "1 s ago",
        stale: false,
        badges: [],
        problems: [],
        ticks: [],
    },
    {
        name: "gameserver",
        role: "game",
        realm: "Ambrose",
        address: "127.0.0.1",
        port: 12001,
        state: "waiting",
        word: "Needs attention",
        uptime: "6 h 12 min",
        revision: "573f769",
        sessions: 9,
        players: 9,
        limit: 500,
        tickAverage: 3.4,
        tickMax: 11.8,
        age: "2 s ago",
        stale: false,
        badges: [
            { tone: "waiting", text: "Pending SQL updates" },
            { tone: "wrong", text: "1 problem" },
        ],
        problems: [
            {
                code: "type_dump_stale",
                message: "The type dump was built from an older client revision.",
                fix: "client",
                fixLabel: "Open client data",
            },
        ],
        ticks: [3.1, 3.3, 3.0, 3.8, 4.2, 3.4, 3.2, 11.8, 3.5, 3.3, 3.1, 3.4],
    },
    {
        name: "patchserver",
        role: "patch",
        realm: "",
        address: "127.0.0.1",
        port: 12500,
        state: "unknown",
        word: "Offline",
        uptime: "",
        revision: "573f769",
        sessions: null,
        players: null,
        limit: null,
        tickAverage: null,
        tickMax: null,
        age: "4 min ago",
        stale: true,
        badges: [{ tone: "unknown", text: "Stopped" }],
        problems: [],
        ticks: [],
    },
];

export const players = [
    { name: "Ember Stormweaver", level: "34", school: "Fire", zone: "Wizard City: Ravenwood", session: "1 h 12 min", realm: "Ambrose" },
    { name: "Talon Frostblade", level: "12", school: "Ice", zone: "Wizard City: Unicorn Way", session: "22 min", realm: "Ambrose" },
    {
        name: "Nova Lifespring",
        level: "50",
        school: "Life",
        zone: "Krokotopia: Pyramid of the Sun",
        session: "3 h 4 min",
        realm: "Ambrose",
    },
    { name: "Quinn Deathwhisper", level: "7", school: "Death", zone: "Wizard City: Golem Court", session: "9 min", realm: "Ambrose" },
];

export const accounts = [
    { username: "ember", created: "2026-09-01", lastSeen: "now", level: "Player", state: "Active" },
    { username: "talon", created: "2026-09-10", lastSeen: "now", level: "Player", state: "Active" },
    { username: "moderator1", created: "2026-08-20", lastSeen: "yesterday", level: "Moderator", state: "Active" },
    { username: "spammer42", created: "2026-09-19", lastSeen: "3 days ago", level: "Player", state: "Banned until 2026-10-19" },
];

export const realms = [
    { name: "Ambrose", address: "127.0.0.1:12001", flags: "Recommended", population: "9 / 500", heartbeat: "2 s ago", zones: "14" },
    { name: "Ambrose Test", address: "127.0.0.1:12011", flags: "Test, offline", population: "0 / 50", heartbeat: "4 min ago", zones: "0" },
];

export const panelUsers = [
    { name: "operator", role: "Owner", twoFactor: "On", lastSignIn: "now" },
    { name: "helper", role: "Operator", twoFactor: "On", lastSignIn: "2 h ago" },
];

export const backups = [
    { name: "nightly-2026-09-22", taken: "2026-09-22 03:00", size: "182 MB", verified: "Verified", kept: "7 days" },
    { name: "nightly-2026-09-21", taken: "2026-09-21 03:00", size: "181 MB", verified: "Verified", kept: "7 days" },
    { name: "before-update-573f769", taken: "2026-09-21 18:40", size: "181 MB", verified: "Verified", kept: "Pinned" },
];

export const activity = [
    { when: "09:12", who: "operator", what: "Restarted gameserver", where: "gameserver" },
    { when: "09:05", who: "helper", what: "Kicked Quinn Deathwhisper: stuck in a zone", where: "Players" },
    { when: "08:55", who: "helper", what: "Banned spammer42 for 30 days: spam in chat", where: "Accounts" },
    { when: "08:40", who: "operator", what: "Changed Realm.PlayerLimit from 400 to 500", where: "Settings" },
    { when: "08:31", who: "operator", what: "Stopped patchserver", where: "patchserver" },
    { when: "03:00", who: "Scheduler", what: "Took nightly-2026-09-22 and verified it", where: "Backups" },
];

const lastHour = [7, 7, 8, 7, 8, 8, 9, 10, 9, 9, 8, 9, 9];
const lastDay = [6, 8, 11, 12, 14, 17, 20, 23, 26, 29, 31, 28, 22, 15, 10, 6, 4, 3, 2, 2, 3, 5, 7, 9];
const lastWeek = [14, 27, 9, 3, 13, 25, 8, 3, 15, 29, 10, 4, 16, 33, 12, 5, 19, 38, 15, 6, 18, 36, 13, 4, 14, 31, 10, 7];
const clock = (minutes: number) => `${String(Math.floor(minutes / 60) % 24).padStart(2, "0")}:${String(minutes % 60).padStart(2, "0")}`;
const now = Date.UTC(2026, 8, 22, 13, 12) / 1000;
const history = (values: number[], step: number, last: number) => ({
    times: values.map((_value, index) => last - (values.length - 1 - index) * step),
    values,
});

export const playerHistory = {
    hour: history(lastHour, 300, now),
    day: history(lastDay, 3600, now - 12 * 60),
    week: history(lastWeek, 6 * 3600, Date.UTC(2026, 8, 22, 10, 0) / 1000),
};

const lines = [
    ["info", "server", "gameserver started in 1.8 s"],
    ["info", "network", "Accepted 127.0.0.1:53122"],
    ["info", "login", "Account ember signed in"],
    ["debug", "zone", "Loaded WizardCity/WC_Ravenwood with 38 objects"],
    ["warn", "database", "Pending SQL update 2026_09_21_00_world.sql"],
    ["info", "commands.console", "Console: account set password (arguments hidden)"],
    ["trace", "network", "Sent MSG_KEEPALIVE to 127.0.0.1:53122"],
    ["info", "login", "Account talon signed in"],
    ["debug", "zone", "Ember Stormweaver entered WC_Ravenwood"],
    ["warn", "client", "The type dump was built from r801440, the install is r806919"],
] as const;
const burst = [
    ["error", "database", "Lost the connection to the character database, retrying in 2 s"],
    ["warn", "database", "A character query took 1840 ms"],
    ["warn", "network", "127.0.0.1:53188 timed out after 30 s"],
] as const;

export type LogRecord = { sequence: number; seconds: number; time: string; level: string; category: string; message: string };

export function logRecord(index: number): LogRecord {
    const [level, category, message] = index >= 150 && index < 168 ? burst[index % burst.length] : lines[index % lines.length];
    const seconds = 9 * 3600 + index * 3;
    const millis = String((index * 379) % 1000).padStart(3, "0");
    return {
        sequence: 1000 + index,
        seconds,
        time: `2026-09-22 ${clock(Math.floor(seconds / 60))}:${String(seconds % 60).padStart(2, "0")}.${millis}`,
        level,
        category,
        message,
    };
}

export const logRecords = Array.from({ length: 240 }, (_, index) => logRecord(index));
