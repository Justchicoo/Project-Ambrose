/*
 * Project Ambrose by Imjustchico
 * The panel end to end against a real app's admin API: a signed-out browser gets the sign-in page and a wrong token is refused with its request id, signing in shows the app online within two seconds, the load raises no console error and asks no other host, the page is drawn with the Ambrose tokens and starts with a skip link, it works at 400 pixels with no sideways scrolling, nothing in the browser holds the token or a server list, and stopping the app reads as not answering within five seconds with its figures stale and their age named.
 */

import { expect, test, type Page } from "@playwright/test";
import { app, built, startPanel, token, type Panel } from "./panel-server";

test.skip(!app || !built, "needs the built panel and a built patchserver, or AMBROSE_PANEL_APP");
test.describe.configure({ mode: "serial" });

let panel: Panel;

test.beforeAll(async () => {
    panel = await startPanel(12610);
});

test.afterAll(async () => {
    await panel?.stop();
});

async function signIn(page: Page, url: string) {
    await page.goto(`${url}/#overview`);
    await page.getByLabel("Admin token").fill(token);
    const started = Date.now();
    await page.getByRole("button", { name: "Sign in" }).click();
    await expect(page.getByRole("heading", { name: "Overview", level: 1 })).toBeVisible();
    await expect(page.getByText("Running", { exact: true }).first()).toBeVisible({ timeout: 2000 });
    return Date.now() - started;
}

test("a signed-out browser gets the sign-in page and a wrong token is refused with its request id", async ({ page }) => {
    await page.goto(`${panel.url}/`);
    await expect(page.getByRole("heading", { name: "Sign in to Ambrose" })).toBeVisible();
    await page.getByLabel("Admin token").fill("wrong-token-wrong-token-wrong-to");
    await page.getByRole("button", { name: "Sign in" }).click();
    const alert = page.getByRole("alert");
    await expect(alert).toContainText("That is not this app's admin token.");
    await expect(alert).toContainText(/Request [A-Za-z0-9_-]{16}/);
});

test("signing in shows the app online within two seconds", async ({ page }) => {
    expect(await signIn(page, panel.url)).toBeLessThan(2000);
});

test("the load raises no console error and asks no other host for anything", async ({ page }) => {
    const errors: string[] = [];
    const elsewhere: string[] = [];
    page.on("console", (message) => {
        if (message.type() === "error") errors.push(message.text());
    });
    page.on("request", (request) => {
        if (!request.url().startsWith(panel.url)) elsewhere.push(request.url());
    });
    await page.goto(`${panel.url}/`);
    await expect(page.getByRole("heading", { name: "Sign in to Ambrose" })).toBeVisible();
    await signIn(page, panel.url);
    await page.waitForTimeout(1500);
    expect(errors, "the panel logged an error").toEqual([]);
    expect(elsewhere, "the panel reached another host").toEqual([]);
});

test("the page is drawn with the Ambrose tokens and starts with a skip link", async ({ page }) => {
    await signIn(page, panel.url);
    const ground = await page.evaluate(() =>
        getComputedStyle(document.documentElement).getPropertyValue("--ambrose-color-surface-page").trim(),
    );
    expect(ground).toMatch(/^#[0-9A-Fa-f]{6}$/);
    expect(await page.evaluate(() => getComputedStyle(document.body).fontFamily)).toContain("Karla");
    await page.keyboard.press("Tab");
    await expect(page.getByRole("link", { name: "Skip to content" })).toBeFocused();
    await page.keyboard.press("Enter");
    await expect(page.locator("#content")).toBeFocused();
    expect(page.url()).toContain("#overview");
});

test("the overview works at 400 pixels wide with no sideways scrolling", async ({ page }) => {
    await page.setViewportSize({ width: 400, height: 860 });
    await signIn(page, panel.url);
    expect(await page.evaluate(() => document.documentElement.scrollWidth - window.innerWidth)).toBeLessThanOrEqual(0);
});

test("nothing in the browser holds the token or a server list", async ({ page }) => {
    await signIn(page, panel.url);
    const kept = await page.evaluate(() => {
        const values = (store: Storage) => Object.keys(store).map((key) => `${key}=${store.getItem(key)}`);
        return { local: values(localStorage), session: values(sessionStorage), cookie: document.cookie };
    });
    const everything = [...kept.local, ...kept.session, kept.cookie].join("\n");
    expect(everything).not.toContain(token);
    expect(everything).not.toContain("patchserver");
    expect(kept.cookie).toBe("");
});

test("a stopped app reads as not answering within five seconds and its figures go stale with their age", async ({ page }) => {
    const doomed = await startPanel(12611);
    try {
        await signIn(page, doomed.url);
        await expect(page.getByText(/^Updated \d+ s ago$/).first()).toBeVisible();
        const stopped = Date.now();
        await doomed.stop();
        await expect(page.getByText("Not answering").first()).toBeVisible({ timeout: 5000 });
        expect(Date.now() - stopped).toBeLessThan(5000);
        await expect(page.getByText(/^Last sample \d+ s ago$/).first()).toBeVisible({ timeout: 5000 });
        expect(Date.now() - stopped).toBeLessThan(3000 + 1000 + 500);
        await expect(page.getByText(/^Updated \d+ s ago$/)).toHaveCount(0);
        await expect(page.getByRole("status").first()).toContainText("The server stopped answering");
    } finally {
        await doomed.stop();
    }
});
