/*
 * Project Ambrose by Imjustchico
 * The only way the window speaks to the launcher, and the shapes of what it hears back. A request carries the same values the console options carry and nothing else, because the launcher turns it into the request the console builds and the two must reach one plan. An answer either describes what would be run or names why nothing would be, and the page never decides which: it asks and shows. A plan carries the window size the launcher will start the client at, so the window shows the size that will be used rather than one it assumes. Every answer carries the launch state the launcher decided, locate, checking, setting up, play, launching, playing or retry, which is what the window's one primary button draws, and the first run's steps come back with that state and the login server's status, online, offline or unknown, from the launcher's own attempt to reach it. Looking again may carry the settings the operator changed, so a newly named installation is looked for from the first step. Opening the log folder names no folder: the launcher opens the one its own plan holds the client's log in and answers with it, or with why there is none yet. The profile is the shape the header shows, the server this run joins and a slot for the account name, written now so the server list a later milestone brings has a place to land, and filled only from what the launcher answered. The channel is an interface rather than a host of its own so the page's logic can be driven by a test that answers as a launcher would. Where a real host is wanted it is built over the one @ambrose/ui already gives every surface, which knows the Windows web view, the one on every other desktop and a plain browser, so this page adds no second way of reaching a host and a browser opened without a launcher behind it says so instead of hanging.
 */

import type { Host } from "@ambrose/ui";

export type LaunchState = "locate" | "checking" | "setting-up" | "play" | "launching" | "playing" | "retry";

export type ServerStatus = "online" | "offline" | "unknown";

export type LauncherRequest = {
    client_dir?: string;
    host?: string;
    port?: string;
    locale?: string;
    window?: string;
    fullscreen?: string;
    window_x?: string;
    window_y?: string;
    run_dir?: string;
    character?: string;
    user?: { user_id: string; key: string; name: string };
};

export type LauncherPlan = {
    schema: number;
    ready: true;
    state?: LaunchState;
    install: string;
    revision: string;
    program: string;
    run_folder: string;
    log_file: string;
    host: string;
    port: number;
    locale: string;
    window: string;
    arguments: string[];
    command: string;
};

export type LauncherRefusal = {
    schema: number;
    ready: false;
    state?: LaunchState;
    reason: string;
};

export type LauncherAnswer = LauncherPlan | LauncherRefusal;

export type SetupStep = {
    id: string;
    label: string;
    state: "waiting" | "doing" | "done" | "wrong";
    word: string;
};

export type ServerReach = {
    status: ServerStatus;
    address: string;
};

export type SetupAnswer = {
    state: LaunchState;
    server: ServerReach;
    steps: SetupStep[];
};

export type FolderAnswer = {
    opened: boolean;
    folder?: string;
    reason?: string;
};

export type ProfileServer = {
    id: string;
    name: string;
    host: string;
    port: number;
};

export type LauncherProfile = {
    server: ProfileServer | null;
    account: string | null;
};

export type LauncherChannel = {
    describe(request: LauncherRequest): Promise<LauncherAnswer>;
    start(request: LauncherRequest): Promise<LauncherAnswer>;
    steps?(): Promise<SetupAnswer>;
    again?(request: LauncherRequest): Promise<SetupAnswer>;
    openLogs?(): Promise<FolderAnswer>;
};

export const LaunchStates: readonly LaunchState[] = ["locate", "checking", "setting-up", "play", "launching", "playing", "retry"];

export function isPlan(answer: LauncherAnswer): answer is LauncherPlan {
    return answer.ready === true;
}

export function reasonOf(answer: LauncherAnswer): string {
    return isPlan(answer) ? "" : answer.reason;
}

export function stateOf(answer: LauncherAnswer): LaunchState {
    if (answer.state !== undefined && LaunchStates.includes(answer.state)) return answer.state;
    return isPlan(answer) ? "play" : "retry";
}

export function profileOf(plan: LauncherPlan | null, account: string): LauncherProfile {
    return {
        server: plan === null ? null : { id: `${plan.host}:${plan.port}`, name: plan.host, host: plan.host, port: plan.port },
        account: account === "" ? null : account,
    };
}

export const NoLauncher = "this window was opened without a launcher behind it, so there is nothing to ask";

const Unanswered: SetupAnswer = { state: "retry", server: { status: "unknown", address: "" }, steps: [] };

function readSetup(body: Partial<SetupAnswer> | null | undefined): SetupAnswer {
    if (body === null || body === undefined || !Array.isArray(body.steps)) return { ...Unanswered, steps: [] };
    const state = body.state !== undefined && LaunchStates.includes(body.state) ? body.state : "checking";
    const status = body.server?.status;
    return {
        state,
        server: {
            status: status === "online" || status === "offline" ? status : "unknown",
            address: typeof body.server?.address === "string" ? body.server.address : "",
        },
        steps: body.steps,
    };
}

export function hostChannel(host: Host): LauncherChannel {
    async function ask(path: string, body: unknown): Promise<LauncherAnswer> {
        const reply = await host.call<LauncherAnswer>({ path, method: "POST", body });
        if (!reply.ok || reply.body === undefined || reply.body === null) {
            return { schema: 1, ready: false, state: "retry", reason: NoLauncher };
        }
        return reply.body;
    }

    return {
        describe: (request: LauncherRequest) => ask("/launcher/describe", request),
        start: (request: LauncherRequest) => ask("/launcher/start", request),
        steps: async (): Promise<SetupAnswer> => {
            const reply = await host.call<Partial<SetupAnswer>>({ path: "/launcher/steps", method: "GET" });
            return reply.ok ? readSetup(reply.body) : { ...Unanswered, steps: [] };
        },
        again: async (request: LauncherRequest): Promise<SetupAnswer> => {
            const reply = await host.call<Partial<SetupAnswer>>({ path: "/launcher/steps/again", method: "POST", body: request });
            return reply.ok ? readSetup(reply.body) : { ...Unanswered, steps: [] };
        },
        openLogs: async (): Promise<FolderAnswer> => {
            const reply = await host.call<FolderAnswer>({ path: "/launcher/logs/open", method: "POST" });
            if (!reply.ok || reply.body === undefined || reply.body === null) return { opened: false, reason: NoLauncher };
            return reply.body;
        },
    };
}
