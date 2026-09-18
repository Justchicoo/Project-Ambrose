/*
 * Project Ambrose by Imjustchico
 * The end-to-end scaffolding the surfaces build on: the built panel loads from its own origin, reads the tokens, keeps its skip link reachable and asks no other host for anything.
 */

import { expect, test } from "@playwright/test";

test("the built panel serves itself and asks no other host for anything", async ({ page }) => {
    const elsewhere: string[] = [];
    page.on("request", (request) => {
        const url = new URL(request.url());
        if (url.hostname !== "127.0.0.1" && url.hostname !== "localhost") {
            elsewhere.push(request.url());
        }
    });

    await page.goto("/");
    await expect(page.getByRole("heading", { name: "Overview", level: 1 })).toBeVisible();
    expect(elsewhere, "the panel reached another host").toEqual([]);
});

test("the page is drawn with the Ambrose tokens rather than a browser default", async ({ page }) => {
    await page.goto("/");
    const ground = await page.evaluate(() =>
        getComputedStyle(document.documentElement).getPropertyValue("--ambrose-color-surface-page").trim(),
    );
    expect(ground.toUpperCase()).toBe("#0B1020");
    const font = await page.evaluate(() => getComputedStyle(document.body).fontFamily);
    expect(font).toContain("Karla");
});

test("the first thing the keyboard reaches is the skip link", async ({ page }) => {
    await page.goto("/");
    await page.keyboard.press("Tab");
    await expect(page.getByRole("link", { name: "Skip to content" })).toBeFocused();
});
