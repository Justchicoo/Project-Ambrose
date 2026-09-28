/*
 * Project Ambrose by Imjustchico
 * The files page against the real supervisor behind its own panel listener: a traversal, an encoded traversal, an absolute path and a device name asked of the real supervisor each answer 403 and name no host path, and the files page opened from a link to the logs root lists it, walks into a folder and back out by its breadcrumbs, and keeps the folder it shows in the address.
 */

import { mkdirSync, writeFileSync } from "node:fs";
import path from "node:path";
import { expect, test } from "@playwright/test";
import { app, built, operator, port, startPanelListener, supervisor, token, type PanelListener } from "./panel-server";

test.skip(!supervisor || !app || !built, "needs the built panel, patchserver and supervisor");
test.describe.configure({ mode: "serial" });

const AdminPort = port(60);
const PanelPort = port(61);
const AppPort = port(62);

let panel: PanelListener;

test.beforeAll(async () => {
    panel = await startPanelListener(AdminPort, PanelPort, AppPort);
    mkdirSync(path.join(panel.folder, "logs", "archive"), { recursive: true });
    writeFileSync(path.join(panel.folder, "logs", "archive", "older-run.log"), "an older run\n");
});

test.afterAll(async () => {
    await panel?.stop();
});

test("a traversal asked of the real panel answers 403 and names no host path", async () => {
    const folder = path.resolve(panel.folder);
    for (const asked of ["../../x", "%2e%2e%2f%2e%2e%2fsupervisor.conf", "/etc/passwd", "C:/Windows/win.ini", "CON", "nul.txt"]) {
        const answer = await fetch(`http://127.0.0.1:${AdminPort}/api/files/logs/content?path=${asked}`, {
            headers: { Authorization: `Bearer ${token}` },
            signal: AbortSignal.timeout(10000),
        });
        const body = await answer.text();
        expect(answer.status, `${asked}: ${body}`).toBe(403);
        const parsed = JSON.parse(body) as { error: string; message: string; root?: string };
        expect(parsed.root).toBe("logs");
        expect(parsed.message).not.toContain(folder);
        expect(body).not.toContain(JSON.stringify(folder).slice(1, -1));
        expect(body).not.toContain(folder.split(path.sep).join("/"));
    }
});

test("the files page lists the supervisor's logs root and opens a folder by its breadcrumbs", async ({ page }) => {
    await page.goto(`${panel.url}/#claim?token=${panel.claim}`);
    await page.locator("#sign-in-token").fill(panel.claim);
    await page.locator("#sign-in-username").fill(operator.name);
    await page.locator("#sign-in-password").fill(operator.password);
    await page.getByRole("button", { name: "Make me the owner" }).click();
    await expect(page.getByRole("heading", { name: "Overview", level: 1 })).toBeVisible();

    await page.goto(`${panel.url}/#files?root=logs`);
    await expect(page.getByRole("heading", { name: "Files", level: 1 })).toBeVisible();
    await expect(page.getByRole("button", { name: "Supervisor.log", exact: true })).toBeVisible();
    await page.getByRole("button", { name: "archive", exact: true }).click();
    await expect(page.getByRole("button", { name: "older-run.log", exact: true })).toBeVisible();
    await expect(page).toHaveURL(/#files\?root=logs&path=archive$/);

    await page.locator("[data-slot=breadcrumb-link]", { hasText: "logs" }).click();
    await expect(page.getByRole("button", { name: "Supervisor.log", exact: true })).toBeVisible();
    await expect(page.getByRole("button", { name: "older-run.log", exact: true })).toHaveCount(0);

    await page.getByRole("button", { name: "Supervisor.log", exact: true }).click();
    await expect(page.getByTestId("file-text")).toBeVisible();
});
