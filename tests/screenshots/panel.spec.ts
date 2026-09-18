/*
 * Project Ambrose by Imjustchico
 * The pixel check that defends the look, which a spacing step or a border colour drifting would otherwise pass: run and re-baselined only inside the official container, never on the blocking path.
 */

import { expect, test } from "@playwright/test";

test.describe("the panel", () => {
    test("looks the way it is drawn, in the dark theme", async ({ page }) => {
        await page.goto("/");
        await page.evaluate(() => document.documentElement.setAttribute("data-theme", "dark"));
        await expect(page).toHaveScreenshot("panel-dark.png", { fullPage: true, animations: "disabled" });
    });

    test("looks the way it is drawn, in the light theme", async ({ page }) => {
        await page.goto("/");
        await page.evaluate(() => document.documentElement.setAttribute("data-theme", "light"));
        await expect(page).toHaveScreenshot("panel-light.png", { fullPage: true, animations: "disabled" });
    });
});
