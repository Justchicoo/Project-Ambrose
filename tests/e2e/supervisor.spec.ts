/*
 * Project Ambrose by Imjustchico
 * The panel end to end against a real supervisor: the overview carries a card for the app the supervisor runs, the servers page lists it with its process and the output the supervisor captured, the restart button asks first and brings the app back as another process without counting a crash, and a stop from the page leaves it offline with a start button in its place.
 */

import { expect, test, type Page } from "@playwright/test";
import { app, built, startSupervisor, supervisor, token, type Panel } from "./panel-server";

test.skip(!app || !supervisor || !built, "needs the built panel, patchserver and supervisor");
test.describe.configure({ mode: "serial" });

let panel: Panel;

test.beforeAll(async () => {
    panel = await startSupervisor(12620, 12621);
});

test.afterAll(async () => {
    await panel?.stop();
});

async function signIn(page: Page) {
    await page.goto(`${panel.url}/#servers`);
    await page.getByLabel("Admin token").fill(token);
    await page.getByRole("button", { name: "Sign in" }).click();
    await expect(page.getByRole("heading", { name: "Servers", level: 1 })).toBeVisible();
}

async function processOf(page: Page): Promise<string> {
    const row = page.getByRole("row").filter({ hasText: "patchserver" });
    await expect(row.getByText("Running")).toBeVisible({ timeout: 20000 });
    return (await row.locator("td").nth(4).innerText()).trim();
}

test("the overview carries a card for every app the supervisor runs", async ({ page }) => {
    await page.goto(`${panel.url}/#overview`);
    await page.getByLabel("Admin token").fill(token);
    await page.getByRole("button", { name: "Sign in" }).click();
    await expect(page.getByRole("heading", { name: "Overview", level: 1 })).toBeVisible();
    await expect(page.getByText("Apps running")).toBeVisible({ timeout: 10000 });
    await expect(page.getByText("1 of 1")).toBeVisible();
    await expect(page.getByRole("link", { name: "Open servers" })).toBeVisible();
});

test("the servers page lists the app with its process and the output the supervisor captured", async ({ page }) => {
    await signIn(page);
    const process = await processOf(page);
    expect(Number.parseInt(process, 10)).toBeGreaterThan(0);
    await expect(page.getByText("Output of patchserver")).toBeVisible();
    await expect(page.getByText("patchserver ready").first()).toBeVisible({ timeout: 20000 });
    await expect(page.getByText("is ready: it printed its ready line")).toBeVisible();
});

test("the restart button asks first and brings the app back as another process", async ({ page }) => {
    await signIn(page);
    const before = await processOf(page);
    await page.getByRole("button", { name: "Restart" }).first().click();
    await expect(page.getByRole("heading", { name: "Restart patchserver?" })).toBeVisible();
    await page.getByRole("button", { name: "Restart it" }).click();
    await expect
        .poll(async () => (await page.getByRole("row").filter({ hasText: "patchserver" }).locator("td").nth(4).innerText()).trim(), {
            timeout: 30000,
        })
        .not.toBe(before);
    const after = await processOf(page);
    expect(Number.parseInt(after, 10)).toBeGreaterThan(0);
    await expect(page.getByRole("row").filter({ hasText: "patchserver" }).locator("td").nth(5)).toHaveText("0");
});

test("a stop from the page leaves the app offline with a start button in its place", async ({ page }) => {
    await signIn(page);
    await processOf(page);
    await page.getByRole("button", { name: "Stop" }).first().click();
    await expect(page.getByRole("heading", { name: "Stop patchserver?" })).toBeVisible();
    await page.getByRole("button", { name: "Stop it" }).click();
    await expect(page.getByRole("row").filter({ hasText: "patchserver" }).getByText("Offline")).toBeVisible({ timeout: 30000 });
    await expect(page.getByRole("button", { name: "Start" })).toBeVisible();
    await page.getByRole("button", { name: "Start" }).click();
    await processOf(page);
});
