/*
 * Project Ambrose by Imjustchico
 * The built panel loaded from the panel's own listener rather than an app's admin API: it signs in and reaches the overview with nothing written to the browser console, every request it makes goes back to the listener it came from, every response carries the policy, frame denial, nosniff and referrer headers, and the servers page reaches the app the supervisor runs through that listener rather than the app list of the supervisor alone.
 */

import { expect, test } from "@playwright/test";
import { app, built, startPanelListener, supervisor, token, type Panel } from "./panel-server";

test.skip(!supervisor || !app || !built, "needs the built panel, patchserver and supervisor");
test.describe.configure({ mode: "serial" });

let panel: Panel;

test.beforeAll(async () => {
    panel = await startPanelListener(12630, 12631, 12632);
});

test.afterAll(async () => {
    await panel?.stop();
});

test("the panel loads from its own listener with no console errors and no request to another host", async ({ page }) => {
    const complaints: string[] = [];
    const elsewhere: string[] = [];
    page.on("console", (message) => {
        if (message.type() === "error" || message.type() === "warning") complaints.push(`${message.type()}: ${message.text()}`);
    });
    page.on("pageerror", (failure) => complaints.push(`pageerror: ${failure.message}`));
    page.on("request", (request) => {
        if (!request.url().startsWith(panel.url) && !request.url().startsWith("data:")) elsewhere.push(request.url());
    });
    page.on("response", (answer) => {
        if (answer.status() >= 400) complaints.push(`${answer.status()} ${answer.url()}`);
    });

    await page.goto(`${panel.url}/#overview`);
    await page.getByLabel("Admin token").fill(token);
    await page.getByRole("button", { name: "Sign in" }).click();
    await expect(page.getByRole("heading", { name: "Overview", level: 1 })).toBeVisible();

    expect(complaints).toEqual([]);
    expect(elsewhere).toEqual([]);
});

test("every response from the panel listener carries its security headers", async () => {
    const answer = await fetch(`${panel.url}/`);
    expect(answer.status).toBe(200);
    expect(answer.headers.get("content-security-policy")).toContain("frame-ancestors 'none'");
    expect(answer.headers.get("x-content-type-options")).toBe("nosniff");
    expect(answer.headers.get("x-frame-options")).toBe("DENY");
    expect(answer.headers.get("referrer-policy")).toBe("same-origin");
    expect(answer.headers.get("strict-transport-security")).toBeNull();

    const refused = await fetch(`${panel.url}/api/health`);
    expect(refused.status).toBe(401);
    expect(refused.headers.get("content-security-policy")).toContain("frame-ancestors 'none'");
    expect(refused.headers.get("x-content-type-options")).toBe("nosniff");
});

test("the servers page reaches the app the supervisor runs through the panel listener", async ({ page }) => {
    await page.goto(`${panel.url}/#servers`);
    await page.getByLabel("Admin token").fill(token);
    await page.getByRole("button", { name: "Sign in" }).click();
    await expect(page.getByRole("heading", { name: "Servers", level: 1 })).toBeVisible();

    const row = page.getByRole("row").filter({ hasText: "patchserver" });
    await expect(row.getByText("Running")).toBeVisible({ timeout: 20000 });
});
