/*
 * Project Ambrose by Imjustchico
 * The two runs that stay off the blocking path, each starting a real app whose admin API serves the built panel: the end-to-end project, and the screenshots, which are taken only in the official container so a font or a driver cannot move a baseline.
 */

import { defineConfig, devices } from "@playwright/test";

export default defineConfig({
    testDir: "tests",
    fullyParallel: true,
    forbidOnly: !!process.env.CI,
    retries: process.env.CI ? 1 : 0,
    reporter: [["list"]],
    use: {
        trace: "on-first-retry",
    },
    projects: [
        {
            name: "e2e",
            testDir: "tests/e2e",
            use: { ...devices["Desktop Chrome"] },
        },
        {
            name: "screenshots",
            testDir: "tests/screenshots",
            use: { ...devices["Desktop Chrome"] },
            snapshotPathTemplate: "tests/screenshots/baselines/{projectName}-{platform}/{testFilePath}/{arg}{ext}",
        },
    ],
});
