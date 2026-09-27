/*
 * Project Ambrose by Imjustchico
 * The built panel loaded from the panel's own listener rather than an app's admin API: its first operator is made from the link the supervisor printed and reaches the overview with nothing written to the browser console, every request it makes goes back to the listener it came from, every response carries the policy, frame denial, nosniff and referrer headers, and the servers page reaches the app the supervisor runs through that listener rather than the app list of the supervisor alone.
 */

import { expect, test } from "@playwright/test";
import { app, built, operator, port, startPanelListener, supervisor, type PanelListener } from "./panel-server";

test.skip(!supervisor || !app || !built, "needs the built panel, patchserver and supervisor");
test.describe.configure({ mode: "serial" });

let panel: PanelListener;

test.beforeAll(async () => {
    panel = await startPanelListener(port(30), port(31), port(32));
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

    await page.goto(`${panel.url}/#claim?token=${panel.claim}`);
    await page.locator("#sign-in-token").fill(panel.claim);
    await page.locator("#sign-in-username").fill(operator.name);
    await page.locator("#sign-in-password").fill(operator.password);
    await page.getByRole("button", { name: "Make me the owner" }).click();
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
    await page.locator("#sign-in-username").fill(operator.name);
    await page.locator("#sign-in-password").fill(operator.password);
    await page.getByRole("button", { name: "Sign in" }).click();
    await expect(page.getByRole("heading", { name: "Servers", level: 1 })).toBeVisible();

    const row = page.getByRole("row").filter({ hasText: "patchserver" });
    await expect(row.getByText("Running")).toBeVisible({ timeout: 20000 });
});
