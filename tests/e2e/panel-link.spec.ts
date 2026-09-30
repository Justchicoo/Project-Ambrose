/*
 * Project Ambrose by Imjustchico
 * A one-time local link on a real supervisor's panel listener: supervisor --panel-link, run against the same config the supervisor started with, prints one line, and opening it makes the panel's owner and lands on the overview with nothing typed, after which the token is in neither the address, the browser's storage nor any file the run left in its folder, and opening it a second time says it has been used.
 */

import { spawnSync } from "node:child_process";
import { readdirSync, readFileSync, statSync } from "node:fs";
import path from "node:path";
import { expect, test, type Page } from "@playwright/test";
import { app, built, port, startPanelListener, supervisor, type PanelListener } from "./panel-server";

test.skip(!supervisor || !app || !built, "needs the built panel, patchserver and supervisor");
test.describe.configure({ mode: "serial" });

let panel: PanelListener;

function everyFile(folder: string): string[] {
    const found: string[] = [];
    for (const entry of readdirSync(folder)) {
        const full = path.join(folder, entry);
        if (statSync(full).isDirectory()) found.push(...everyFile(full));
        else found.push(full);
    }
    return found;
}

async function storage(page: Page): Promise<string> {
    return page.evaluate(() => {
        const kept: string[] = [];
        for (const place of [window.localStorage, window.sessionStorage])
            for (let index = 0; index < place.length; index += 1) {
                const key = place.key(index) ?? "";
                kept.push(key, place.getItem(key) ?? "");
            }
        return kept.join("\n");
    });
}

test.beforeAll(async () => {
    panel = await startPanelListener(port(70), port(71), port(72));
});

test.afterAll(async () => {
    await panel?.stop();
});

test("a local link from supervisor --panel-link opens the overview with nothing typed and leaves its token nowhere", async ({ page }) => {
    const printed = spawnSync(supervisor ?? "", ["-c", path.join(panel.folder, "supervisor.conf"), "--panel-link"], {
        cwd: panel.folder,
        encoding: "utf8",
        timeout: 60000,
    });
    expect(printed.status, printed.stderr).toBe(0);
    const lines = printed.stdout.split(/\r?\n/).filter((line) => line !== "");
    expect(lines).toHaveLength(1);
    const link = lines[0];
    expect(link.startsWith(`${panel.url}/#link?token=`)).toBe(true);
    const token = new URLSearchParams(link.slice(link.indexOf("?") + 1)).get("token") ?? "";
    expect(token).toHaveLength(43);

    await page.goto(link);
    await expect(page.getByRole("heading", { name: "Overview", level: 1 })).toBeVisible();
    expect(page.url()).not.toContain(token);
    expect(await storage(page)).not.toContain(token);

    for (const file of everyFile(panel.folder)) expect(readFileSync(file).includes(token), `${file} holds the token`).toBe(false);

    await page.context().clearCookies();
    await page.goto("about:blank");
    await page.goto(link);
    await expect(page.getByRole("alert")).toContainText("That link has been used or has run out. Ask for another.");
});
