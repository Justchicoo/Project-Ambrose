/*
 * Project Ambrose by Imjustchico
 * The pixel check that defends the look, which a spacing step or a border colour drifting would otherwise pass: the sign-in page a real app's admin API serves, in both themes, which holds no live figure that changes between runs, run and re-baselined only inside the official container and never on the blocking path.
 */

import { expect, test } from "@playwright/test";
import { app, built, startPanel, type Panel } from "../e2e/panel-server";

test.skip(!app || !built, "needs the built panel and a built patchserver, or AMBROSE_PANEL_APP");

let panel: Panel;

test.beforeAll(async () => {
    panel = await startPanel(12620);
});

test.afterAll(async () => {
    await panel?.stop();
});

test.describe("the panel", () => {
    for (const theme of ["dark", "light"] as const) {
        test(`looks the way it is drawn, in the ${theme} theme`, async ({ page }) => {
            await page.addInitScript((chosen) => localStorage.setItem("ambrose.panel.theme", chosen), theme);
            await page.goto(`${panel.url}/`);
            await expect(page.getByRole("heading", { name: "Sign in to Ambrose" })).toBeVisible();
            await expect(page).toHaveScreenshot(`panel-${theme}.png`, { fullPage: true, animations: "disabled" });
        });
    }
});
