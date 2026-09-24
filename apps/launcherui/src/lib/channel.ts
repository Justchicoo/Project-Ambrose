/*
 * Project Ambrose by Imjustchico
 * The only way the window speaks to the launcher, and the shapes of what it hears back. A request carries the same values the console options carry and nothing else, because the launcher turns it into the request the console builds and the two must reach one plan. An answer either describes what would be run or names why nothing would be, and the page never decides which: it asks and shows. The channel is an interface rather than a host of its own so the page's logic can be driven by a test that answers as a launcher would. Where a real host is wanted it is built over the one @ambrose/ui already gives every surface, which knows the Windows web view, the one on every other desktop and a plain browser, so this page adds no second way of reaching a host and a browser opened without a launcher behind it says so instead of hanging.
 */

import type { Host } from "@ambrose/ui";

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
    install: string;
    revision: string;
    program: string;
    run_folder: string;
    log_file: string;
    host: string;
    port: number;
    locale: string;
    arguments: string[];
    command: string;
};

export type LauncherRefusal = {
    schema: number;
    ready: false;
    reason: string;
};

export type LauncherAnswer = LauncherPlan | LauncherRefusal;

export type SetupStep = {
    id: string;
    label: string;
    state: "waiting" | "doing" | "done" | "wrong";
    word: string;
};

export type LauncherChannel = {
    describe(request: LauncherRequest): Promise<LauncherAnswer>;
    start(request: LauncherRequest): Promise<LauncherAnswer>;
    steps?(): Promise<SetupStep[]>;
};

export function isPlan(answer: LauncherAnswer): answer is LauncherPlan {
    return answer.ready === true;
}

export function reasonOf(answer: LauncherAnswer): string {
    return isPlan(answer) ? "" : answer.reason;
}

export const NoLauncher = "this window was opened without a launcher behind it, so there is nothing to ask";

export function hostChannel(host: Host): LauncherChannel {
    async function ask(path: string, body: unknown): Promise<LauncherAnswer> {
        const reply = await host.call<LauncherAnswer>({ path, method: "POST", body });
        if (!reply.ok || reply.body === undefined || reply.body === null) {
            return { schema: 1, ready: false, reason: NoLauncher };
        }
        return reply.body;
    }

    return {
        describe: (request: LauncherRequest) => ask("/launcher/describe", request),
        start: (request: LauncherRequest) => ask("/launcher/start", request),
        steps: async (): Promise<SetupStep[]> => {
            const reply = await host.call<{ steps?: SetupStep[] }>({ path: "/launcher/steps", method: "GET" });
            return reply.ok && Array.isArray(reply.body?.steps) ? reply.body.steps : [];
        },
    };
}
