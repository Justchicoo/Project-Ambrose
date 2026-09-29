/*
 * Project Ambrose by Imjustchico
 * Tests the error reports page in a browser: source links use the reporting revision, clearing refreshes the persisted group, rendered text appears only after an explicit preview choice, and download creation sends the same selection that was previewed.
 */

import { flushSync, mount, unmount } from "svelte";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { session } from "$lib/api.svelte";
import type { InferOutput } from "valibot";
import { ErrorReportAnswer, ErrorsAnswer } from "$lib/schemas";
import Errors from "./Errors.svelte";

type ErrorGroup = InferOutput<typeof ErrorsAnswer>["groups"][number];
type Report = InferOutput<typeof ErrorReportAnswer>["report"];

const firstGroup: ErrorGroup = {
    id: 11,
    app: "gameserver",
    category: "server.database",
    level: "error",
    file: "src/server/database/Pool.cpp",
    line: 42,
    function: "Open",
    template: "could not reach {}",
    revision: "build-revision",
    count: 3,
    total_count: 5,
    first_epoch_ms: 1000,
    last_epoch_ms: 3000,
    last_message: "private rendered value",
    context_before: [
        {
            sequence: 76,
            time: "2026-09-27T00:00:00Z",
            epoch_ms: 2000,
            level: "warn",
            category: "server.database",
            message: "previous log line",
        },
    ],
    new_since_cleared: false,
};

const secondGroup: ErrorGroup = {
    ...firstGroup,
    id: 12,
    app: "loginserver",
    file: "src/server/login/Auth.cpp",
    line: 83,
    revision: "other-revision",
};

const report: Report = {
    format: "project-ambrose-error-report",
    schema: 1,
    product: {
        name: "Project Ambrose",
        version: "Project Ambrose rev build",
        commit: "build",
        branch: "milestone/17.106-error-reports",
    },
    operating_system: "Windows",
    apps: { gameserver: "build-revision" },
    groups: [
        {
            app: "gameserver",
            revision: "build-revision",
            level: "error",
            category: "server.database",
            source: { file: firstGroup.file, line: firstGroup.line, function: firstGroup.function },
            template: firstGroup.template,
            count: firstGroup.count,
            total_count: firstGroup.total_count,
            first_epoch_ms: firstGroup.first_epoch_ms,
            last_epoch_ms: firstGroup.last_epoch_ms,
            rendered_message: "rendered value",
            log_lines_before: firstGroup.context_before,
        },
    ],
};

const sent: { method: string; path: string; body: unknown }[] = [];
let groups: ErrorGroup[];
let page: ReturnType<typeof mount> | null = null;
let host: HTMLDivElement;

function answer(body: unknown): Response {
    return new Response(JSON.stringify(body), { status: 200, headers: { "Content-Type": "application/json" } });
}

beforeEach(() => {
    sent.length = 0;
    groups = [firstGroup, secondGroup];
    session.panel = true;
    session.via = "session";
    session.csrf = "csrf-token";
    session.user = {
        id: 1,
        username: "operator",
        display_name: "Operator",
        owner: true,
        role: "owner",
        permissions: ["errors.read", "errors.clear", "errors.report"],
        grants: {},
        must_change_password: false,
        two_factor: false,
        two_factor_required: false,
    };
    vi.stubGlobal("fetch", (path: string, options: RequestInit) => {
        const body = options.body ? JSON.parse(String(options.body)) : undefined;
        sent.push({ method: options.method ?? "GET", path, body });
        if (path === "api/panel/errors") return Promise.resolve(answer({ schema: 1, groups }));
        if (path === "api/panel/errors/clear") return Promise.resolve(answer({ schema: 1, cleared: true }));
        if (path === "api/panel/errors/report/preview" || path === "api/panel/errors/report")
            return Promise.resolve(answer({ schema: 1, report }));
        return Promise.resolve(answer({}));
    });
    vi.spyOn(URL, "createObjectURL").mockReturnValue("blob:report");
    vi.spyOn(URL, "revokeObjectURL").mockImplementation(() => {});
    vi.spyOn(HTMLAnchorElement.prototype, "click").mockImplementation(() => {});
    host = document.createElement("div");
    document.body.append(host);
    page = mount(Errors, { target: host });
    flushSync();
});

afterEach(() => {
    if (page) unmount(page);
    host.remove();
    vi.restoreAllMocks();
    vi.unstubAllGlobals();
});

describe("the error reports page", () => {
    it("shows source locations and clears a group through the audited route", async () => {
        await vi.waitFor(() => expect(host.textContent).toContain("could not reach {}"));
        const source = [...host.querySelectorAll("a")].find((link) => link.textContent?.includes("Pool.cpp:42"));
        expect(source?.getAttribute("href")).toBe(
            "https://github.com/Justchicoo/Project-Ambrose/blob/build-revision/src/server/database/Pool.cpp#L42",
        );
        expect(host.textContent).toContain("build-revision");
        expect(host.textContent).toContain("5 total");
        expect(host.textContent).toContain("3 this run");

        host.querySelector<HTMLButtonElement>('[aria-label="Clear gameserver error at src/server/database/Pool.cpp:42"]')?.click();
        await vi.waitFor(() => expect(sent.some((call) => call.path === "api/panel/errors/clear")).toBe(true));
        expect(sent.find((call) => call.path === "api/panel/errors/clear")?.body).toEqual({ id: 11 });
        await vi.waitFor(() => expect(sent.filter((call) => call.path === "api/panel/errors").length).toBeGreaterThan(1));
    });

    it("previews selected rendered contents before the report is created and downloaded", async () => {
        await vi.waitFor(() => expect(host.textContent).toContain("could not reach {}"));
        const select = host.querySelector<HTMLInputElement>('input[aria-label*="gameserver"]');
        expect(select).not.toBeNull();
        if (!select) return;
        select.click();
        const includeRendered = host.querySelector<HTMLInputElement>("#include-rendered");
        expect(includeRendered).not.toBeNull();
        includeRendered?.click();
        flushSync();
        const preview = [...host.querySelectorAll("button")].find((button) => button.textContent?.includes("Preview report"));
        preview?.click();
        await vi.waitFor(() => expect(sent.some((call) => call.path === "api/panel/errors/report/preview")).toBe(true));
        const previewCall = sent.find((call) => call.path === "api/panel/errors/report/preview");
        expect(previewCall?.body).toEqual({ groups: [11], include_rendered: true });
        await vi.waitFor(() => expect(host.querySelector('[aria-label="Exact report contents"]')).not.toBeNull());
        const reportContents = host.querySelector('[aria-label="Exact report contents"]')?.textContent ?? "";
        expect(reportContents).toContain("previous log line");
        expect(reportContents).toContain("rendered value");
        expect(host.textContent).toContain("1 log line(s) before latest occurrence");
        expect(sent.some((call) => call.path === "api/panel/errors/report")).toBe(false);

        const create = [...host.querySelectorAll("button")].find((button) => button.textContent?.includes("Create and download"));
        create?.click();
        await vi.waitFor(() => expect(sent.some((call) => call.path === "api/panel/errors/report")).toBe(true));
        expect(sent.find((call) => call.path === "api/panel/errors/report")?.body).toEqual({
            groups: [11],
            include_rendered: true,
        });
        await vi.waitFor(() => expect(host.textContent).toContain("The report was audited and downloaded. It has not been sent anywhere."));
        const download = vi.mocked(URL.createObjectURL).mock.calls[0]?.[0];
        expect(download).toBeInstanceOf(Blob);
        if (download instanceof Blob) expect(await download.text()).toBe(`${reportContents}\n`);
    });
});
